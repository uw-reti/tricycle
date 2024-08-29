#include "fusion_power_plant.h"

namespace tricycle {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FusionPowerPlant::FusionPowerPlant(cyclus::Context* ctx) : cyclus::Facility(ctx) {}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string FusionPowerPlant::str() {
  return Facility::str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
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
    //Set up a schedule fill policy with Katie's active/dormant policy code
    //(not shown here). Do not start the policy yet.
    fuel_refill_policy.Init(&tritium_storage).Set(fuel_incommod);
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

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::Tick() {
  //pseudocode implementation of Tick():
  
  if (can_operate) {
    SequesterTritium();
    OperateReactor();
  }

  //I think somehow combining all these would be nice...
  DecayInventory(tritium_storage);
  ExtractHelium(tritium_storage);

  DecayInventory(tritium_excess);
  ExtractHelium(tritium_excess);

  sequestered_tritium->Decay(context()->time());

  //This was a bunch of free-floating code earlier, but i've truncated it 
  //because it's mostly for DRE stuff.
  MoveExcessTritiumToSellBuffer();

  //This maybe belongs in its own function?
  if (context()->time() % blanket_turnover_frequency == 0 && !blanket.empty()) {
    if (blanket.quantity() >= blanket_turnover) {
      blanket_excess.Push(blanket.Pop(blanket_turnover));
      CombineInventory(blanket_excess);
      RecordOperationalInfo("Blanket Cycled");
    } else {
      RecordOperationalInfo("Blanket Not Cycled");
    }
  }
  

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::Tock() {
  //pseudocode implementation of Tock():
  
  if(!can_operate) {
    try { 
      CheckOperatingConditions(); //Previously "Startup()"
      fuel_startup_policy.Stop();
      fuel_refill_policy.Start();
    } catch (const std::exception& e) {
      RecordError("Startup Error", e.what());
      LOG(cyclus::LEV_INFO2, "FusionPowerPlant") << e.what();
    }
  }

  //I think this behavior got added to cyclus earlier, but am not sure what the
  //overall functionality of it is at the moment. Now may be a good time to fold
  //that new behavior in, though, if it's functional. 
  CombineInventory(tritium_storage);
  CombineInventory(blanket);

  //Again, not sure about the recording:
  RecordInventories(all_of_them.quantity());
  
}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructFusionPowerPlant(cyclus::Context* ctx) {
  return new FusionPowerPlant(ctx);
}

}  // namespace tricycle
