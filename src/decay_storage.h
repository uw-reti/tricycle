#ifndef CYCLUS_DECAYSTORAGES_DECAYSTORAGE_FACILITY_H_
#define CYCLUS_DECAYSTORAGES_DECAYSTORAGE_FACILITY_H_

#include <string>

#include "cyclus.h"

namespace decaystorage {

/// @class DecayStorage
///
/// The DecayStorage facility manages tritium storage with radioactive decay
/// and helium-3 extraction. It accepts tritium material, allows it to decay
/// over time, and periodically extracts the helium-3 that forms from tritium
/// decay for separate storage and output.
///
/// @section intro Introduction
/// DecayStorage is designed for fuel cycle simulations involving tritium
/// breeding and handling. It accounts for the radioactive decay of tritium
/// and enables tracking of both the remaining tritium and accumulated
/// helium-3 inventories.
///
/// @section agentparams Agent Parameters
/// - incommod: Input commodity name for accepting tritium material
/// - outcommod: Output commodity name for offering helium-3
///
/// @section detailed Detailed Behavior
/// Each time step, the facility:
/// - Tick: Decays all tritium inventory, then extracts accumulated helium-3
/// - Tock: Transfers incoming material from input buffer to tritium storage
///         and records current inventories
/// The tritium_storage buffer uses bulk storage mode for automatic material
/// combining, and helium-3 is continuously separated and stored independently.
class DecayStorage : public cyclus::Facility  {
 public:
  /// Constructor for DecayStorage Class
  /// @param ctx the cyclus context for access to simulation-wide parameters
  explicit DecayStorage(cyclus::Context* ctx);

  /// The Prime Directive
  /// Generates code that handles all input file reading and restart operations
  /// (e.g., reading from the database, instantiating a new object, etc.).
  /// @warning The Prime Directive must have a space before it! (A fix will be
  /// in 2.0 ^TM)

  #pragma cyclus

  #pragma cyclus note {"doc": "A DecayStorage facility manages tritium storage " \
                              "with radioactive decay and helium-3 extraction."}



  #pragma cyclus var { \
  "tooltip": "Tritium input commodity", \
  "doc": "Input commodity on which DecayStorage requests tritium material.", \
  "uilabel": "Input Commodity", \
  "uitype": "incommodity", \
  }
  std::string incommod;

  #pragma cyclus var { \
  "tooltip": "Helium-3 output commodity", \
  "doc": "Output commodity on which DecayStorage offers extracted helium-3.", \
  "uilabel": "Output Commodity", \
  "uitype": "outcommodity", \
  }
  std::string outcommod;

  #pragma cyclus var {"tooltip":"Input buffer for incoming tritium material"}
  cyclus::toolkit::ResBuf<cyclus::Material> input;

  #pragma cyclus var {"tooltip":"Bulk storage buffer for tritium inventory with decay"}
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_storage{true};

  #pragma cyclus var {"tooltip":"Storage buffer for extracted helium-3"}
  cyclus::toolkit::ResBuf<cyclus::Material> helium_storage;

  #pragma cyclus var {"tooltip":"Tracker to handle on-hand tritium"}
  cyclus::toolkit::TotalInvTracker fuel_tracker;

  /// Policy for requesting tritium material
  cyclus::toolkit::MatlBuyPolicy buy_policy;

  /// Policy for offering helium-3 material
  cyclus::toolkit::MatlSellPolicy sell_policy;



  /// Set up policies and buffers
  virtual void EnterNotify();

  /// A verbose printer for the DecayStorage facility
  virtual std::string str();

  /// Decays tritium inventory and extracts helium-3 each time step
  virtual void Tick();

  /// Transfers incoming material to storage and records inventories
  virtual void Tock();

  /// Extracts helium-3 from decayed tritium and stores it separately
  void ExtractHelium(cyclus::toolkit::ResBuf<cyclus::Material> &inventory);
  
  /// Records current tritium and helium-3 inventory quantities
  void RecordInventories(double tritium, double helium);


  const int He3_id = 20030000;
  const cyclus::CompMap He3 = {{He3_id, 1}};
  const cyclus::Composition::Ptr He3_comp = cyclus::Composition::CreateFromAtom(He3);

  // And away we go!
};

}  // namespace decaystorage

#endif  // CYCLUS_DECAYSTORAGES_DECAYSTORAGE_FACILITY_H_
