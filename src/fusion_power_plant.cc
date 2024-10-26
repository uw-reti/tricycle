#include "fusion_power_plant.h"

using cyclus::Material;
using cyclus::Composition;
using cyclus::IntDistribution;
using cyclus::DoubleDistribution;
using cyclus::FixedIntDist;
using cyclus::FixedDoubleDist;
using cyclus::KeyError;



namespace tricycle {

const double FusionPowerPlant::burn_rate = 55.8;

const double MW_to_GW = 1000;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FusionPowerPlant::FusionPowerPlant(cyclus::Context* ctx) : cyclus::Facility(ctx) {
  fuel_tracker.Init({&tritium_storage}, fuel_limit);
  blanket_tracker.Init({&blanket_feed}, blanket_limit);

  tritium_storage = cyclus::toolkit::ResBuf<cyclus::Material>(true);
  tritium_excess = cyclus::toolkit::ResBuf<cyclus::Material>(true);
  helium_excess = cyclus::toolkit::ResBuf<cyclus::Material>(true);
  blanket_feed = cyclus::toolkit::ResBuf<cyclus::Material>(true);
  blanket_waste = cyclus::toolkit::ResBuf<cyclus::Material>(true);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string FusionPowerPlant::str() {
  return Facility::str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::EnterNotify() {
  cyclus::Facility::EnterNotify();

  fuel_usage_mass = (burn_rate * (fusion_power / MW_to_GW) / 
    (kDefaultTimeStepDur * 12) * context()->dt());
  //fuel_usage_atoms = fuel_usage_mass / tritium_atomic_mass;
  blanket_turnover = blanket_size * blanket_turnover_fraction;

  //Create the blanket material for use in the core, no idea if this works...
  blanket = Material::Create(this, 0.0, 
      context()->GetRecipe(blanket_inrecipe));

  fuel_startup_policy
      .Init(this, &tritium_storage, std::string("Tritium Storage"),
            &fuel_tracker)
      .Set(fuel_incommod, tritium_comp)
      .Start();

  blanket_fill_policy
      .Init(this, &blanket_feed, std::string("Blanket Startup"), 
            &blanket_tracker)
      .Set(blanket_incommod)
      .Start();

  // Tritium Buy Policy Selection:
  if (refuel_mode == "schedule") {
    IntDistribution::Ptr active_dist = 
      FixedIntDist::Ptr(new FixedIntDist(1));
    IntDistribution::Ptr dormant_dist = 
      FixedIntDist::Ptr(new FixedIntDist(buy_frequency - 1));
    DoubleDistribution::Ptr size_dist = 
      FixedDoubleDist::Ptr(new FixedDoubleDist(1));

    fuel_refill_policy
        .Init(this, &tritium_storage, std::string("Input"), 
              &fuel_tracker,
              buy_quantity, 
              active_dist, dormant_dist, size_dist)
        .Set(fuel_incommod, tritium_comp);

  } else if (refuel_mode == "fill") {
    fuel_refill_policy
        .Init(this, &tritium_storage, std::string("Input"), 
              &fuel_tracker).Set(fuel_incommod, tritium_comp);

  } else {
    throw KeyError("Refill mode " + refuel_mode + 
                    " not recognized! Try 'schedule' or 'fill'.");
  }

  tritium_sell_policy
      .Init(this, &tritium_excess, std::string("Excess Tritium"))
      .Set(fuel_incommod)
      .Start();

  helium_sell_policy.Init(this, &helium_excess, std::string("Helium-3"))
      .Set(he3_outcommod)
      .Start();

  //This is going to need some work... The blanket waste recipe is going to be
  //different all the time, so we want to just peg it to "depleted lithium" or
  //something... Come back to this when you have a second.
  blanket_waste_sell_policy
      .Init(this, &blanket_waste, std::string("Blanket Waste"))
      .Set(blanket_outcommod)
      .Start();

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::Tick() {
  //pseudocode implementation of Tick():
  
  if (ReadyToOperate()) {
    fuel_startup_policy.Stop();
    fuel_refill_policy.Start();
    LoadCore();
    OperateReactor();
  } else {
    //Some way of leaving a record of what is going wrong is helpful info I think
    //Record(Error);
  }

  DecayInventories();
  ExtractHelium();
  
  double excess_tritium = std::max(tritium_storage.quantity() - 
                                   (reserve_inventory+ SequesteredTritiumGap()), 0.0);
  
  tritium_excess.Push(tritium_storage.Pop(excess_tritium));

  if (sequestered_tritium->quantity() != 0) {
    fuel_startup_policy.Stop();
    fuel_refill_policy.Start();
  }

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::Tock() {
  //This is where we used to squash tritium_storage and blanket, but that's no
  //longer needed. Leaving a comment to remind myself about that.

  //Again, not sure about the recording:
  //RecordInventories(all_of_them.quantity());
  
}

double FusionPowerPlant::SequesteredTritiumGap() {
  
  double current_sequestered_tritium = 0.0;

  if (sequestered_tritium->quantity() > cyclus::eps_rsrc()) {
    cyclus::toolkit::MatQuery mq(sequestered_tritium);
    current_sequestered_tritium = mq.mass(tritium_id);
  }

  return std::max(sequestered_equilibrium - current_sequestered_tritium, 0.0);
}


bool FusionPowerPlant::TritiumStorageClean() {

  cyclus::toolkit::MatQuery mq(tritium_storage.Peek());
  return cyclus::AlmostEq(mq.mass(tritium_id), tritium_storage.quantity());

}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool FusionPowerPlant::ReadyToOperate() {

  // determine required tritium storage inventory
  double required_storage_inventory = reserve_inventory + SequesteredTritiumGap();

  // check tritium storage quantity
  if (tritium_storage.quantity() < required_storage_inventory || !TritiumStorageClean()) {
        return false;
  }
  if (BlanketCycleTime() && blanket_feed.quantity() < blanket_turnover) {
    return false;
  }
      
  return true;

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::LoadCore() {
  CycleBlanket();
  sequestered_tritium->Absorb(tritium_storage.Pop(SequesteredTritiumGap()));
  incore_fuel->Absorb(tritium_storage.Pop(fuel_usage_mass));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::BreedTritium(double T_burned) {
  int Li6_id = pyne::nucname::id("Li-6");
  int Li7_id = pyne::nucname::id("Li-7");
  int He4_id = pyne::nucname::id("He-4");
  
  double Li6_molar_mass = pyne::atomic_mass(Li6_id);
  double Li7_molar_mass = pyne::atomic_mass(Li7_id);
  double T_molar_mass = pyne::atomic_mass(tritium_id);
  double He4_molar_mass = pyne::atomic_mass(He4_id);

  cyclus::Composition::Ptr Li6 = cyclus::Composition::CreateFromAtom(cyclus::CompMap({{Li6_id, 1.0}}));
  cyclus::Composition::Ptr Li7 = cyclus::Composition::CreateFromAtom(cyclus::CompMap({{Li7_id, 1.0}}));
  cyclus::Composition::Ptr Tritium = cyclus::Composition::CreateFromAtom(cyclus::CompMap({{tritium_id, 1.0}}));
  cyclus::Composition::Ptr He4 = cyclus::Composition::CreateFromAtom(cyclus::CompMap({{He4_id, 1.0}}));

  // Breed tritium
  cyclus::Material::Ptr T_created = cyclus::Material::Create(this, T_burned * TBR, Tritium);
  double T_created_atoms = T_created->quantity() * T_molar_mass;
  cyclus::Material::Ptr Li7_burned = cyclus::Material::CreateUntracked(T_created_atoms * Li7_contribution/Li7_molar_mass, Li7);
  cyclus::Material::Ptr Li6_burned = cyclus::Material::CreateUntracked(T_created_atoms * (1-Li7_contribution) / Li6_molar_mass, Li6);
  cyclus::Material::Ptr He4_generated = cyclus::Material::CreateUntracked(T_created_atoms/He4_molar_mass, He4);
  
  cyclus::Material::Ptr consumed_Li = blanket->ExtractComp(Li7_burned->quantity(), Li7);
  consumed_Li->Absorb(blanket->ExtractComp(Li6_burned->quantity(), Li6));
  blanket->Absorb(He4_generated);

  tritium_storage.Push(T_created);
}

void FusionPowerPlant::OperateReactor() {

  cyclus::Material::Ptr consumed_fuel = incore_fuel->ExtractQty(fuel_usage_mass);
  BreedTritium(fuel_usage_mass);

}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::DecayInventories() {
  //Left empty to quickly check if code builds
  //tritium_storage.Decay();
  //tritium_excess.Decay();
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::ExtractHelium() {

  int He3_id = pyne::nucname::id("He-3");
  cyclus::Composition::Ptr He3 = cyclus::Composition::CreateFromAtom(cyclus::CompMap({{He3_id, 1.0}}));

  std::vector<cyclus::toolkit::ResBuf<Material>> tritium_buffers = {tritium_storage, tritium_excess};

  for (auto inventory : tritium_buffers) {
    if (!inventory.empty()) {
      cyclus::Material::Ptr mat = inventory.Pop();
      cyclus::toolkit::MatQuery mq(mat);
      
      cyclus::Material::Ptr helium = mat->ExtractComp(mq.mass(He3_id), He3);

      helium_excess.Push(helium);
      inventory.Push(mat);
    }
  }

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::CycleBlanket() {
  
  if (blanket->quantity() < cyclus::eps_rsrc()) {
    blanket->Absorb(blanket_feed.Pop(blanket_size));
  } else if (BlanketCycleTime()) {

    blanket_waste.Push(blanket->ExtractQty(blanket_turnover));
    blanket->Absorb(blanket_feed.Pop(blanket_turnover));

  }
}

bool FusionPowerPlant::BlanketCycleTime(){
  return ((context()->time() > 0) &&
          (context()->time() % blanket_turnover_frequency == 0) );
}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructFusionPowerPlant(cyclus::Context* ctx) {
  return new FusionPowerPlant(ctx);
}

}  // namespace tricycle
