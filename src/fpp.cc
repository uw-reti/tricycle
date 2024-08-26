#include "fpp.h"

namespace tricycle {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
fpp::fpp(cyclus::Context* ctx) : cyclus::Facility(ctx) {}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string fpp::str() {
  return Facility::str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void fpp::Tick() {
  std::cout<<"Hello"<<std::endl;

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void fpp::Tock() {
  std::cout<<"World!"<<std::endl;
}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* Constructfpp(cyclus::Context* ctx) {
  return new fpp(ctx);
}

}  // namespace tricycle
