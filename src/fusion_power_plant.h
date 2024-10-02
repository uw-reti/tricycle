#ifndef CYCLUS_TRICYCLE_FUSION_POWER_PLANT_H_
#define CYCLUS_TRICYCLE_FUSION_POWER_PLANT_H_

#include <string>

#include "cyclus.h"

namespace tricycle {

/// @class FusionPowerPlant
/// The FusionPowerPlant class inherits from the Facility class and is
/// dynamically loaded by the Agent class when requested.
///
/// @section intro Introduction
/// This agent is designed to function as a basic representation of a fusion
/// power plant with respect to tritium flows. This is currently the alpha
/// version of the agent, and as such some simplifying assumptions were made.
///
/// @section agentparams Agent Parameters
/// Place a description of the required input parameters which define the
/// agent implementation. Saving for Later.
///
/// @section optionalparams Optional Parameters
/// Place a description of the optional input parameters to define the
/// agent implementation. Saving for later.
///
/// @section detailed Detailed Behavior
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
    "doc": "Fresh fuel commodity", \
    "tooltip": "Name of fuel commodity requested", \
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
    "doc": "Helium-3 output commodity Designation", \
    "tooltip": "He-3 output commodity", \
    "uilabel": "He-3 output commodity" \
  }
  std::string he3_outcommod;

  #pragma cyclus var { \
    "doc": "Recipe for blanket feed material", \
    "tooltip": "Recipe for blanket feed material", \
    "uilabel": "Recipe for blanket feed material" \
  }
  std::string blanket_inrecipe;

  #pragma cyclus var { \
    "doc": "Blanket waste commodity designation", \
    "tooltip": "Blanket waste commodity", \
    "uilabel": "Blanket waste commodity" \
  }
  std::string blanket_outcommod;

  //Resource Buffers and Trackers:
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_storage;
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_excess;
  cyclus::toolkit::ResBuf<cyclus::Material> helium_excess;
  cyclus::toolkit::ResBuf<cyclus::Material> blanket_feed;
  cyclus::toolkit::ResBuf<cyclus::Material> blanket_waste;

  cyclus::toolkit::MatlBuyPolicy fuel_startup_policy;
  cyclus::toolkit::MatlBuyPolicy fuel_refill_policy;
  cyclus::toolkit::MatlBuyPolicy blanket_fill_policy;

  cyclus::toolkit::MatlSellPolicy tritium_sell_policy;
  cyclus::toolkit::MatlSellPolicy helium_sell_policy;
  cyclus::toolkit::MatlSellPolicy blanket_waste_sell_policy;

  cyclus::toolkit::TotalInvTracker fuel_tracker;
  cyclus::toolkit::TotalInvTracker blanket_tracker;

  //Functions:
  void CycleBlanket();
  bool CheckOpeartingConditions();
  bool CheckInitialStartupCondition();
  void SequesterTritium();
  void OperateReactor();
  void CycleBlanket();
  void DecayInventories();
  void ExtractHelium();
  void MoveExcessTritiumToSellBuffer();

  private:
    //This is to correctly instantiate the TotalInvTracker(s)
    double fuel_limit = 1000.0;
    double blanket_limit = 100000.0; 
    Material:Ptr blanket;


    //NucIDs for Pyne
    const int tritium_id = 10030000;

    //Compositions:
    const cyclus::CompMap T = {{tritium_id, 1}};
    const cyclus::Composition::Ptr tritium_comp = cyclus::Composition::CreateFromAtom(T);

    //Materials:
    cyclus::Material::Ptr sequestered_tritium = cyclus::Material::CreateUntracked(0.0, tritium_comp);

  // And away we go!
};

}  // namespace tricycle

#endif  // CYCLUS_TRICYCLE_FUSION_POWER_PLANT_H_
