#ifndef CYCLUS_TRICYCLE_FUSION_POWER_PLANT_H_
#define CYCLUS_TRICYCLE_FUSION_POWER_PLANT_H_

#include <string>

#include "cyclus.h"
#include "boost/shared_ptr.hpp"
#include "pyne.h"
#include <Eigen/Dense>
#include <unsupported/Eigen/MatrixFunctions>

using cyclus::Material;

namespace tricycle {

/// @class FusionPowerPlant
/// The FusionPowerPlant class inherits from the Facility class and is
/// dynamically loaded by the Agent class when requested.
///
/// @section intro Introduction
/// This agent is designed to function as a basic representation of a fusion
/// power plant with respect to tritium flows. This is currently the alpha
/// version of the agent, and as such some simplifying assumptions were made.
/// This version only tracks tritium; not lithium or He.
///
/// @section agentparams Agent Parameters
/// .... e.g., power?
/// Place a description of the required input parameters which define the
/// agent implementation. Saving for Later.
///
/// @section optionalparams Optional Parameters
/// ... e.g., specific plant type
/// Place a description of the optional input parameters to define the
/// agent implementation. Saving for later.
///
/// @section detailed Detailed Behavior
/// ... 
/// Place a description of the detailed behavior of the agent. Consider
/// describing the behavior at the tick and tock as well as the behavior
/// upon sending and receiving materials and messages.
///
/// This section needs to be filled out once there is some behavior to describe.
///
class FusionPowerPlant : public cyclus::Facility  {
 public:
  /// Constructor for FusionPowerPlant Class
  /// @param ctx the cyclus context for access to simulation-wide parameters
  explicit FusionPowerPlant(cyclus::Context* ctx);

  /// The Prime Directive
  /// Generates code that handles all input file reading and restart operations
  /// (e.g., reading from the database, instantiating a new object, etc.).
  /// @warning The Prime Directive must have a space before it! (A fix will be
  /// in 2.0 ^TM)

  #pragma cyclus


  #pragma cyclus note {"doc": "A stub facility is provided as a skeleton " \
                              "for the design of new facility agents."}

  /// Set up policies and buffers:
  virtual void EnterNotify();

  virtual ~FusionPowerPlant() {};

  /// The handleTick function specific to the FusionPowerPlant.
  /// @param time the time of the tick
  virtual void Tick();

  /// The handleTick function specific to the FusionPowerPlant.
  /// @param time the time of the tock
  virtual void Tock();

  /// A verbose printer for the FusionPowerPlant
  virtual std::string str();

