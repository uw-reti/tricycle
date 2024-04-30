#include "reactor.h"

#include "boost/shared_ptr.hpp"

namespace tricycle {

Reactor::Reactor(cyclus::Context* ctx) : cyclus::Facility(ctx) {
  fuel_tracker.Init({&tritium_storage}, fuel_limit);
  blanket_tracker.Init({&blanket}, blanket_limit);
}

void Reactor::Tick() {
  if (sufficient_tritium_for_operation) {
    SequesterTritium();
    OperateReactor(TBR);
    blanket_fill_policy.Start();
    Reactor::RecordStatus("Online", fusion_power);
  } else {
    Reactor::RecordStatus("Shut-down", 0);
  }

  DecayInventory(tritium_storage);
  DecayInventory(tritium_excess);
  sequestered_tritium->Decay(context()->time());

  ExtractHelium(tritium_storage);
  ExtractHelium(tritium_excess);

  if (!tritium_storage.empty() && sufficient_tritium_for_operation) {
    double surplus = std::max(
      tritium_storage.quantity() - reserve_inventory, 0.0);

    if (surplus > 0.0) {

      tritium_excess.Push(tritium_storage.Pop(surplus));
      CombineInventory(tritium_excess);

      RecordOperationalInfo(
          "Tritium Moved",
          std::to_string(surplus) + "kg of T moved from storage to excess");
    }
  }

  if (context()->time() % blanket_turnover_frequency == 0 && !blanket.empty()) {
    if (blanket.quantity() >= blanket_turnover) {
      blanket_excess.Push(blanket.Pop(blanket_turnover));
      CombineInventory(blanket_excess);
      RecordOperationalInfo(
          "Blanket Cycled",
          std::to_string(blanket_turnover) + "kg of blanket removed");
    } else {
      RecordOperationalInfo(
          "Blanket Not Cycled",
          "Total blanket material (" + std::to_string(blanket.quantity()) +
              ") insufficient to extract " +
              std::to_string(blanket_turnover) + "kg!");
    }
  }
}

void Reactor::Tock() {
  if (!sufficient_tritium_for_operation) {
    try {
      Startup();
      fuel_startup_policy.Stop();
      fuel_refill_policy.Start();
    } catch (const std::exception& e) {
      RecordOperationalInfo("Startup Error", e.what());
      LOG(cyclus::LEV_INFO2, "Reactor") << e.what();
    }
  }

  CombineInventory(tritium_storage);
  CombineInventory(blanket);

  RecordInventories(tritium_storage.quantity(), tritium_excess.quantity(), 
                    sequestered_tritium->quantity(), blanket.quantity(),
                    blanket_excess.quantity(), helium_storage.quantity());
}

void Reactor::EnterNotify() {
  cyclus::Facility::EnterNotify();

  fuel_usage_mass = (burn_rate * (fusion_power / MW_to_GW) / seconds_per_year * context()->dt());
  fuel_usage_atoms = fuel_usage_mass / tritium_atomic_mass;
  blanket_turnover = blanket_size * blanket_turnover_rate;

  fuel_startup_policy
      .Init(this, &tritium_storage, std::string("Tritium Storage"),
            &fuel_tracker, std::string("ss"),
            startup_inventory,
            startup_inventory)
      .Set(fuel_incommod)
      .Start();
  blanket_fill_policy
      .Init(this, &blanket, std::string("Blanket Startup"), &blanket_tracker,
            std::string("ss"), blanket_size, blanket_size)
      .Set(blanket_incommod)
      .Start();

  // Tritium Buy Policy Selection:
  if (refuel_mode == "schedule") {
    cyclus::IntDistribution::Ptr active_dist =
        cyclus::FixedIntDist::Ptr(new cyclus::FixedIntDist(1));
    cyclus::IntDistribution::Ptr dormant_dist =
        cyclus::FixedIntDist::Ptr(new cyclus::FixedIntDist(buy_frequency - 1));
    cyclus::DoubleDistribution::Ptr size_dist =
        cyclus::FixedDoubleDist::Ptr(new cyclus::FixedDoubleDist(1));
    fuel_refill_policy
        .Init(this, &tritium_storage, std::string("Input"), &fuel_tracker,
              buy_quantity, active_dist, dormant_dist, size_dist)
        .Set(fuel_incommod);
  } else if (refuel_mode == "fill") {
    fuel_refill_policy
        .Init(this, &tritium_storage, std::string("Input"), &fuel_tracker,
              std::string("ss"), reserve_inventory, reserve_inventory)
        .Set(fuel_incommod);
  } else {
    throw cyclus::KeyError("Refill mode " + refuel_mode +
                           " not recognized! Try 'schedule' or 'fill'.");
    RecordOperationalInfo("Transaction Error",
                          "Refill mode " + refuel_mode +
                              " not recognized! Try 'schedule' or 'fill'.");
  }

  tritium_sell_policy
      .Init(this, &tritium_excess, std::string("Excess Tritium"))
      .Set(fuel_incommod)
      .Start();
  helium_sell_policy.Init(this, &helium_storage, std::string("Helium-3"))
      .Set(he3_outcommod)
      .Start();
}

void Reactor::SequesterTritium(){
  if (sequestered_tritium->quantity() == 0.0){
    sequestered_tritium = tritium_storage.Pop(sequestered_equilibrium);
  } else {
    cyclus::toolkit::MatQuery mq(sequestered_tritium);
    double equilibrium_deficit = std::max(sequestered_equilibrium - 
                                          mq.mass(tritium_id), 0.0);
    sequestered_tritium->Absorb(tritium_storage.Pop(equilibrium_deficit));
  }
}

void Reactor::Startup() {
  cyclus::Material::Ptr initial_storage = tritium_storage.Peek();
  cyclus::CompMap c = initial_storage->comp()->atom();
  cyclus::compmath::Normalize(&c, 1);

  if (tritium_storage.quantity() < startup_inventory){
    throw cyclus::ValueError(
      "Startup Failed: " + std::to_string(tritium_storage.quantity()) +
      " kg in storage is less than required " +
      std::to_string(startup_inventory) +
      " kg to start-up!");
  } else if (startup_inventory < fuel_usage_mass) {
    throw cyclus::ValueError("Startup Failed: Startup Inventory insufficient "+ 
        std::string("to maintain reactor for full timestep!"));
  } else if (!cyclus::compmath::AlmostEq(c, T, 1e-7)) {
    throw cyclus::ValueError(
        "Startup Failed: Fuel incommod not as expected. ");
  } else {
    RecordEvent("Startup", "Sufficient tritium in system to begin operation");
    sufficient_tritium_for_operation = true;
  }
}

void Reactor::DecayInventory(
    cyclus::toolkit::ResBuf<cyclus::Material>& inventory) {
  if (!inventory.empty()) {
    cyclus::Material::Ptr mat = inventory.Pop();
    mat->Decay(context()->time());
    inventory.Push(mat);
  }
}

void Reactor::CombineInventory(
    cyclus::toolkit::ResBuf<cyclus::Material>& inventory) {
  if (!inventory.empty()) {
    cyclus::Material::Ptr base = inventory.Pop();
    int count = inventory.count();
    for (int i = 0; i < count; i++) {
      cyclus::Material::Ptr m = inventory.Pop();
      base->Absorb(m);
    }

    inventory.Push(base);
  }
}

void Reactor::ExtractHelium(
    cyclus::toolkit::ResBuf<cyclus::Material>& inventory) {
  if (!inventory.empty()) {
    cyclus::Material::Ptr mat = inventory.Pop();
    cyclus::toolkit::MatQuery mq(mat);
    
    cyclus::Material::Ptr helium = mat->ExtractComp(mq.mass(He3_id), He3_comp);

    helium_storage.Push(helium);
    inventory.Push(mat);
  }
}

void Reactor::RecordEvent(std::string name, std::string val) {
  context()
      ->NewDatum("ReactorEvents")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("Event", name)
      ->AddVal("Value", val)
      ->Record();
}

void Reactor::RecordOperationalInfo(std::string name, std::string val) {
  context()
      ->NewDatum("ReactorOperationsLog")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("Event", name)
      ->AddVal("Value", val)
      ->Record();
}

void Reactor::RecordStatus(std::string status, double power) {
  context()
      ->NewDatum("ReactorStatus")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("Status", status)
      ->AddVal("Power", power)
      ->Record();
}

void Reactor::RecordInventories(double storage, double excess, double sequestered, 
                            double blanket, double blanket_excess, double helium) {
  context()
      ->NewDatum("ReactorInventories")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("TritiumStorage", storage)
      ->AddVal("TritiumExcess", excess)
      ->AddVal("TritiumSequestered", sequestered)
      ->AddVal("LithiumBlanket", blanket)
      ->AddVal("BlanketExcess", blanket_excess)
      ->AddVal("HeliumStorage", helium)
      ->Record();
}

void Reactor::DepleteBlanket(double bred_tritium_moles) {
  cyclus::Material::Ptr blanket_mat = blanket.Pop();
  cyclus::toolkit::MatQuery b(blanket_mat);
  cyclus::CompMap depleted_comp;


  double bred_He4 = bred_tritium_moles;
  double blanket_tritium = b.moles(tritium_id);
  double blanket_He4 = b.moles(He4_id);
  double remaining_Li6 = b.moles(Li6_id) - Li6_contribution 
                          * bred_tritium_moles;
  double remaining_Li7 = b.moles(Li7_id) - Li7_contribution 
                          * bred_tritium_moles;

  // This is ALMOST the right behavior, not "scraping the bottom of the barrel
  if ((remaining_Li6 > 0) && (remaining_Li7 > 0)) {
    depleted_comp = {{Li7_id, remaining_Li7},
                     {Li6_id, remaining_Li6},
                     {tritium_id, blanket_tritium + bred_tritium_moles},
                     {He4_id, blanket_He4 + bred_He4}};

    cyclus::compmath::Normalize(&depleted_comp, 1);

    // There's a mass difference between T+He and Li, blanket will change mass
    double mass_difference = ((remaining_Li7) * Li7_atomic_mass
              + (remaining_Li6) * Li6_atomic_mass
              + (blanket_tritium + bred_tritium_moles) * tritium_atomic_mass
              + (blanket_He4 + bred_He4) * He4_atomic_mass)*avagadros_number
              -blanket_mat->quantity();

    // Account for the mass difference after depletion, then transmute to new comp
    if (mass_difference > 0){
      blanket_mat->Absorb(cyclus::Material::Create(this, mass_difference,
            cyclus::Composition::CreateFromAtom(blanket_mat->comp()->mass())));
      blanket_mat->Transmute(cyclus::Composition::CreateFromAtom(depleted_comp));
    } else if (mass_difference < 0){
      blanket_mat->ExtractQty(std::abs(mass_difference));
      blanket_mat->Transmute(cyclus::Composition::CreateFromAtom(depleted_comp));
    } else {
      blanket_mat->Transmute(cyclus::Composition::CreateFromAtom(depleted_comp));
    }
    
    RecordOperationalInfo("Blanket Depletion",
                          "Tritium bred at perscribed rate");
  } else {
    RecordOperationalInfo(
        "Breeding Error",
        "Blanket composition lacks sufficient lithium to continue "
        "breeding at perscribed rate");
  }
  blanket.Push(blanket_mat);
}

cyclus::Material::Ptr Reactor::BreedTritium(double atoms_burned, double TBR) {
  DepleteBlanket(atoms_burned/avagadros_number * TBR);
  cyclus::Material::Ptr mat = blanket.Pop();
  cyclus::toolkit::MatQuery mq(mat);
  cyclus::Material::Ptr bred_fuel = mat->ExtractComp(mq.mass(tritium_id), tritium_comp);
  blanket.Push(mat);

  RecordOperationalInfo("Bred Tritium", std::to_string(bred_fuel->quantity()) +
                                            " kg of T bred from blanket");

  return bred_fuel;
}

void Reactor::OperateReactor(double TBR) {

  cyclus::Material::Ptr fuel = tritium_storage.Pop();

  if (fuel->quantity() > fuel_usage_mass) {
    cyclus::Material::Ptr used_fuel = fuel->ExtractQty(fuel_usage_mass);
    fuel->Absorb(BreedTritium(fuel_usage_atoms, TBR));
    tritium_storage.Push(fuel);

  } else {
    fuel_refill_policy.Stop();
    blanket_fill_policy.Stop();
    fuel_startup_policy.Start();
    RecordEvent("Core Shut-down", "Not enough tritium to operate");
    sufficient_tritium_for_operation = false;

    tritium_storage.Push(fuel);
  }
}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructReactor(cyclus::Context* ctx) {
  return new Reactor(ctx);
}

}  // namespace tricycle
