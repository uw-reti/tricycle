#include "fusion_power_plant.h"

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
FusionPowerPlant::FusionPowerPlant(cyclus::Context* ctx)
    : cyclus::Facility(ctx) {
  fuel_tracker.Init({&tritium_storage}, fuel_limit);

  tritium_storage = ResBuf<Material>(true);
  tritium_excess = ResBuf<Material>(true);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string FusionPowerPlant::str() {
  return Facility::str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::EnterNotify() {
  cyclus::Facility::EnterNotify();

  burn_rate = mass_tritium * fusion_power * MW_to_W/ 
	  (conversion_efficiency * energy_DT);
  fuel_usage_mass = burn_rate * context()->dt();

  int N = compartments.size();
  
  // Size vector of intermediate buffers
  tritium_elsewhere = std::vector<ResBuf<Material>>(N, ResBuf<Material>(true));

  // Build compartment lookup map
  for (int i = 0; i < N; ++i) {
    comp_index[compartments[i]] = i;
  }

  // Ensure that the compartments contain plasma and breeder
  std::vector<std::string> required = {
      "breeder",
      "plasma",
      "storage"
  };

  for (auto const& name : required) {
    if (comp_index.count(name) == 0) {
      throw cyclus::ValueError(
          "Required tritium compartment '" +
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
            reserve_inventory, reserve_inventory)
      .Set(fuel_incommod, tritium_comp)
      .Start();

  // Tritium Buy Policy Selection:
  // Keep it simple until I understand this better!
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
    fuel_refill_policy
        .Init(this, &tritium_storage, std::string("Input"), &fuel_tracker,
              std::string("ss"), reserve_inventory, reserve_inventory)
        .Set(fuel_incommod, tritium_comp);

  } else {
    throw KeyError("Refuel mode " + refuel_mode +
                   " not recognized! Try 'schedule' or 'fill'.");
  }

  tritium_sell_policy.Init(this, &tritium_excess, std::string("Excess Tritium"))
      .Set(fuel_incommod)
      .Start();
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::BuildMatrix(double tritium_consumption_rate) {

  // Wipe any existing elements
  A.setZero();
      
  int plasma = comp_index["plasma"];

  // Fill transfer terms
  if (transfer_rate.size() > 0) {
    for (int k = 0; k < transfer_rate.size(); k++) {

      if (comp_index.count(transfer_from[k]) == 0) {
        throw cyclus::ValueError(
          "Unknown transfer source compartment: " +
          transfer_from[k]);
      }

      if (comp_index.count(transfer_to[k]) == 0) {
        throw cyclus::ValueError(
          "Unknown transfer destination compartment: " +
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
          "Unknown escape destination compartment: " +
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
  for (int k = 0; k < compartments.size(); k++) {
    if (k == plasma) {continue;}
    A(k, k) -= lambda_T;
  }

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::Tick() {

  if (ReadyToOperate()) {
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
                                  (reserve_inventory + SequesteredTritium())
                                  , 0.0);
  
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
void FusionPowerPlant::Tock() {
  // ExplicitInventories wasn't working. If possible, may be best to use that
  // down the road.
  RecordInventories(tritium_storage.quantity(), tritium_excess.quantity(),
                    SequesteredTritium());
}

void FusionPowerPlant::RecordInventories(double tritium_storage,
                                         double tritium_excess,
                                         double sequestered_tritium) {
  context()
      ->NewDatum("FPPInventories")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("TritiumStorage", tritium_storage)
      ->AddVal("TritiumExcess", tritium_excess)
      ->AddVal("TritiumSequestered", sequestered_tritium)
      ->Record();
}

double FusionPowerPlant::SequesteredTritium() {
  double current_sequestered_tritium = 0.0;

  for (int i = 0; i < compartments.size(); i++) {
    
    if (i == comp_index["plasma"] || tritium_elsewhere[i].empty()
	|| i == comp_index["storage"]) {continue;}
    
    cyclus::toolkit::MatQuery mq(tritium_elsewhere[i].Peek());
    current_sequestered_tritium += mq.mass(tritium_id);
  }

  return current_sequestered_tritium;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool FusionPowerPlant::ReadyToOperate() {
  
  // Determine tritium inventory required to operate
  if (tritium_storage.quantity() < fuel_usage_mass ||
		  tritium_storage.quantity() < minimum_startup_mass ||
		  tritium_storage.quantity() < cyclus::eps_rsrc()) {
    return false;
  }
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::OperateReactor(bool burn_tritium) {

  double dt = context()->dt();
  double Q = 0;
  
  // Construct tritium vector and evolve it according to 
  // burn rate and transition rates
  int N = compartments.size();
  Eigen::VectorXd tritium_vector(N);
  std::vector<cyclus::Material::Ptr> popped_mats(N);
  
  // Remove mass that will be consumed from storage,
  // including that which will not be burned.
  // Meanwhile add the remainder in storage to the tritium vector
  // in case stored tritium is expected to leak.
  if (burn_tritium) {
    
    Q = fuel_usage_mass / TBE;

    tritium_storage.Pop(Q);

    cyclus::toolkit::MatQuery mq(tritium_storage.Pop());

    tritium_vector(comp_index["storage"]) = mq.mass(tritium_id) + Q;

  }

  for (int i = 0; i < N; i++) {
   
    double T_mass = 0;

    // Skip the plasma - does not explicitly contain tritium
    // Contains '1' to act as an inhomogeneous source
    if (i == comp_index["plasma"]) {
      tritium_vector(i) = 1;
      continue;
    }

    // Already filled the storage vector element
    if (i == comp_index["storage"]) {
      continue;
    }

    // Place tritium from other components
    if (!tritium_elsewhere[i].empty()) {
      
      popped_mats[i] = tritium_elsewhere[i].Pop();
      
      cyclus::toolkit::MatQuery mq(popped_mats[i]);

      T_mass = mq.mass(tritium_id);

    }

    tritium_vector(i) = T_mass;

  }
  
  // Set the source term
  BuildMatrix(Q);

  // Evolve the densities of tritium
  Eigen::MatrixXd M = (A * dt).exp();
  Eigen::VectorXd new_tritium = M * tritium_vector;
  
  // Replace the densities back in the buffer
  for (int i = 0; i < N; i++) {
   
    if (i == comp_index["plasma"]) {
      continue;
    }
    
    double new_mass = new_tritium(i);

    // Remove anything there erronesously
    if (!tritium_elsewhere[i].empty()) {
      tritium_elsewhere[i].Pop();
    }

    // Avoid tiny negative numerical noise
    new_mass = std::max(new_mass, 0.0);

    if (new_mass > cyclus::eps_rsrc()) {

      cyclus::Material::Ptr mat =
          cyclus::Material::Create(
              this,
              new_mass,
              tritium_comp);

      // Add surplus stored tritium back to storage
      if (i == comp_index["storage"]) {
        tritium_storage.Push(mat);

      // Otherwise put in one of the N buffers
      } else {
        tritium_elsewhere[i].Push(mat);
      }
    }

  }

}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Not sure if this function is needed!
void FusionPowerPlant::DecayInventories() {
  tritium_storage.Decay();
  tritium_excess.Decay();
  for (int i = 0; i < compartments.size(); i++) {
    tritium_elsewhere[i].Decay();
  }
}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructFusionPowerPlant(cyclus::Context* ctx) {
  return new FusionPowerPlant(ctx);
}

}  // namespace tricycle