  //State Variables:
  #pragma cyclus var { \
    "doc": "Nameplate fusion power of the reactor", \
    "tooltip": "Nameplate fusion power", \
    "units": "MW", \
    "uitype": "range", \
    "range": [0, 1e299], \
    "uilabel": "Fusion Power" \
  }
  double fusion_power;

  #pragma cyclus var { \
    "doc": "Achievable system tritium breeding ratio before decay", \
    "default": 1.2, \
    "tooltip": "Achievable system tritium breeding ratio before decay", \
    "units": "non-dimensional", \
    "uitype": "range", \
    "range": [0, 1e299], \
    "uilabel": "Tritium Breeding Ratio" \
  }
  double TBR;
  
  #pragma cyclus var { \
    "doc": "Tritium burn efficiency; fraction of tritium entering plasma which is burned", \
    "default": 1, \
    "tooltip": "Fraction of tritium entering the plasma which is burned", \
    "units": "non-dimensional", \
    "uitype": "range", \
    "range": [0, 1], \
    "uilabel": "Tritium Burn Efficiency" \
  }
  double TBE;

  #pragma cyclus var { \
    "doc": "Amount of tritium consumed per ...", \
    "default": 55.8, \
    "tooltip": "Tritium consumption rate", \
    "units": "FFFF", \
    "uitype": "range", \
    "range": [0, 1e299], \
    "uilabel": "Tritium Burn Rate" \
  }
  double burn_rate;

  #pragma cyclus var {"tooltip":"Compartment names"}
  std::vector<std::string> compartments;

  #pragma cyclus var {"tooltip":"Transfer source compartments"}
  std::vector<std::string> transfer_from;

  #pragma cyclus var {"tooltip":"Transfer destination compartments"}
  std::vector<std::string> transfer_to;

  #pragma cyclus var {"tooltip":"Transfer rates",\
                      "units": "s^-1",\
                      "range": [0, 1e299]}
  std::vector<double> transfer_rate;
  
  #pragma cyclus var {\
    "doc": "Fraction of unburned tritium which escapes to a given component. These "\
	   "should sum to 1, otherwise tritium will be lost without being recorded.",\
    "tooltip":"Fractions of unburned tritium",\
    "units": "dimensionless",\
    "range": [0, 1]}
  std::vector<double> escape_fractions;
  
  #pragma cyclus var {\
    "doc": "Names of compartments to which tritium escapes from the plasma", \
    "tooltip": "Tritium from plasma destination compartments"}
  std::vector<std::string> escape_to;

  #pragma cyclus var { \
    "doc": "Minimum tritium inventory to hold in reserve in case of tritium recovery system failure", \
    "tooltip": "Minimum tritium inventory to hold in reserve", \
    "units": "kg", \
    "uilabel": "Reserve Inventory" \
  }
  double reserve_inventory;  

  #pragma cyclus var { \
    "doc": "Fresh fuel commodity", \
    "tooltip": "Name of fuel commodity requested", \
    "default": "Tritium", \
    "uilabel": "Fuel input commodity" \
  }
  std::string fuel_incommod;

  #pragma cyclus var { \
    "default": 'fill', \
    "doc": "Method of refueling the reactor", \
    "tooltip": "Options: 'schedule' or 'fill'", \
    "uitype": "combobox", \
    "categorical": ['schedule', 'fill'], \
    "uilabel": "Refuel Mode" \
  }
  std::string refuel_mode;

  #pragma cyclus var { \
    "default": 0.1, \
    "doc": "Quantity of fuel reactor tries to purchase in schedule mode", \
    "tooltip": "Defaults to 100g/purchase", \
    "units": "kg", \
    "uitype": "range", \
    "range": [0, 1e299], \
    "uilabel": "Buy quantity" \
  }
  double buy_quantity;

  #pragma cyclus var { \
    "default": 1, \
    "doc": "Frequency which reactor tries to purchase new fuel", \
    "tooltip": "Reactor is active for 1 timestep, then dormant for buy_frequency-1 timesteps", \
    "units": "Timesteps", \
    "uitype": "range", \
    "range": [0, 1e299], \
    "uilabel": "Buy frequency" \
  }
  int buy_frequency;


  //Functions:
  bool ReadyToOperate();
  void OperateReactor();
  void DecayInventories();
  void BuildMatrix(double tritium_consumption_rate);
  bool TritiumStorageClean();
  double SequesteredTritium();
  void RecordInventories(double tritium_storage, double tritium_excess, 
                         double sequestered_tritium);


 private:
  //Resource Buffers and Trackers:
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_storage;
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_excess;
  std::vector<cyclus::toolkit::ResBuf<cyclus::Material>> tritium_elsewhere;

  cyclus::toolkit::MatlBuyPolicy fuel_startup_policy;
  cyclus::toolkit::MatlBuyPolicy fuel_refill_policy;

  cyclus::toolkit::MatlSellPolicy tritium_sell_policy;

  cyclus::toolkit::TotalInvTracker fuel_tracker;

  //This is to correctly instantiate the TotalInvTracker(s)
  double fuel_limit = 1000.0;
  double fuel_usage_mass;

  //NucIDs for Pyne
  const int tritium_id = 10030000;
  
  //Compositions:
  const cyclus::CompMap T = {{tritium_id, 1}};
  const cyclus::Composition::Ptr tritium_comp = cyclus::Composition::CreateFromAtom(T);

  //Materials:
  cyclus::Material::Ptr sequestered_tritium = cyclus::Material::CreateUntracked(0.0, tritium_comp);
  cyclus::Material::Ptr incore_fuel = cyclus::Material::CreateUntracked(0.0, tritium_comp);

  // Transition rate matrix
  Eigen::MatrixXd A;

  // Indices of different compartments
  std::map<std::string,int> comp_index;

  // And away we go!
};

}  // namespace tricycle

#endif  // CYCLUS_TRICYCLE_FUSION_POWER_PLANT_H_

