#ifndef CYCLUS_TRICYCLE_REACTOR_H_
#define CYCLUS_TRICYCLE_REACTOR_H_

#include <string>

#include "cyclus.h"

namespace tricycle {

/// @class Reactor
///
/// This Facility is intended
/// as a skeleton to guide the implementation of new Facility
/// agents.
/// The Reactor class inherits from the Facility class and is
/// dynamically loaded by the Agent class when requested.
///
/// @section intro Introduction
/// Place an introduction to the agent here.
///
/// @section agentparams Agent Parameters
/// Place a description of the required input parameters which define the
/// agent implementation.
///
/// @section optionalparams Optional Parameters
/// Place a description of the optional input parameters to define the
/// agent implementation.
///
/// @section detailed Detailed Behavior
/// Place a description of the detailed behavior of the agent. Consider
/// describing the behavior at the tick and tock as well as the behavior
/// upon sending and receiving materials and messages.
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

  #pragma cyclus note {"doc": "A stub facility is provided as a skeleton " \
                              "for the design of new facility agents."}

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


//-----------------------------------------------------------//
//                     State Variables                       //
//-----------------------------------------------------------//

  #pragma cyclus var { \
    "doc": "Nameplate fusion power of the reactor", \
    "tooltip": "Nameplate fusion power", \
    "units": "MW", \
    "uilabel": "Fusion Power" \
  }
  double fusion_power;

  #pragma cyclus var { \
    "doc": "Achievable system tritium breeding ratio before decay", \
    "tooltip": "Achievable system tritium breeding ratio before decay", \
    "units": "non-dimensional", \
    "uilabel": "Tritium Breeding Ratio" \
  }
  double TBR;

  #pragma cyclus var { \
    "doc": "Minimum tritium inventory to hold in reserve in case of tritium recovery system failure", \
    "tooltip": "Minimum tritium inventory to hold in reserve (excluding core invneotry)", \
    "units": "kg", \
    "uilabel": "Reserve Inventory" \
  }
  double reserve_inventory;

  #pragma cyclus var { \
    "doc": "Tritium core inventory required to start reactor", \
    "tooltip": "Tritium core inventory required to start reactor", \
    "units": "kg", \
    "uilabel": "Start-up Inventory" \
  }
  double startup_inventory;


  #pragma cyclus var { \
    "doc": "Quantity of fuel which reactor tries to purchase", \
    "tooltip": "Defaults to fill reserve inventory", \
    "units": "kg", \
    "uilabel": "Buy quantity" \
  }
  double buy_quantity;

  #pragma cyclus var { \
    "doc": "Fresh fuel commodity", \
    "tooltip": "Name of fuel commodity requested", \
    "uilabel": "Fuel input commodity" \
  }
  std::string fuel_incommod;

/* !Marked for deletion!
  #pragma cyclus var { \
    "doc": "Fresh fuel recipe", \
    "tooltip": "Fresh fuel recipe", \
    "uilabel": "Fuel Input Recipe" \
  }
  std::string fuel_inrecipe;
*/
  #pragma cyclus var { \
    "default": 100.0, \
    "doc": "Initial mass of full blanket material", \
    "tooltip": "Only blanket material mass, not structural mass", \
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
/*

    #pragma cyclus var { \
    "doc": "Fresh Fuel recipe", \
    "tooltip": "Fresh fuel recipe", \
    "uilabel": "Fuel Input Recipe" \
  }
  std::string blanket_inrecipe;
*/

  bool operational = false; 
//-----------------------------------------------------------//
//                     Materail Buffers                      //
//-----------------------------------------------------------//

/*These must be defined after member variables for some reason?*/

  //Tritium stored in the core of the reactor
  #pragma cyclus var {"capacity" : "1000"} //capacity set to 1000 arbitrarily
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_core;

  //Tritium stored in the reserve system of the reactor
  #pragma cyclus var {"capacity" : "1000"}
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_storage;

  //helium-3 extracted from decayed tritium and stored by the reactor
  #pragma cyclus var {"capacity" : "1000"}
  cyclus::toolkit::ResBuf<cyclus::Material> helium_storage;

  //blanket material
  #pragma cyclus var {"capacity" : "1000"}
  cyclus::toolkit::ResBuf<cyclus::Material> blanket;

//-----------------------------------------------------------//
//                   Buy and Sell Policies                   //
//-----------------------------------------------------------//
  cyclus::toolkit::MatlBuyPolicy fuel_startup_policy;
  cyclus::toolkit::MatlBuyPolicy fuel_schedule_policy;
  cyclus::toolkit::MatlBuyPolicy fuel_refill_policy;
  cyclus::toolkit::MatlBuyPolicy blanket_startup_policy;


  cyclus::toolkit::MatlSellPolicy sell_policy;

//-----------------------------------------------------------//
//                     Fusion Functions                      //
//-----------------------------------------------------------//

  void Startup();
  void OperateReactor(double TBR, double burn_rate=55.8);
  void DecayInventory(cyclus::toolkit::ResBuf<cyclus::Material> &inventory);
  void CombineInventory(cyclus::toolkit::ResBuf<cyclus::Material> &inventory);
  void ExtractHelium(cyclus::Material::Ptr material);
  void Record(std::string Status, double power);
  void DepleteBlanket(double bred_tritium_mass);
  cyclus::Material::Ptr BreedTritium(double fuel_usage, double TBR);



//-----------------------------------------------------------//
//                      Test Functions                       //
//-----------------------------------------------------------//
  void PrintComp(cyclus::Material::Ptr mat);

  // And away we go!
};

}  // namespace tricycle

#endif  // CYCLUS_TRICYCLE_REACTOR_H_
