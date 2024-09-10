#include "fusion_power_plant.h"

namespace tricycle {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FusionPowerPlant::FusionPowerPlant(cyclus::Context* ctx) : cyclus::Facility(ctx) {}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string FusionPowerPlant::str() {
  return Facility::str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::EnterNotify() {
  //Pseudocode implementation of EnterNotify()
  cyclus::Facility::EnterNotify();
  fuel_usage_mass = (burn_rate * (fusion_power / MW_to_GW) / seconds_per_year * context()->dt());
  fuel_usage_atoms = fuel_usage_mass / tritium_atomic_mass;
  blanket_turnover = blanket_size * blanket_turnover_rate;

  //Truncated for readability. It goes on Tritium Storage and gets started, though.
  fuel_startup_policy.Init(&tritium_storage).Set(fuel_incommod).Start();
  
  //Truncated for readability
  blanket_fill_policy.Init(&blanket).Set(blanket_incommod).Start();

  //Tritium Buy Policy Section:
  if (refuel_mode == "schedule") {
    IntDistribution::Ptr active_dist =
        FixedIntDist::Ptr(new FixedIntDist(1));
    IntDistribution::Ptr dormant_dist =
        FixedIntDist::Ptr(new FixedIntDist(buy_frequency - 1));
    DoubleDistribution::Ptr size_dist =
        FixedDoubleDist::Ptr(new FixedDoubleDist(1));
    //Do not start the policy yet.
    fuel_refill_policy
        .Init(&tritium_storage, &fuel_tracker,
              buy_quantity, active_dist, dormant_dist, size_dis)
        .Set(fuel_incommod);
  } else if (refuel_mode == "fill") {
    //otherwise we just do it normally, but still don't start it.
    fuel_refill_policy.Init(&tritium_storage).Set(fuel_incommod);
  } else {
    //This can be more sophisticated later.
    throw cyclus::KeyError("Refill mode not recognized!");
  }

  tritium_sell_policy.Init(&tritium_excess).Set(fuel_incommod).Start();
  helium_sell_policy.Init(&helium_storage).Set(he3).Start();

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
    Record(Error);
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
  RecordInventories(all_of_them.quantity());
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::CycleBlanket() {
  if (BlanketCycleTime()) {
    if (blanket->quantity() >= blanket_turnover) {
      blanket_waste.Push(blanket->ExtractQty(blanket_turnover));

      //guarantee blanket has enough material in CheckOperatingConditions()
      blanket->Absorb(blanket_feed.Pop(blanket_turnover));

      RecordOperationalInfo("Blanket Cycled");
    } else {
      RecordOperationalInfo("Blanket Not Cycled");
    }
  }
}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructFusionPowerPlant(cyclus::Context* ctx) {
  return new FusionPowerPlant(ctx);
}

}  // namespace tricycle
