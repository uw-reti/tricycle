#ifndef CYCLUS_DECAYSTORAGES_DECAYSTORAGE_FACILITY_H_
#define CYCLUS_DECAYSTORAGES_DECAYSTORAGE_FACILITY_H_

#include <string>

#include "cyclus.h"

namespace decaystorage {

/// @class DecaystorageFacility
///
/// This Facility is intended
/// as a skeleton to guide the implementation of new Facility
/// agents.
/// The DecaystorageFacility class inherits from the Facility class and is
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
class DecayStorage : public cyclus::Facility  {
 public:
  /// Constructor for DecaystorageFacility Class
  /// @param ctx the cyclus context for access to simulation-wide parameters
  explicit DecayStorage(cyclus::Context* ctx);

  /// The Prime Directive
  /// Generates code that handles all input file reading and restart operations
  /// (e.g., reading from the database, instantiating a new object, etc.).
  /// @warning The Prime Directive must have a space before it! (A fix will be
  /// in 2.0 ^TM)

  #pragma cyclus

  #pragma cyclus note {"doc": "A decaystorage facility is provided as a skeleton " \
                              "for the design of new facility agents."}\

  #pragma cyclus var { \
  "tooltip": "Storage input commodity", \
  "doc": "Input commodity on which Storage requests material.", \
  "uilabel": "Input Commodity", \
  "uitype": "incommodity", \
  }
  std::string incommod;

  #pragma cyclus var { \
  "tooltip": "Storage output commodity", \
  "doc": "Output commodity on which Storage offers material.", \
  "uilabel": "Output Commodity", \
  "uitype": "outcommodity", \
  }
  std::string outcommod;

  #pragma cyclus var { \
  "doc": "Maximum amount of material that can be transferred in or out each time step", \
  "tooltip": "Maximum amount of material that can be transferred in or out each time step", \
  "units": "kg", \
  "uilabel": "Maximum Throughput" \
  }
  double throughput;

  #pragma cyclus var {"tooltip":"Buffer for handling tritium material to be used in reactor"}
  cyclus::toolkit::ResBuf<cyclus::Material> input;

  #pragma cyclus var {"tooltip":"Buffer for handling tritium material to be used in reactor"}
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_storage;

  #pragma cyclus var {"tooltip":"Buffer for handling tritium material to be used in reactor"}
  cyclus::toolkit::ResBuf<cyclus::Material> helium_storage;

  #pragma cyclus var {"tooltip":"Tracker to handle on-hand tritium"}
  cyclus::toolkit::TotalInvTracker fuel_tracker;

  /// a policy for requesting material
  cyclus::toolkit::MatlBuyPolicy buy_policy;

  /// a policy for sending material
  cyclus::toolkit::MatlSellPolicy sell_policy;



  /// Set up policies and buffers:
  virtual void EnterNotify();

  /// A verbose printer for the DecaystorageFacility
  virtual std::string str();

  /// The handleTick function specific to the DecaystorageFacility.
  /// @param time the time of the tick
  virtual void Tick();

  /// The handleTick function specific to the DecaystorageFacility.
  /// @param time the time of the tock
  virtual void Tock();

  void DecayInventory(cyclus::toolkit::ResBuf<cyclus::Material> &inventory);
  void ExtractHelium(cyclus::toolkit::ResBuf<cyclus::Material> &inventory);
  void CombineInventory(cyclus::toolkit::ResBuf<cyclus::Material> &inventory);
  void RecordInventories(double tritium, double helium);


  const int He3_id = 20030000;
  const cyclus::CompMap He3 = {{He3_id, 1}};
  const cyclus::Composition::Ptr He3_comp = cyclus::Composition::CreateFromAtom(He3);

  // And away we go!
};

}  // namespace decaystorage

#endif  // CYCLUS_DECAYSTORAGES_DECAYSTORAGE_FACILITY_H_
