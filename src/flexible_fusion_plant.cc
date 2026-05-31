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

const int tritium_id = 10030000;
const cyclus::CompMap T = {{tritium_id, 1}};
const cyclus::Composition::Ptr tritium_comp = cyclus::Composition::CreateFromAtom(T);
const double mass_tritium = pyne::atomic_mass(tritium_id) / (1000.0 * pyne::N_A);

const double MW_to_W = 1000000;
const double MeV_to_J = 1.6021766E-13;
const double lambda_T = std::log(2.0) / (12.32 * cyclusYear);
const double energy_DT = 17.6 * MeV_to_J;

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

  // Only used to instantiate the tracker
  double fuel_limit = 1000.0;
  fuel_tracker.Init({&tritium_storage}, fuel_limit);

  sequestered_tritium = cyclus::Material::CreateUntracked(0.0, tritium_comp);
  
  burn_rate = mass_tritium * fusion_power * MW_to_W/ 
	  (conversion_efficiency * energy_DT);
  fuel_usage_mass = burn_rate * context()->dt();

  failure_probability = 1.0 - std::exp(-failure_frequency * context()->dt() / cyclusYear);  
  
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

  // Ensure that the components contains storage and breeder
  std::vector<std::string> required = {
      "breeder",
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

  // Ensure that the components DO NOT contain plasma
  if (comp_index.count("plasma") > 0) {
    throw cyclus::ValueError("plasma is a reserved component name");
  }

  // Ensure transfer vectors have the same length
  if (transfer_from.size() != transfer_to.size() ||
      transfer_from.size() != transfer_rate.size()) {
    throw cyclus::ValueError(
        "Transfer vectors must have equal length.");
  }
  
  // Check that transfers are going to/from real places
  if (transfer_rate.size() > 0) {
    for (int flow = 0; flow < transfer_rate.size(); flow++) {

      _require_string(comp_index, transfer_from[flow],
		      "Unknown transfer source component: " + 
		      transfer_from[flow]);

      _require_string(comp_index, transfer_to[flow],
		      "Unknown transfer destination component: " + 
		      transfer_to[flow]);

    }
  }
  
  // And escape fractions
  if (escape_to.size() != escape_fraction.size()) {
    throw cyclus::ValueError(
        "Escape vectors must have equal length.");
  }

  // Ensure tritium escape fraction sums to less than one,
  // are all positive, and to real locations
  if (escape_fraction.size() > 0) {
    
    // Check total
    double total = std::accumulate(escape_fraction.begin(),
		    escape_fraction.end(), 0.0);
    if (total > 1.0) {
      throw cyclus::ValueError(
        "Escape fractions must sum to <= 1.");
    }

    // Check positivity
    if (std::any_of(escape_fraction.begin(), escape_fraction.end(),
			    [](double f) {return f < 0;})) {
      throw cyclus::ValueError(
          "All escape fractions must be non-negative.");
    }
    
    // Check validity of destinations
    for (int flow = 0; flow < escape_fraction.size(); flow++) {

      _require_string(comp_index, escape_to[flow],
		      "Unknown escape destination component: " + 
		      escape_to[flow]);
    }

  }

  // Create matrices
  A_burn = BuildMatrix(burn_rate / TBE);
  A_off  = BuildMatrix(0.0);

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
// Constructs matrices used for evolving tritium. Input is the rate at which
// tritium is removed from the store and fed into the plasma. This should be
// zero if the plant is switched off. 
Eigen::MatrixXd FlexibleFusionPlant::BuildMatrix(double tritium_consumption_rate) {

  // Add +1 for the plasma
  int N = components.size() + 1;
  
  // For legibility: plasma is the last element
  int plasma = N - 1;
  
  Eigen::MatrixXd A = Eigen::MatrixXd::Zero(N,N);
      
  // Fill transfer terms
  if (transfer_rate.size() > 0) {
    for (int flow = 0; flow < transfer_rate.size(); flow++) {

      int from = comp_index[transfer_from[flow]];
      int to   = comp_index[transfer_to[flow]];
      
      double rate = transfer_rate[flow];

      // Off-diagonal gain
      A(to, from) += rate;

      // Diagonal loss to other component
      A(from, from) -= rate;

    }
  }

  // Fill plasma escape terms
  if (escape_fraction.size() > 0) {
    for (int flow = 0; flow < escape_fraction.size(); flow++) {

      int to = comp_index[escape_to[flow]];
      
      double fraction = escape_fraction[flow];

      A(to, plasma) = fraction * (1 - TBE) * tritium_consumption_rate;
	
    }
  }

  // Add constant removal from storage into plasma
  // The indexing is due to the plasma being an inhomogeneous source
  // which does not itself gain tritium. Because the plasma vector element
  // is constant, in the matrix equation, this line corresponds to a constant 
  // removal rate from the storage.
  int storage = comp_index["storage"];
  A(storage, plasma) = -tritium_consumption_rate;
  
  // Add the tritium source term
  // In analogy with the above, this corresponds to a constant production rate
  // in the breeder.
  int breeder = comp_index["breeder"];
  A(breeder, plasma) = TBR * TBE * tritium_consumption_rate;

  // Also add diagonal tritium decay term
  for (int component = 0; component < components.size(); component++) {
    A(component, component) -= lambda_T;
  }

  return A;

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

  }

  // Decay any tritium stored in the excess
  tritium_excess.Decay();

  // Accumulate tritium into the excess store
  double excess_tritium = 0.0;
  if (has_started) {
    excess_tritium = std::max(tritium_storage.quantity() - 
                                  (reserve_inventory + margin_to_excess), 0.0);
  }
  
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
    
    if (tritium_elsewhere[i].empty()
        || i == comp_index["storage"]) {continue;}
    
    cyclus::toolkit::MatQuery mq(tritium_elsewhere[i].Peek());
    current_sequestered_tritium += mq.mass(tritium_id);
  }

  return current_sequestered_tritium;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool FlexibleFusionPlant::ReadyToOperate() {
  
  // If the plant has failed, increment the recovery counter
  if (has_failed) {

    // Plant restarts
    if (++recovery_counter >= shutdown_duration) {

      recovery_counter = 0;
      has_failed = false;

    // Plant remains off
    } else {
      return false;
    }
  }
	  
  // Determine tritium inventory required to operate
  const double tritium = tritium_storage.quantity();
  if (tritium < startup_inventory && !has_started) {
    return false;
  }
  if (tritium < std::max(reserve_inventory, fuel_usage_mass)) {
    return false;
  }

  // Check if there is a disruption that prevents operation
  double xi = context()->random_01();
  if (xi < failure_probability) {
    has_failed = true;
    recovery_counter = 0;
    return false;
  }
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleFusionPlant::OperateReactor(bool burn_tritium) {

  double dt = context()->dt();
  double Q = 0;
  
  // Construct tritium vector and evolve it according to 
  // burn rate and transition rates.
  // The final element is the plasma
  int N = components.size() + 1;
  Eigen::VectorXd tritium_vector(N);
  std::vector<cyclus::Material::Ptr> popped_mats(N - 1);
  
  for (int i = 0; i < N; i++) {
   
    // Skip the plasma - does not explicitly contain tritium
    // Essentially assumes tritium has zero residence time in
    // the plasma.
    // Contains '1' to act as an inhomogeneous source
    if (i == N - 1) {
      tritium_vector(i) = 1;
    } else if (i == comp_index["storage"]) {
      tritium_vector(i) = tritium_storage.quantity();
    } else {
      tritium_vector(i) = tritium_elsewhere[i].quantity();
    }

  }
  
  // Choose the matrix based on whether plasma is burning
  Eigen::MatrixXd M;
  if (burn_tritium) {
    M = (A_burn * dt).exp();
  } else {
    M = (A_off * dt).exp();
  }

  // Evolve the densities of tritium in each component
  Eigen::VectorXd new_tritium = M * tritium_vector;
  
  // Update the densities in the appropriate buffers
  for (int i = 0; i < N - 1; i++) {
   
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
    // Handle other components
    else {
      if (delta < -cyclus::eps_rsrc()) {
        tritium_elsewhere[i].Pop(-delta);
      } else if (delta > cyclus::eps_rsrc()) {
        tritium_elsewhere[i].Push(cyclus::Material::Create(this, delta, tritium_comp));
      }
    }


  }

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FlexibleFusionPlant::_require_string(std::map<std::string, int> string_map,
		std::string required, std::string error_string) {
      
  if (string_map.count(required) == 0) {
    throw cyclus::ValueError(error_string);
  }

}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructFlexibleFusionPlant(cyclus::Context* ctx) {
  return new FlexibleFusionPlant(ctx);
}

}  // namespace tricycle
