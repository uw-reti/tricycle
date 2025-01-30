#include "fusion_power_plant.h"

using cyclus::Material;
using cyclus::Composition;
using cyclus::CompMap;
using cyclus::IntDistribution;
using cyclus::DoubleDistribution;
using cyclus::FixedIntDist;
using cyclus::FixedDoubleDist;
using cyclus::KeyError;
using cyclus::toolkit::ResBuf;

namespace tricycle {

const double FusionPowerPlant::burn_rate = 55.8;
const double MW_to_GW = 1000;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FusionPowerPlant::FusionPowerPlant(cyclus::Context* ctx) : cyclus::Facility(ctx) {
  fuel_tracker.Init({&tritium_storage}, fuel_limit);
  blanket_tracker.Init({&blanket_feed}, blanket_limit);

  tritium_storage = ResBuf<Material>(true);
  tritium_excess = ResBuf<Material>(true);
  helium_excess = ResBuf<Material>(true);
  blanket_feed = ResBuf<Material>(true);
  blanket_waste = ResBuf<Material>(true);
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
  blanket_turnover = blanket_size * blanket_turnover_fraction; 
  
  //Create the blanket material for use in the core, no idea if this works...
  blanket = Material::Create(this, 0.0, 
    context()->GetRecipe(blanket_inrecipe));

  fuel_startup_policy
    .Init(this, &tritium_storage, std::string("Tritium Storage"),
          &fuel_tracker, std::string("ss"),
          reserve_inventory + sequestered_equilibrium,
          reserve_inventory + sequestered_equilibrium)
    .Set(fuel_incommod, tritium_comp)
    .Start();

  blanket_fill_policy
      .Init(this, &blanket_feed, std::string("Blanket Startup"), &blanket_tracker,
            std::string("ss"), blanket_size, blanket_size)
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
        .Init(this, &tritium_storage, std::string("Input"), &fuel_tracker,
              std::string("ss"), reserve_inventory, reserve_inventory)
        .Set(fuel_incommod, tritium_comp);

  } else {
    throw KeyError("Refuel mode " + refuel_mode + 
                    " not recognized! Try 'schedule' or 'fill'.");
  }

  tritium_sell_policy
      .Init(this, &tritium_excess, std::string("Excess Tritium"))
      .Set(fuel_incommod)
      .Start();

  helium_sell_policy.Init(this, &helium_excess, std::string("Helium-3"))
      .Set(he3_outcommod)
      .Start();

  blanket_waste_sell_policy
      .Init(this, &blanket_waste, std::string("Blanket Waste"))
      .Set(blanket_outcommod)
      .Start();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::Tick() {
  

  if (ReadyToOperate()) {
    fuel_startup_policy.Stop();
    fuel_refill_policy.Start();
    
    LoadCore();
    OperateReactor();
    
  } else {
    // Some way of leaving a record of what is going wrong is helpful info I think
    // Use the cyclus logger
  }
  
  DecayInventories();
  ExtractHelium();
  
  double excess_tritium = std::max(tritium_storage.quantity() - 
                                  (reserve_inventory + SequesteredTritiumGap())
                                  , 0.0);
  
  // Otherwise the ResBuf encounters an error when it tries to squash
  if (excess_tritium > cyclus::eps_rsrc()) {
    tritium_excess.Push(tritium_storage.Pop(excess_tritium));
  }

  if (sequestered_tritium->quantity() != 0) {
    fuel_startup_policy.Stop();
    fuel_refill_policy.Start();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::Tock() {
  
  // ExplicitInventories wasn't working. If possible, may be best to use that
  // down the road.
  RecordInventories(tritium_storage.quantity(), tritium_excess.quantity(), 
                    sequestered_tritium->quantity(), blanket_feed.quantity(),
                    blanket_waste.quantity(), helium_excess.quantity());
}

void FusionPowerPlant::RecordInventories(double tritium_storage, 
                                         double tritium_excess, 
                                         double sequestered_tritium, 
                                         double blanket_feed, 
                                         double blanket_waste, 
                                         double helium_excess) {
  context()
      ->NewDatum("FPPInventories")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("TritiumStorage", tritium_storage)
      ->AddVal("TritiumExcess", tritium_excess)
      ->AddVal("TritiumSequestered", sequestered_tritium)
      ->AddVal("BlanketFeed", blanket_feed)
      ->AddVal("BlanketWaste", blanket_waste)
      ->AddVal("HeliumExcess", helium_excess)
      ->Record();
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
  
  // Determine tritium inventory required to operate
  double required_storage_inventory = SequesteredTritiumGap();
  if (sequestered_tritium->quantity() < cyclus::eps_rsrc()) {
    required_storage_inventory += reserve_inventory;
  } else {
    required_storage_inventory += fuel_usage_mass;
  }

  // check  tritium storage quantity requirement
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
  
  // Force decay of storage_inventory to avoid it resetting dt to 0
  tritium_storage.Decay();
  ExtractHelium();

  CycleBlanket();

  // Squash runs into issues when you give it zero, so we need to check frist
  if (SequesteredTritiumGap() > cyclus::eps_rsrc()) { 
    sequestered_tritium->Absorb(tritium_storage.Pop(SequesteredTritiumGap()));
  }
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

  Composition::Ptr Li6 = Composition::CreateFromAtom(CompMap({{Li6_id, 1.0}}));
  Composition::Ptr Li7 = Composition::CreateFromAtom(CompMap({{Li7_id, 1.0}}));
  Composition::Ptr Tritium = Composition::CreateFromAtom(CompMap({{tritium_id, 1.0}}));
  Composition::Ptr He4 = Composition::CreateFromAtom(CompMap({{He4_id, 1.0}}));

  // Breed tritium
  Material::Ptr T_created = Material::Create(this, T_burned * TBR, Tritium);
  double T_created_atoms = T_created->quantity() * T_molar_mass;
  Material::Ptr Li7_burned = Material::CreateUntracked(T_created_atoms * Li7_contribution/Li7_molar_mass, Li7);
  Material::Ptr Li6_burned = Material::CreateUntracked(T_created_atoms * (1-Li7_contribution) / Li6_molar_mass, Li6);
  Material::Ptr He4_generated = Material::CreateUntracked(T_created_atoms/He4_molar_mass, He4);
  
  Material::Ptr consumed_Li = blanket->ExtractComp(Li7_burned->quantity(), Li7);
  consumed_Li->Absorb(blanket->ExtractComp(Li6_burned->quantity(), Li6));
  blanket->Absorb(He4_generated);

  tritium_storage.Push(T_created);
}

void FusionPowerPlant::OperateReactor() {

  Material::Ptr consumed_fuel = incore_fuel->ExtractQty(fuel_usage_mass);
  BreedTritium(fuel_usage_mass);

}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::DecayInventories() {
  tritium_storage.Decay();
  tritium_excess.Decay();
  sequestered_tritium->Decay(context()->time());
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::ExtractHelium() {

  int He3_id = pyne::nucname::id("He-3");
  Composition::Ptr He3 = Composition::CreateFromAtom(CompMap({{He3_id, 1.0}}));

  std::vector<ResBuf<Material>*> tritium_buffers = {&tritium_storage, &tritium_excess};

  for (auto* inventory : tritium_buffers) {
    if (!inventory->empty()) {
      Material::Ptr mat = inventory->Pop();
      cyclus::toolkit::MatQuery mq(mat);
      
      Material::Ptr helium = mat->ExtractComp(mq.mass(He3_id), He3);

      helium_excess.Push(helium);
      inventory->Push(mat);
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

