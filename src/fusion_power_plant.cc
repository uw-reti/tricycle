#include "fusion_power_plant.h"

using cyclus::Material;
using cyclus::Composition;
using cyclus::IntDistribution;
using cyclus::DoubleDistribution;
using cyclus::FixedIntDist;
using cyclus::FixedDoubleDist;
using cyclus::KeyError;



namespace tricycle {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FusionPowerPlant::FusionPowerPlant(cyclus::Context* ctx) : cyclus::Facility(ctx) {
  fuel_tracker.Init({&tritium_storage}, fuel_limit);
  blanket_tracker.Init({&blanket_feed}, blanket_limit);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string FusionPowerPlant::str() {
  return Facility::str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::EnterNotify() {
  cyclus::Facility::EnterNotify();

  //fuel_usage_mass = (burn_rate * (fusion_power / MW_to_GW) / 
  //  seconds_per_year * context()->dt());
  //fuel_usage_atoms = fuel_usage_mass / tritium_atomic_mass;
  blanket_turnover = blanket_size * blanket_turnover_fraction;

  //Create the blanket material for use in the core, no idea if this works...
  blanket = Material::Create(this, 0.0, 
      context()->GetRecipe(blanket_inrecipe));

  fuel_startup_policy
      .Init(this, &tritium_storage, std::string("Tritium Storage"),
            &fuel_tracker)
      .Set(fuel_incommod)
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
        .Set(fuel_incommod);

  } else if (refuel_mode == "fill") {
    fuel_refill_policy
        .Init(this, &tritium_storage, std::string("Input"), 
              &fuel_tracker).Set(fuel_incommod);

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
  
  if (CheckOperatingConditions()) {
    fuel_startup_policy.Stop();
    fuel_refill_policy.Start();
    SequesterTritium();
    OperateReactor();
    CycleBlanket();

  } else {
    //Some way of leaving a record of what is going wrong is helpful info I think
    //Record(Error);
  }

  DecayInventories();
  ExtractHelium();
  MoveExcessTritiumToSellBuffer();

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::Tock() {
  //This is where we used to squash tritium_storage and blanket, but that's no
  //longer needed. Leaving a comment to remind myself about that.

  //Again, not sure about the recording:
  //RecordInventories(all_of_them.quantity());
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool FusionPowerPlant::CheckOperatingConditions() {
  //Left empty to quickly check if code builds
  return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::SequesterTritium() {
  //Left empty to quickly check if code builds
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::OperateReactor() {
  //Left empty to quickly check if code builds
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::DecayInventories() {
  //Left empty to quickly check if code builds
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::ExtractHelium() {
  //Left empty to quickly check if code builds
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::MoveExcessTritiumToSellBuffer() {
  //Left empty to quickly check if code builds
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::CycleBlanket() {
  if (BlanketCycleTime()) {
    if (blanket->quantity() >= blanket_turnover) {
      blanket_waste.Push(blanket->ExtractQty(blanket_turnover));

      //guarantee blanket has enough material in CheckOperatingConditions()
      blanket->Absorb(blanket_feed.Pop(blanket_turnover));

    }
  }
}

bool FusionPowerPlant::BlanketCycleTime(){
  return (context()->time() % blanket_turnover_frequency == 0 
          && !blanket_feed.empty());
}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructFusionPowerPlant(cyclus::Context* ctx) {
  return new FusionPowerPlant(ctx);
}

}  // namespace tricycle
