#ifndef CYCLUS_DECAYSTORAGES_DECAYSTORAGE_FACILITY_H_
#define CYCLUS_DECAYSTORAGES_DECAYSTORAGE_FACILITY_H_

#include <string>

#include "cyclus.h"

namespace decaystorage {

/// @class DecayStorage
///
/// The DecayStorage facility provides tritium storage and tracking with
/// proper radioactive decay accounting. It accepts incoming tritium material,
/// stores it with bulk storage, applies decay each time step, and offers
/// the decayed tritium back to the market. Helium-3 extraction from decay is 
/// performed as a byproduct and may be offered in future enhancements.
///
/// @section intro Introduction
/// DecayStorage is designed for fuel cycle simulations involving tritium
/// breeding and handling. It provides tracking of tritium inventory
/// with proper decay accounting. The facility accounts for radioactive decay of
/// tritium each time step and maintains records of both tritium and
/// accumulated helium-3 inventories.
///
/// @section agentparams Agent Parameters
/// - incommod: Input commodity name for accepting tritium material
/// - outcommod: Output commodity name for offering stored tritium
///
/// @section detailed Detailed Behavior
/// Each time step, the facility:
/// - Tick: Decays all tritium inventory, then extracts accumulated helium-3
///         (helium-3 is treated as a byproduct)
/// - Tock: Transfers incoming material from input buffer to tritium storage
///         and records current inventories
/// The tritium_storage buffer uses bulk storage mode for automatic material
/// combining, and helium-3 is continuously separated and stored independently
/// as a byproduct (not currently offered to market).
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

  #pragma cyclus note {"doc": "A DecayStorage facility provides tritium storage " \
                              "and tracking with proper radioactive decay accounting."}



  #pragma cyclus var { \
  "tooltip": "Tritium input commodity", \
  "doc": "Input commodity on which DecayStorage requests tritium material.", \
  "uilabel": "Input Commodity", \
  "uitype": "incommodity", \
  }
  std::string incommod;

  #pragma cyclus var { \
  "tooltip": "Tritium output commodity", \
  "doc": "Output commodity on which DecayStorage offers decayed tritium material.", \
  "uilabel": "Output Commodity", \
  "uitype": "outcommodity", \
  }
  std::string outcommod;

  #pragma cyclus var {"tooltip":"Bulk input buffer for incoming tritium material"}
  cyclus::toolkit::ResBuf<cyclus::Material> input{true};

  #pragma cyclus var {"tooltip":"Bulk storage buffer for tritium inventory with decay"}
  cyclus::toolkit::ResBuf<cyclus::Material> tritium_storage{true};

  #pragma cyclus var {"tooltip":"Bulk storage buffer for extracted helium-3 byproduct"}
  cyclus::toolkit::ResBuf<cyclus::Material> helium_storage{true};

  /// Required to make the matl_buy/sell_policy work
  #pragma cyclus var {"tooltip":"Tracker to handle on-hand tritium"}
  cyclus::toolkit::TotalInvTracker fuel_tracker;

  /// Policy for requesting tritium material
  cyclus::toolkit::MatlBuyPolicy buy_policy;

  /// Policy for offering tritium material
  cyclus::toolkit::MatlSellPolicy sell_policy;



  /// Set up policies and buffers
  virtual void EnterNotify();

  /// A verbose printer for the DecayStorage facility
  virtual std::string str();

  /// Decays tritium inventory and extracts helium-3 byproduct each time step
  virtual void Tick();

  /// Transfers incoming material to storage and records inventories
  virtual void Tock();

  /// Extracts helium-3 byproduct from decayed tritium and stores it separately
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
