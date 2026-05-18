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

const double MW_to_GW = 1000;
const double lambda_T = std::log(2.0) / (12.32 * 365 * 24 * 3600)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FusionPowerPlant::FusionPowerPlant(cyclus::Context* ctx)
    : cyclus::Facility(ctx) {
  fuel_tracker.Init({&tritium_storage}, fuel_limit);

  tritium_storage = ResBuf<Material>(true);
  tritium_excess = ResBuf<Material>(true);

  // Size vector of intermediate buffers
  tritium_elsewhere = std::vector<ResBuf<Material>(true)>(compartments.size());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string FusionPowerPlant::str() {
  return Facility::str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::EnterNotify() {
  cyclus::Facility::EnterNotify();

  fuel_usage_mass = (burn_rate * (fusion_power / MW_to_GW) /
                     (kDefaultTimeStepDur * 12) * context()->dt());

  int N = compartments.size();

  // Build compartment lookup map
  for (int i = 0; i < N; ++i) {
    comp_index[compartments[i]] = i;
  }

  // Ensure that the compartments contain plasma and breeder
  std::vector<std::string> required = {
      "breeder",
      "plasma",
      "storage",
      "divertor"
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

  // Ensure tritium escape fraction sums to less than one
  double total = 0.0;

  for (int i = 0; i < escape_fractions.size(); i++) {

    double x = escape_fractions[i];

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
  //if (refuel_mode == "schedule") {
  IntDistribution::Ptr active_dist = FixedIntDist::Ptr(new FixedIntDist(1));
  IntDistribution::Ptr dormant_dist =
      FixedIntDist::Ptr(new FixedIntDist(buy_frequency - 1));
  DoubleDistribution::Ptr size_dist =
      FixedDoubleDist::Ptr(new FixedDoubleDist(1));

  fuel_refill_policy
      .Init(this, &tritium_storage, std::string("Input"), &fuel_tracker,
            buy_quantity, active_dist, dormant_dist, size_dist)
      .Set(fuel_incommod, tritium_comp);

  //} else if (refuel_mode == "fill") {
  //  fuel_refill_policy
  //      .Init(this, &tritium_storage, std::string("Input"), &fuel_tracker,
  //            std::string("ss"), reserve_inventory, reserve_inventory)
  //      .Set(fuel_incommod, tritium_comp);

  //} else {
  //  throw KeyError("Refuel mode " + refuel_mode +
  //                 " not recognized! Try 'schedule' or 'fill'.");
  //}

  tritium_sell_policy.Init(this, &tritium_excess, std::string("Excess Tritium"))
      .Set(fuel_incommod)
      .Start();
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::BuildMatrix(double tritium_consumption_rate) {

  // Wipe any existing elements
  A.setZero();

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

      int to   = comp_index[escape_to[k]];
      int plasma = comp_index["plasma"];

      double fraction = escape_fraction[k];

      A(to, plasma) = fraction * (1 - TBE) * tritium_consumption_rate / TBE
	
    }
  }

  // Add removal from storage into plasma
  int storage = comp_index["storage"];
  A(storage, storage) = -tritium_consumption_rate / TBE;
  
  // Add the tritium source term
  int plasma = comp_index["plasma"];
  int breeder = comp_index["breeder"];
  A(breeder, plasma) = TBR * tritium_consumption_rate;

  // Also add diagonal tritium decay term
  for (int k = 0; k < N; k++) {
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

  for (int i = 0; i < compartmens.size(); i++) {
    if (i == comp_index["plasma"]) {continue;}
    cyclus::toolkit::MatQuery mq(tritium_elsewhere[i].Peek())
    current_sequestered_tritium += mq.mass(tritium_id);
  }

  return current_sequestered_tritium
}

bool FusionPowerPlant::TritiumStorageClean() {
  cyclus::toolkit::MatQuery mq(tritium_storage.Peek());
  return cyclus::AlmostEq(mq.mass(tritium_id), tritium_storage.quantity());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool FusionPowerPlant::ReadyToOperate() {
  // Determine tritium inventory required to operate
  double required_storage_inventory = fuel_usage_mass;

  // check  tritium storage quantity requirement
  if (tritium_storage.quantity() < required_storage_inventory ||
      !TritiumStorageClean()) {
    return false;
  }
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FusionPowerPlant::OperateReactor(bool burn_tritium = true;) {

  double dt = context()->dt();
  double Q = 0;
  
  // Remove mass that will be consumed from storage
  // Create tritium source term from plasma
  if (burn_tritium) {
    tritium_storage.Pop(fuel_usage_mass);
    Q = fuel_usage_mass;
  }

  // Construct tritium vector and evolve it according to burn rate and transition rates
  N = compartments.size()
  Eigen::VectorXd tritium_vector(N);
  std::vector<cyclus::Material::Ptr> popped_mats(N);
  
  for (int i = 0; i < N; i++) {
   
    double T_mass = 0;

    // Skip the plasma - does not explicitly contain tritium
    // Contains '1' to act as an inhomogeneous source
    if (i == comp_index["plasma"]) {
      tritium_vector(i) = 1;
      continue;
    }

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
  Eigen::VectorXd new_tritium = (A*dt).exp() * tritium_vector;

  // Replace the densities back in the buffer
  for (int i = 0; i < N; i++) {
   
    if (i == comp_index["plasma"]) {
      continue;
    }
    
    double new_mass = new_tritium(i);
    
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

      tritium_elsewhere[i].Push(mat);
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
