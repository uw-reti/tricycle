#ifndef CYCLUS_TRICYCLE_FPP_H_
#define CYCLUS_TRICYCLE_FPP_H_

#include <string>

#include "cyclus.h"

namespace tricycle {

/// @class fpp
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
/// Place a description of the required input parameters which define the
/// agent implementation. None so far.
///
/// @section optionalparams Optional Parameters
/// Place a description of the optional input parameters to define the
/// agent implementation. None so far.
///
/// @section detailed Detailed Behavior
/// Place a description of the detailed behavior of the agent. Consider
/// describing the behavior at the tick and tock as well as the behavior
/// upon sending and receiving materials and messages.
/// This has not yet been defined.
///
class fpp : public cyclus::Facility  {
 public:
  /// Constructor for fpp Class
  /// @param ctx the cyclus context for access to simulation-wide parameters
  explicit fpp(cyclus::Context* ctx);

  /// The Prime Directive
  /// Generates code that handles all input file reading and restart operations
  /// (e.g., reading from the database, instantiating a new object, etc.).
  /// @warning The Prime Directive must have a space before it! (A fix will be
  /// in 2.0 ^TM)

  #pragma cyclus

  #pragma cyclus note {"doc": "A stub facility is provided as a skeleton " \
                              "for the design of new facility agents."}

  /// A verbose printer for the fpp
  virtual std::string str();

  /// The handleTick function specific to the fpp.
  /// @param time the time of the tick
  virtual void Tick();

  /// The handleTick function specific to the fpp.
  /// @param time the time of the tock
  virtual void Tock();

  //Resource Buffers:
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_core;
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_reserve;
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_excess;
  cyclus::toolkit::ResBuf<cyclus::Material> helium_storage;
  cyclus::toolkit::ResBuf<cyclus::Material> blanket;

  // And away we go!
};

}  // namespace tricycle

#endif  // CYCLUS_TRICYCLE_FPP_H_
