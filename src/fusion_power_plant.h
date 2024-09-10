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
  //Functions:
  /// A verbose printer for the FusionPowerPlant
  virtual std::string str();

  /// The handleTick function specific to the FusionPowerPlant.
  /// @param time the time of the tick
  virtual void Tick();

  /// The handleTick function specific to the FusionPowerPlant.
  /// @param time the time of the tock
  virtual void Tock();

  //Member Variables:

  //Functions:
  void CycleBlanket();

  //Resource Buffers:
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_storage;
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_excess;
  cyclus::toolkit::ResBuf<cyclus::Material> helium_excess;
  cyclus::toolkit::ResBuf<cyclus::Material> blanket_feed;
  cyclus::toolkit::ResBuf<cyclus::Material> blanket_waste;

  //PyneIDs
  const int tritium_id = 10030000;

  //Compositions:
  const cyclus::CompMap T = {{tritium_id, 1}};
  //something like this... Not sure if this will work here though...
  const cyclus::Composition::Ptr enriched_li = context->GetRecipe(blanket_inrecipe);
  const cyclus::Composition::Ptr tritium_comp = cyclus::Composition::CreateFromAtom(T);

  //Materials:
  cyclus::Material::Ptr sequestered_tritium = cyclus::Material::CreateUntracked(0.0, tritium_comp);
  
  //This might make more sense to do in EnterNotify()? I'll have to see how it
  //works outside of pesudocode.
  cyclus::Material::Ptr blanket = cyclus::Composition::CreateFromAtom(0.0, enriched_li);

  // And away we go!
};

}  // namespace tricycle

#endif  // CYCLUS_TRICYCLE_FUSION_POWER_PLANT_H_
