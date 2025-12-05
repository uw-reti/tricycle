#include "decay_storage.h"

namespace decaystorage {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
DecayStorage::DecayStorage(cyclus::Context* ctx) : cyclus::Facility(ctx) {
  // Required by DRE policies
  fuel_tracker.Init({&tritium_storage}, 100000000);

  bool is_bulk = true;

  tritium_storage = cyclus::toolkit::ResBuf<cyclus::Material>(is_bulk);
  helium_storage = cyclus::toolkit::ResBuf<cyclus::Material>(is_bulk);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string DecayStorage::str() {
  return Facility::str();
}

void DecayStorage::EnterNotify() {
  cyclus::Facility::EnterNotify(); // call base function first
  buy_policy.Init(this, &tritium_storage, std::string("input"), &fuel_tracker).Set(incommod).Start();
  sell_policy.Init(this, &tritium_storage, std::string("output")).Set(outcommod).Start();
}

void DecayStorage::RecordInventories(double tritium, double helium) {
  context()
      ->NewDatum("StorageInventories")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("TritiumStorage", tritium)
      ->AddVal("HeliumStorage", helium)

      ->Record();
}

void DecayStorage::ExtractHelium(
    cyclus::toolkit::ResBuf<cyclus::Material>& inventory) {
  if (!inventory.empty()) {
    cyclus::Material::Ptr mat = inventory.Pop();
    cyclus::toolkit::MatQuery mq(mat);
    
    cyclus::Material::Ptr helium = mat->ExtractComp(mq.mass(He3_id), He3_comp);

    helium_storage.Push(helium);
    inventory.Push(mat);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void DecayStorage::Tick() {
  tritium_storage.Decay();
  ExtractHelium(tritium_storage);
  LOG(cyclus::LEV_INFO2, "Storage") << "Quantity to be offered: " << sell_policy.Limit() << " kg.";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void DecayStorage::Tock() {
  RecordInventories(tritium_storage.quantity(), helium_storage.quantity());
}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructDecayStorage(cyclus::Context* ctx) {
  return new DecayStorage(ctx);
}

}  // namespace decaystorage
