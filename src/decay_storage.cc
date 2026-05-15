// decay_storage.cc

#include "decay_storage.h"

namespace tricycle {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
DecayStorage::DecayStorage(cyclus::Context* ctx) : cyclus::Facility(ctx) {
  // Required by DRE policies
  fuel_tracker.Init({&tritium_storage}, cyclus::CY_LARGE_DOUBLE);

  bool is_bulk = true;

  tritium_storage = cyclus::toolkit::ResBuf<cyclus::Material>(is_bulk);
  helium_storage = cyclus::toolkit::ResBuf<cyclus::Material>(is_bulk);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void DecayStorage::EnterNotify() {
  cyclus::Facility::EnterNotify(); // call base function first
  fuel_tracker.set_capacity(max_tritium_inventory);
  buy_policy.Init(this, &tritium_storage, std::string("input"), &fuel_tracker, throughput).Set(incommod).Start();
  sell_policy.Init(this, &tritium_storage, std::string("output")).Set(outcommod).Start();
}

void DecayStorage::RecordInventories() {
  context()
      ->NewDatum("StorageInventories")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("TritiumStorage", tritium_storage.quantity())
      ->AddVal("HeliumStorage", helium_storage.quantity())
      ->Record();
}

void DecayStorage::ExtractHelium() {
  if (!tritium_storage.empty()) {
    cyclus::Material::Ptr mat = tritium_storage.Pop();
    cyclus::toolkit::MatQuery mq(mat);
    
    cyclus::Material::Ptr helium = mat->ExtractComp(mq.mass(He3_id), He3_comp);

    helium_storage.Push(helium);
    tritium_storage.Push(mat);
  };
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void DecayStorage::Tick() {
  tritium_storage.Decay();
  ExtractHelium();
  LOG(cyclus::LEV_INFO2, "Storage") << "Quantity to be offered: " << throughput << " kg.";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void DecayStorage::Tock() {
  RecordInventories();
}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructDecayStorage(cyclus::Context* ctx) {
  return new DecayStorage(ctx);
}

}  // namespace tricycle