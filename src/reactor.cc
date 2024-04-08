#include "reactor.h"

#include "boost/shared_ptr.hpp"

namespace tricycle {

Reactor::Reactor(cyclus::Context* ctx) : cyclus::Facility(ctx) {
  // capacities set somewhat arbitrarily
  fuel_tracker.Init({&tritium_storage}, 1000.0);
  blanket_tracker.Init({&blanket}, 100000.0);
}

std::string Reactor::str() {
  return Facility::str();
}

void Reactor::Tick() {
  if (sufficient_tritium_for_operation) {
    OperateReactor(TBR);
    blanket_fill_policy.Start();
    Reactor::RecordStatus("Online", fusion_power);
  } else {
    Reactor::RecordStatus("Shut-down", 0);
  }

  DecayInventory(tritium_storage);
  DecayInventory(tritium_excess);

  ExtractHelium(tritium_storage);
  ExtractHelium(tritium_excess);

  if (!tritium_storage.empty() && sufficient_tritium_for_operation) {
    double surplus = std::max(
      tritium_storage.quantity() - startup_inventory, 0.0);

    cyclus::Material::Ptr storage_fuel = tritium_storage.Pop();
    tritium_excess.Push(storage_fuel->ExtractQty(surplus));

    if (surplus > 0.0) {
      RecordOperationalInfo(
          "Tritium Moved",
          std::to_string(surplus) + "kg of T moved from storage to excess");
    }
    tritium_storage.Push(storage_fuel);
    CombineInventory(tritium_excess);
  }

  // This pulls out some of the blanket each timestep so that fresh blanket can
  // be added.
  if (!blanket.empty() &&
      blanket.quantity() >= blanket_size * blanket_turnover_rate &&
      context()->time() % blanket_turnover_frequency == 0) {
    cyclus::Material::Ptr spent_blanket =
        blanket.Pop(blanket_size * blanket_turnover_rate);
    RecordOperationalInfo(
        "Blanket Cycled",
        std::to_string(spent_blanket->quantity()) + "kg of blanket removed");
  } else if (!blanket.empty() &&
             blanket.quantity() < blanket_size * blanket_turnover_rate) {
    RecordOperationalInfo(
        "Blanket Not Cycled",
        "Total blanket material (" + std::to_string(blanket.quantity()) +
            ") insufficient to extract " +
            std::to_string(blanket_size * blanket_turnover_rate) + "kg!");
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

  RecordInventories(tritium_storage.quantity(), tritium_excess.quantity(), blanket.quantity(),
                    helium_storage.quantity());
}

void Reactor::EnterNotify() {
  cyclus::Facility::EnterNotify();

  fuel_usage = burn_rate * (fusion_power / MW_to_GW) / seconds_per_year * context()->dt();

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

std::string Reactor::GetComp(cyclus::Material::Ptr mat) {
  std::string comp = "{";
  cyclus::CompMap c = mat->comp()->atom();
  cyclus::compmath::Normalize(&c, 1);
  for (std::map<const int, double>::const_iterator it = c.begin();
       it != c.end(); ++it) {
    comp = comp + std::string("{") + std::to_string(it->first) +
           std::string(",") + std::to_string(it->second) + std::string("},");
  }
  comp.pop_back();
  comp = comp + std::string("}");
  return comp;
}

void Reactor::Startup() {
  cyclus::Material::Ptr initial_storage = tritium_storage.Peek();
  cyclus::CompMap c = initial_storage->comp()->atom();
  cyclus::compmath::Normalize(&c, 1);

  if ((tritium_storage.quantity() >= (startup_inventory)) &&
      (GetComp(initial_storage) == "{{10030000,1.000000}}") &&
      startup_inventory >= fuel_usage) {
    RecordEvent("Startup", "Sufficient tritium in system to begin operation");
    sufficient_tritium_for_operation = true;
  } else if (startup_inventory < fuel_usage){
    throw cyclus::ValueError("Startup Failed: Startup Inventory insufficient "+ 
        std::string("to maintain reactor for full timestep!"));
  } else if (GetComp(initial_storage) != "{{10030000,1.000000}}") {
    throw cyclus::ValueError(
        "Startup Failed: Fuel incommod not as expected. " +
        std::string("Expected Composition: {{10030000,1.000000}}. ") +
        std::string("Fuel Incommod Composition: ") +
        std::string(GetComp(initial_storage)));
  } else {
    throw cyclus::ValueError(
        "Startup Failed: " + std::to_string(tritium_storage.quantity()) +
        " kg in storage is less than required " +
        std::to_string(startup_inventory) +
        " kg to start-up!");
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
    cyclus::CompMap c = mat->comp()->atom();
    cyclus::compmath::Normalize(&c, mat->quantity());

    cyclus::CompMap He3 = {{20030000, 1}};

    // A threshold of 1e-5 was set to allow tritium_reserve inventories up to
    // 1000kg. A 1 decade lower threshold prevents tritium_reserve inventories
    // above 33kg.
    cyclus::Material::Ptr helium = mat->ExtractComp(
        c[20030000], cyclus::Composition::CreateFromAtom(He3), 1e-5);

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

void Reactor::RecordInventories(double storage, double excess, double blanket, double helium) {
  context()
      ->NewDatum("ReactorInventories")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("TritiumStorage", storage)
      ->AddVal("TritiumExcess", excess)
      ->AddVal("LithiumBlanket", blanket)
      ->AddVal("HeliumStorage", helium)
      ->Record();
}

void Reactor::DepleteBlanket(double bred_tritium_mass) {
  cyclus::Material::Ptr blanket_mat = blanket.Pop();

  cyclus::CompMap b = blanket_mat->comp()->mass();
  cyclus::compmath::Normalize(&b, blanket_mat->quantity());

  cyclus::CompMap depleted_comp;

  // This is ALMOST the correct behavior, but "scraping the bottom of the
  // barrel" is a little too complex for this implementation.
  if ((b[30060000] - (1 - Li7_contribution) * 2 * bred_tritium_mass > 0) &&
      (b[30070000] - Li7_contribution * 7.0 / 3.0 * bred_tritium_mass > 0)) {
    depleted_comp = {{30070000, b[30070000] - Li7_contribution * 7.0 / 3.0 *
                                                  bred_tritium_mass},
                     {30060000, b[30060000] - (1 - Li7_contribution) * 2 *
                                                  bred_tritium_mass},
                     {10030000, b[10030000] + bred_tritium_mass},
                     {20040000, b[20040000] + 4.0 / 3.0 * bred_tritium_mass}};

    // Account for the added mass of the absorbed neutrons
    double neutron_mass_correction =
        1.0 / 3.0 * bred_tritium_mass * (1 - Li7_contribution);
    cyclus::Material::Ptr additional_mass = cyclus::Material::Create(
        this, neutron_mass_correction,
        cyclus::Composition::CreateFromMass(depleted_comp));

    blanket_mat->Transmute(cyclus::Composition::CreateFromMass(depleted_comp));
    blanket_mat->Absorb(additional_mass);

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

cyclus::Material::Ptr Reactor::BreedTritium(double fuel_usage, double TBR) {
  DepleteBlanket(fuel_usage * TBR);
  cyclus::Material::Ptr mat = blanket.Pop();

  cyclus::CompMap c = mat->comp()->mass();
  cyclus::compmath::Normalize(&c, mat->quantity());

  cyclus::CompMap T = {{10030000, 1}};

  cyclus::Material::Ptr bred_fuel =
      mat->ExtractComp(c[10030000], cyclus::Composition::CreateFromAtom(T));
  blanket.Push(mat);

  RecordOperationalInfo("Bred Tritium", std::to_string(bred_fuel->quantity()) +
                                            " kg of T bred from blanket");

  return bred_fuel;
}

void Reactor::OperateReactor(double TBR) {

  cyclus::Material::Ptr fuel = tritium_storage.Pop();

  if (fuel->quantity() > fuel_usage) {
    cyclus::Material::Ptr used_fuel = fuel->ExtractQty(fuel_usage);
    fuel->Absorb(BreedTritium(fuel_usage, TBR));
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
