#ifndef CYCLUS_TRICYCLE_REACTOR_H_
#define CYCLUS_TRICYCLE_REACTOR_H_

#include <string>

#include "cyclus.h"
#include "boost/shared_ptr.hpp"

namespace tricycle {

/// @class Reactor
///
/// The Reactor class inherits from the Facility class and is
/// dynamically loaded by the Agent class when requested.
///
/// @section intro Introduction
/// This agnet is designed to function as a basic representation of a fusion
/// energy system with respect to tritium flows. This is currently the alpha
/// version of the agent, and as such some simplifying assumptions were made.
///
/// @section agentparams Agent Parameters
/// Required Inputs:
/// fusion_power - the fusion power of the fusion energy system. Analagous to
/// thermal power. Typical values in the range of 100-5000 MW.
/// TBR - Overall Trituim Breeding Ratio of the fusion energy system. 
/// startup_inventory - total inventory of tritium required to start fusion
/// energy system. Includes a reserve inventory. See Abdou 2023 for more
/// details.
/// fuel_incommod - input commodity for fuel. Currently required to be 100%
/// tritium. Future implementations will change this.
/// blanket_incommod - input commodity for tritium breeding blanket. Currently
/// required to be pure enriched lithium.
/// blanket_inrecipe - input recipe for tritium breeding blanket. Currently
/// required to be pure enriched lithium. Enrichment level is not restricted.
/// he-3_outcommod - output commodity for Helium-3 produced by decay of tritium.
/// Required for the agent to offload this supply.
///
/// @section optionalparams Optional Parameters
/// refuel_mode - mode of refueling reactor. Two options, "schedule" and "fill"
/// Default is fill, which tries to ensure that tritium_storage is greater than
/// or equal to startup_inventory each timestep. Schedule allows a fixed amount
/// of tritium to be purchased on a fixed schedule. See buy_quantity, and
/// buy_frequency for more details.
/// buy_quantity - quantity of tritium to buy each buy cycle for schedule buy
/// purchase mode (in kg). Default is 0.1 kg.
/// buy_frequency - frequency with which to order tritium in schedule buy mode.
/// Delay (in timesteps) between orders is buy_frequency-1. Purchasing once per
/// year, with 1 month timesteps would correspond to a buy frequency of 12. 
/// Default is 1 (buy every time step).
/// Li7_contribution - fraction of tritium which comes from the (n+Li7-->T+He+n)
/// reaction. Default is 0.03 (3%).
/// blanket_size - size of the blanket in kg of enriched lithium. Default is
/// 1000 kg.
/// blanket_turnover_rate - fraction of blanket which gets removed and replaced
/// each timestep. Removed blanket is replaced with the original blanket recipe.
/// Default is 0.05 (5%), but was set arbitrarily.
/// 
/// @section detailed Detailed Behavior
/// Agent begins by checking if there is sufficient tritium in the system to
/// operate. If there is, it burns tritium to achieve input fusion_power, then
/// replenishes itself by breeding tritium based on TBR. If there is not
/// sufficient tritum in the reactor initially, agent records that it did not
/// produce power that timestep. Next, agent decays all inventories of tritium
/// and extracts He-3 from those inventories. Agent then replaces lost mass
/// from He-3 extraction with tritium from storage, and cycles the blanket.
///
/// Agent then enters the DRE, where it attempts to purchase more tritium should
/// there be a deficit, or sell excess should there be a surplus, as well as to
/// purchase blanket material required to refill blanket. Finally, excess He-3
/// is offered to the market.
///
/// During tock, agent checks again whether there is enough tritium in the system
/// and if not, tries to "startup", which entails loading the core with tritium.
/// If it fails to do so, a note is made in the opearational log. Next, any newly
/// purchased tritium is squashed within its resbuf, and a record is made of all
/// inventory quantities.

class Reactor : public cyclus::Facility  {
 public:
  /// Constructor for Reactor Class
  /// @param ctx the cyclus context for access to simulation-wide parameters
  explicit Reactor(cyclus::Context* ctx);

  /// The Prime Directive
  /// Generates code that handles all input file reading and restart operations
  /// (e.g., reading from the database, instantiating a new object, etc.).
  /// @warning The Prime Directive must have a space before it! (A fix will be
  /// in 2.0 ^TM)

  #pragma cyclus

  #pragma cyclus note {"doc": "A facility to model basic operation of a" \
                              "fusion energy system."}

  /// A verbose printer for the Reactor
  virtual std::string str();

  /// Set up policies and buffers:
  virtual void EnterNotify();

  /// The handleTick function specific to the Reactor.
  /// @param time the time of the tick
  virtual void Tick();

