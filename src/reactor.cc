#include "reactor.h"

#include "boost/shared_ptr.hpp"

namespace tricycle {

Reactor::Reactor(cyclus::Context* ctx) : cyclus::Facility(ctx) {
  // capacities set somewhat arbitrarily
  fuel_tracker.Init({&tritium_reserve}, 1000.0);
  blanket_tracker.Init({&blanket}, 100000.0);
  excess_tritium_tracker.Init({&tritium_storage}, 1000.0);
  helium_tracker.Init({&helium_storage}, 1000.0);
}

std::string Reactor::str() {
  return Facility::str();
}

void Reactor::Tick() {
  if (core_loaded) {
    OperateReactor(TBR);
    blanket_fill_policy.Start();
    if (operational) {
      Reactor::RecordStatus("Online", fusion_power);
    } else {
      Reactor::RecordStatus("Shut-down", 0);
    }
  } else {
    Reactor::RecordStatus("Shut-down", 0);
  }

  DecayInventory(tritium_core);
  DecayInventory(tritium_reserve);
  DecayInventory(tritium_storage);

  ExtractHelium(tritium_core);
  ExtractHelium(tritium_reserve);
  ExtractHelium(tritium_storage);

  // Pull excesss tritium out of reserve and put it into storage
  // Replenish core from Helium Extraction.
  if (!tritium_reserve.empty() && core_loaded) {
    double core_deficit = startup_inventory - tritium_core.quantity();

    // Excess tritium will always be in tritium_reserve.
    double surplus = std::max(
        tritium_reserve.quantity() - reserve_inventory - core_deficit, 0.0);

    cyclus::Material::Ptr reserve_fuel = tritium_reserve.Pop();
    cyclus::Material::Ptr core_fuel = tritium_core.Pop();

    core_fuel->Absorb(reserve_fuel->ExtractQty(core_deficit));
    tritium_storage.Push(reserve_fuel->ExtractQty(surplus));

    RecordOperationalInfo(
        "Tritium Moved",
        std::to_string(core_deficit) + "kg of T moved from reserve to core");
    if (surplus > 0.0) {
      RecordOperationalInfo(
          "Tritium Moved",
          std::to_string(surplus) + "kg of T moved from reserve to storage");
    }
    tritium_reserve.Push(reserve_fuel);
    tritium_core.Push(core_fuel);

    CombineInventory(tritium_storage);
  }

  // This pulls out some of the blanket each timestep so that fresh blanket can
  // be added.
  if (!blanket.empty() &&
      blanket.quantity() >= blanket_size * blanket_turnover_rate) {
    cyclus::Material::Ptr blanket_mat = blanket.Pop();
    cyclus::Material::Ptr spent_blanket =
        blanket_mat->ExtractQty(blanket_size * blanket_turnover_rate);
    RecordOperationalInfo(
        "Blanket Cycled",
        std::to_string(spent_blanket->quantity()) + "kg of blanket removed");
    blanket.Push(blanket_mat);
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
  if (!core_loaded) {
    try {
      Startup();
      fuel_startup_policy.Stop();
      fuel_refill_policy.Start();
      core_loaded = true;
    } catch (const std::exception& e) {
      RecordOperationalInfo("Startup Error", e.what());
      LOG(cyclus::LEV_INFO2, "Reactor") << e.what();
    }
  }

  CombineInventory(tritium_reserve);
  CombineInventory(blanket);

  RecordInventories(tritium_core.quantity(), tritium_reserve.quantity(),
                    tritium_storage.quantity(), blanket.quantity(),
                    helium_storage.quantity());
}

void Reactor::EnterNotify() {
  cyclus::Facility::EnterNotify();

  fuel_startup_policy
      .Init(this, &tritium_reserve, std::string("Tritium Storage"),
            &fuel_tracker, std::string("ss"),
            reserve_inventory + startup_inventory,
            reserve_inventory + startup_inventory)
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
        .Init(this, &tritium_reserve, std::string("Input"), &fuel_tracker,
              buy_quantity, active_dist, dormant_dist, size_dist)
        .Set(fuel_incommod);
  } else if (refuel_mode == "fill") {
    fuel_refill_policy
        .Init(this, &tritium_reserve, std::string("Input"), &fuel_tracker,
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
      .Init(this, &tritium_storage, std::string("Excess Tritium"))
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
  double reserve_qty = tritium_reserve.quantity();
  cyclus::Material::Ptr initial_reserve = tritium_reserve.Pop();
  cyclus::CompMap c = initial_reserve->comp()->atom();
  cyclus::compmath::Normalize(&c, 1);

  if ((reserve_qty >= (startup_inventory + reserve_inventory)) &&
      (GetComp(initial_reserve) == "{{10030000,1.000000}}")) {
    cyclus::Material::Ptr initial_core =
        initial_reserve->ExtractQty(startup_inventory);
    tritium_core.Push(initial_core);
    tritium_reserve.Push(initial_reserve);
    RecordEvent("Startup", "Core Loaded with " +
                               std::to_string(tritium_core.quantity()) +
                               " kg of Tritium.");
  } else if (GetComp(initial_reserve) != "{{10030000,1.000000}}") {
    tritium_reserve.Push(initial_reserve);
    throw cyclus::ValueError(
        "Startup Failed: Fuel incommod not as expected. " +
        std::string("Expected Composition: {{10030000,1.000000}}. ") +
        std::string("Fuel Incommod Composition: ") +
        std::string(GetComp(initial_reserve)));
  } else {
    tritium_reserve.Push(initial_reserve);
    throw cyclus::ValueError(
        "Startup Failed: " + std::to_string(tritium_reserve.quantity()) +
        " kg in reserve is less than required " +
        std::to_string(startup_inventory + reserve_inventory) +
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

void Reactor::RecordInventories(double core, double reserve, double storage,
                                double blanket, double helium) {
  context()
      ->NewDatum("ReactorInventories")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("TritiumCore", core)
      ->AddVal("TritiumReserve", reserve)
      ->AddVal("TritiumStorage", storage)
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

void Reactor::OperateReactor(double TBR, double burn_rate) {
  int seconds_per_year = 31536000;
  double fuel_usage =
      burn_rate * (fusion_power / 1000) / seconds_per_year * context()->dt();

  cyclus::Material::Ptr reserve_fuel = tritium_reserve.Pop();
  cyclus::Material::Ptr core_fuel = tritium_core.Pop();

  if (core_fuel->quantity() > fuel_usage) {
    cyclus::Material::Ptr used_fuel = core_fuel->ExtractQty(fuel_usage);
    core_fuel->Absorb(BreedTritium(fuel_usage, TBR));

    if (((reserve_fuel->quantity() + core_fuel->quantity()) >=
         startup_inventory)) {
      reserve_fuel->Absorb(core_fuel);
      tritium_core.Push(reserve_fuel->ExtractQty(startup_inventory));
      tritium_reserve.Push(reserve_fuel);

    } else {
      RecordOperationalInfo("Tritium Moved",
                            std::to_string(core_fuel->quantity()) +
                                "kg of T moved from core to reserve");
      reserve_fuel->Absorb(core_fuel);
      tritium_reserve.Push(reserve_fuel);
      fuel_refill_policy.Stop();
      blanket_fill_policy.Stop();
      fuel_startup_policy.Start();
      RecordEvent("Core Shut-down", "Not enough tritium to operate");
      core_loaded = false;
    }
  } else {
    operational = false;
    RecordOperationalInfo("Operational Error",
                          "core startup_inventory of " +
                              std::to_string(startup_inventory) +
                              " kg insufficient to support fuel_usage of " +
                              std::to_string(fuel_usage) + "kg/timestep!");
    tritium_reserve.Push(reserve_fuel);
    tritium_core.Push(core_fuel);
  }
}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructReactor(cyclus::Context* ctx) {
  return new Reactor(ctx);
}

}  // namespace tricycle
