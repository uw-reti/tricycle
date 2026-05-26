#include "flexible_fusion_plant.h"

using cyclus::CompMap;
using cyclus::Composition;
using cyclus::DoubleDistribution;
using cyclus::FixedDoubleDist;
using cyclus::FixedIntDist;
using cyclus::IntDistribution;
using cyclus::KeyError;
using cyclus::Material;
using cyclus::toolkit::ResBuf;

namespace tricycle {

const double MW_to_W = 1000000;
const double lambda_T = std::log(2.0) / (12.32 * 365 * 24 * 3600);
const double energy_DT = 17.6 * 1.6021766 * std::pow(10,-13);
const double mass_tritium = 5.01 * std::pow(10,-27);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FlexibleFusionPlant::FlexibleFusionPlant(cyclus::Context* ctx)
    : cyclus::Facility(ctx) {

  tritium_storage = ResBuf<Material>(true);
  tritium_excess = ResBuf<Material>(true);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string FlexibleFusionPlant::str() {
  return Facility::str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleFusionPlant::EnterNotify() {
  cyclus::Facility::EnterNotify();

  fuel_tracker.Init({&tritium_storage}, fuel_limit);
  
  burn_rate = mass_tritium * fusion_power * MW_to_W/ 
	  (conversion_efficiency * energy_DT);
  fuel_usage_mass = burn_rate * context()->dt();
  
  // Ensure startup inventory is greater than reserve  
  if (startup_inventory < reserve_inventory) {
    throw cyclus::ValueError(
        "Startup inventory must exceed or equal reserve inventory."
	);
  }

  int N = components.size();
  
  // Size vector of intermediate buffers
  tritium_elsewhere = std::vector<ResBuf<Material>>(N, ResBuf<Material>(true));

  // Build component lookup map
  for (int i = 0; i < N; ++i) {
    comp_index[components[i]] = i;
  }

  // Ensure that the components contain plasma and breeder
  std::vector<std::string> required = {
      "breeder",
      "plasma",
      "storage"
  };

  for (auto const& name : required) {
    if (comp_index.count(name) == 0) {
      throw cyclus::ValueError(
          "Required tritium component '" +
          name +
          "' was not defined in input.");
    }
  }

  // Ensure transfer vectors have the same length
  if (transfer_from.size() != transfer_to.size() ||
    transfer_from.size() != transfer_rate.size()) {

    throw cyclus::ValueError(
        "Transfer vectors must have equal length.");
  }
  
  // And escape fractions
  if (escape_to.size() != escape_fraction.size()) {

    throw cyclus::ValueError(
        "Escape vectors must have equal length.");
  }

  // Ensure tritium escape fraction sums to less than one
  if (escape_fraction.size() > 0) {
    double total = 0.0;

    for (int i = 0; i < escape_fraction.size(); i++) {

      double x = escape_fraction[i];

      // Check positivity
      if (x < 0.0) {
        throw cyclus::ValueError(
            "All escape fractions must be non-negative.");
      }

      total += x;
    }

    // Check total
    if (total > 1.0) {
      throw cyclus::ValueError(
        "Escape fractions must sum to <= 1.");
    }
  }

  // Create matrix
  A = Eigen::MatrixXd::Zero(N, N);

  // Build the matrix
  BuildMatrix(burn_rate);

  fuel_startup_policy
      .Init(this, &tritium_storage, std::string("Tritium Storage"),
            &fuel_tracker, std::string("ss"),
            startup_inventory, startup_inventory)
      .Set(fuel_incommod, tritium_comp)
      .Start();

  // Tritium Buy Policy Selection:
  if (refuel_mode == "schedule") {
    IntDistribution::Ptr active_dist = FixedIntDist::Ptr(new FixedIntDist(1));
    IntDistribution::Ptr dormant_dist =
        FixedIntDist::Ptr(new FixedIntDist(buy_frequency - 1));
    DoubleDistribution::Ptr size_dist =
        FixedDoubleDist::Ptr(new FixedDoubleDist(1));

    fuel_refill_policy
        .Init(this, &tritium_storage, std::string("Input"), &fuel_tracker,
              buy_quantity, active_dist, dormant_dist, size_dist)
        .Set(fuel_incommod, tritium_comp);

  } else if (refuel_mode == "fill") {
    double reserve = std::max(fuel_usage_mass, reserve_inventory);

    fuel_refill_policy
        .Init(this, &tritium_storage, std::string("Input"), &fuel_tracker,
              std::string("ss"), reserve, reserve)
        .Set(fuel_incommod, tritium_comp);

  } else {
    throw KeyError("Refuel mode " + refuel_mode +
                   " not recognized! Try 'schedule' or 'fill'.");
  }

  tritium_sell_policy.Init(this, &tritium_excess, std::string("Excess Tritium"))
      .Set(fuel_outcommod)
      .Start();
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleFusionPlant::BuildMatrix(double tritium_consumption_rate) {

  // Wipe any existing elements
  A.setZero();
      
  int plasma = comp_index["plasma"];

  // Fill transfer terms
  if (transfer_rate.size() > 0) {
    for (int k = 0; k < transfer_rate.size(); k++) {

      if (comp_index.count(transfer_from[k]) == 0) {
        throw cyclus::ValueError(
          "Unknown transfer source component: " +
          transfer_from[k]);
      }

      if (comp_index.count(transfer_to[k]) == 0) {
        throw cyclus::ValueError(
          "Unknown transfer destination component: " +
          transfer_to[k]);
      }
      
      int from = comp_index[transfer_from[k]];
      int to   = comp_index[transfer_to[k]];
      
      if (from == plasma || to == plasma) {
        throw cyclus::ValueError(
          "Cannot transfer to or from the plasma");
      }

      double rate = transfer_rate[k];

      // Off-diagonal gain
      A(to, from) += rate;

      // Diagonal loss to other component
      A(from, from) -= rate;

    }
  }

  // Fill plasma escape terms
  if (escape_fraction.size() > 0) {
    for (int k = 0; k < escape_fraction.size(); k++) {
  
      if (comp_index.count(escape_to[k]) == 0) {
        throw cyclus::ValueError(
          "Unknown escape destination component: " +
          escape_to[k]);
      }

      int to = comp_index[escape_to[k]];
      
      if (to == plasma) {
        throw cyclus::ValueError(
          "Cannot escape to the plasma");
      }

      double fraction = escape_fraction[k];

      A(to, plasma) = fraction * (1 - TBE) * tritium_consumption_rate;
	
    }
  }

  // Add constant removal from storage into plasma
  int storage = comp_index["storage"];
  A(storage, plasma) = -tritium_consumption_rate;
  
  // Add the tritium source term
  int breeder = comp_index["breeder"];
  A(breeder, plasma) = TBR * TBE * tritium_consumption_rate;

  // Also add diagonal tritium decay term
  for (int k = 0; k < components.size(); k++) {
    if (k == plasma) {continue;}
    A(k, k) -= lambda_T;
  }

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleFusionPlant::Tick() {

  if (ReadyToOperate()) {
    has_started = true;
    fuel_startup_policy.Stop();
    fuel_refill_policy.Start();

    OperateReactor();

  } else {

    // Tritium in the system leaks and decays, but no burning plasma
    OperateReactor(false);

    // Some way of leaving a record of what is going wrong is helpful info I
    // think Use the cyclus logger
  }
  
  double excess_tritium = std::max(tritium_storage.quantity() - 
                                  reserve_inventory, 0.0);
  
  // Otherwise the ResBuf encounters an error when it tries to squash
  if (excess_tritium > cyclus::eps_rsrc()) {
    tritium_excess.Push(tritium_storage.Pop(excess_tritium));
  }

  if (SequesteredTritium() != 0) {
    fuel_startup_policy.Stop();
    fuel_refill_policy.Start();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleFusionPlant::Tock() {
  // ExplicitInventories wasn't working. If possible, may be best to use that
  // down the road.
  RecordInventories(tritium_storage.quantity(), tritium_excess.quantity(),
                    SequesteredTritium());
}

void FlexibleFusionPlant::RecordInventories(double tritium_storage,
                                         double tritium_excess,
                                         double sequestered_tritium) {
  context()
      ->NewDatum("FFPInventories")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("TritiumStorage", tritium_storage)
      ->AddVal("TritiumExcess", tritium_excess)
      ->AddVal("TritiumSequestered", sequestered_tritium)
      ->Record();
}

double FlexibleFusionPlant::SequesteredTritium() {
  double current_sequestered_tritium = 0.0;

  for (int i = 0; i < components.size(); i++) {
    
    if (i == comp_index["plasma"] || tritium_elsewhere[i].empty()
	|| i == comp_index["storage"]) {continue;}
    
    cyclus::toolkit::MatQuery mq(tritium_elsewhere[i].Peek());
    current_sequestered_tritium += mq.mass(tritium_id);
  }

  return current_sequestered_tritium;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool FlexibleFusionPlant::ReadyToOperate() {
  
  // Determine tritium inventory required to operate
  if (tritium_storage.quantity() < startup_inventory &&
      !has_started) {
    return false;
  }
  if (tritium_storage.quantity() < reserve_inventory ||
		  tritium_storage.quantity() < fuel_usage_mass) {
    return false;
  }
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleFusionPlant::OperateReactor(bool burn_tritium) {

  double dt = context()->dt();
  double Q = 0;
  
  // Construct tritium vector and evolve it according to 
  // burn rate and transition rates
  int N = components.size();
  Eigen::VectorXd tritium_vector(N);
  std::vector<cyclus::Material::Ptr> popped_mats(N);
  
  // Remove mass that will be consumed from storage,
  // including that which will not be burned.
  if (burn_tritium) {
    Q = burn_rate / TBE;
  }

  for (int i = 0; i < N; i++) {
   
    // Skip the plasma - does not explicitly contain tritium
    // Essentially assumes tritium has zero residence time in
    // the plasma.
    // Contains '1' to act as an inhomogeneous source
    if (i == comp_index["plasma"]) {
      tritium_vector(i) = 1;
    } else if (i == comp_index["storage"]) {
      tritium_vector(i) = tritium_storage.quantity();
    } else {
      tritium_vector(i) = tritium_elsewhere[i].quantity();
    }

  }
  
  // Set the source term
  BuildMatrix(Q);

  // Evolve the densities of tritium in each component
  Eigen::MatrixXd M = (A * dt).exp();
  Eigen::VectorXd new_tritium = M * tritium_vector;
  
  // Update the densities in the appropriate buffers
  for (int i = 0; i < N; i++) {
   
    if (i == comp_index["plasma"]) continue;
    
    double current_mass = tritium_vector(i);
    double new_mass = std::max(new_tritium(i), 0.0);
    double delta = new_mass - current_mass;

    // Handle Storage Buffer
    if (i == comp_index["storage"]) {
      if (delta < -cyclus::eps_rsrc()) {
        tritium_storage.Pop(-delta);
      } else if (delta > cyclus::eps_rsrc()) {
        tritium_storage.Push(cyclus::Material::Create(this, delta, tritium_comp));
      }
    }
    // Handle Other Components (e.g., Breeder)
    else {
      if (delta < -cyclus::eps_rsrc()) {
        tritium_elsewhere[i].Pop(-delta);
      } else if (delta > cyclus::eps_rsrc()) {
        tritium_elsewhere[i].Push(cyclus::Material::Create(this, delta, tritium_comp));
      }
    }


  }

}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructFlexibleFusionPlant(cyclus::Context* ctx) {
  return new FlexibleFusionPlant(ctx);
}

}  // namespace tricycle