  /// The handleTick function specific to the Reactor.
  /// @param time the time of the tock
  virtual void Tock();

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
    "tooltip": "Achievable system tritium breeding ratio before decay", \
    "units": "non-dimensional", \
    "uitype": "range", \
    "range": [0, 1e299], \
    "uilabel": "Tritium Breeding Ratio" \
  }
  double TBR;

  #pragma cyclus var { \
    "doc": "Tritium required to start reactor, includes a reserve inventory", \
    "tooltip": "Tritium inventory required to start reactor, includes reserve", \
    "units": "kg", \
    "uitype": "range", \
    "range": [0, 1e299], \
    "uilabel": "Start-up Inventory" \
  }
  double startup_inventory;

  #pragma cyclus var { \
    "doc": "Minimum tritium inventory to hold in reserve in case of tritium recovery system failure", \
    "tooltip": "Minimum tritium inventory to hold in reserve (excluding core invneotry)", \
    "units": "kg", \
    "uilabel": "Reserve Inventory" \
  }
  double reserve_inventory;  

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
    "doc": "Quantity of fuel which reactor tries to purchase", \
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

  #pragma cyclus var { \
    "doc": "Fresh fuel commodity", \
    "tooltip": "Name of fuel commodity requested", \
    "uilabel": "Fuel input commodity" \
  }
  std::string fuel_incommod;

  #pragma cyclus var { \
    "doc": "Fresh fuel commodity", \
    "tooltip": "Name of fuel commodity requested", \
    "uilabel": "Fuel input commodity" \
  }
  std::string he3_outcommod;

  #pragma cyclus var { \
    "default": 0.03, \
    "doc": "Fraction of tritium that comes from the (n + Li-7 --> T + He + n) reaction", \
    "tooltip": "Fraction of tritium from Li-7 breeding", \
    "units": "dimensionless", \
    "uitype": "range", \
    "range": [0, 1], \
    "uilabel": "Li-7 Contribution" \
  }
  double Li7_contribution;

  #pragma cyclus var { \
    "default": 1000.0, \
    "doc": "Initial mass of full blanket material", \
    "tooltip": "Only blanket material mass, not structural mass", \
    "uitype": "range", \
    "range": [0, 10000], \
    "uilabel": "Initial Mass of Blanket" \
  }
  double blanket_size;

  #pragma cyclus var { \
    "doc": "Fresh fuel commodity", \
    "tooltip": "Name of fuel commodity requested", \
    "uilabel": "Fuel input commodity" \
  }
  std::string blanket_incommod;

  #pragma cyclus var { \
    "doc": "Fresh fuel commodity", \
    "tooltip": "Name of fuel commodity requested", \
    "uilabel": "Fuel input commodity" \
  }
  std::string blanket_inrecipe;

  //WARNING: The default on this is completely arbitrary!
  #pragma cyclus var { \
    "default": 0.05, \
    "doc": "Percent of blanket that gets recycled every blanket turnover period", \
    "tooltip": "Defaults to 0.05 (5%), must be between 0 and 15%", \
    "units": "dimensionless", \
    "uitype": "range", \
    "range": [0, 0.15], \
    "uilabel": "Blanket Turnover Rate" \
  }
  double blanket_turnover_rate;

  #pragma cyclus var { \
    "default": 1, \
    "doc": "number of timesteps between blanket recycles", \
    "tooltip": "Defaults to 0.05 (5%), must be between 0 and 15%", \
    "units": "dimensionless", \
    "uitype": "range", \
    "range": [0, 1000], \
    "uilabel": "Blanket Turnover Rate" \
  }
  int blanket_turnover_frequency;

  bool sufficient_tritium_for_operation = false;
  int seconds_per_year = 31536000;
  int MW_to_GW = 1000;
  double fuel_usage;

  // kg/GW-fusion-power-year (Abdou et al. 2021)
  double burn_rate = 55.8;

  #pragma cyclus var {"tooltip":"Buffer for handling tritium material to be used in reactor"}
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_storage;

  #pragma cyclus var {"tooltip":"Buffer for handling excess tritium material to be sold"}
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_excess;

  #pragma cyclus var {"tooltip":"Buffer for handling helium-3 byproduct material"}
  cyclus::toolkit::ResBuf<cyclus::Material> helium_storage;

  #pragma cyclus var {"tooltip":"Buffer for handling enriched lithium blanket material"}
  cyclus::toolkit::ResBuf<cyclus::Material> blanket;

  #pragma cyclus var {"tooltip":"Tracker to handle on-hand tritium"}
  cyclus::toolkit::TotalInvTracker fuel_tracker;

  #pragma cyclus var {"tooltip":"Tracker to handle blanket material"}
  cyclus::toolkit::TotalInvTracker blanket_tracker;

  cyclus::toolkit::MatlBuyPolicy fuel_startup_policy;
  cyclus::toolkit::MatlBuyPolicy fuel_refill_policy;

  cyclus::toolkit::MatlBuyPolicy blanket_fill_policy;

  cyclus::toolkit::MatlSellPolicy tritium_sell_policy;
  cyclus::toolkit::MatlSellPolicy helium_sell_policy;

  void Startup();
  void OperateReactor(double TBR);
  void DecayInventory(cyclus::toolkit::ResBuf<cyclus::Material> &inventory);
  void CombineInventory(cyclus::toolkit::ResBuf<cyclus::Material> &inventory);
  void ExtractHelium(cyclus::toolkit::ResBuf<cyclus::Material> &inventory);
  void RecordEvent(std::string name, std::string val);
  void RecordStatus(std::string Status, double power);
  void RecordInventories(double storage, double excess, double blanket, double helium);
  void RecordOperationalInfo(std::string name, std::string val);
  void DepleteBlanket(double bred_tritium_mass);
  cyclus::Material::Ptr BreedTritium(double fuel_usage, double TBR);
  std::string GetComp(cyclus::Material::Ptr mat);

  // And away we go!
};

}  // namespace tricycle

#endif  // CYCLUS_TRICYCLE_REACTOR_H_
