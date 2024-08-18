#include <gtest/gtest.h>

#include "fpp.h"

#include "agent_tests.h"
#include "context.h"
#include "facility_tests.h"
#include "pyhooks.h"

using tricycle::fpp;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class fppTest : public ::testing::Test {
 protected:
  cyclus::TestContext tc;
  fpp* facility;

  virtual void SetUp() {
    cyclus::PyStart();
    facility = new fpp(tc.get());
  }

  virtual void TearDown() {
    delete facility;
    cyclus::PyStop();
  }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(fppTest, InitialState) {
  // Test things about the initial state of the facility here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(fppTest, Print) {
  EXPECT_NO_THROW(std::string s = facility->str());
  // Test fpp specific aspects of the print method here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(fppTest, Tick) {
  ASSERT_NO_THROW(facility->Tick());
  // Test fpp specific behaviors of the Tick function here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(fppTest, Tock) {
  EXPECT_NO_THROW(facility->Tock());
  // Test fpp specific behaviors of the Tock function here
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Do Not Touch! Below section required for connection with Cyclus
cyclus::Agent* fppConstructor(cyclus::Context* ctx) {
  return new fpp(ctx);
}
// Required to get functionality in cyclus agent unit tests library
#ifndef CYCLUS_AGENT_TESTS_CONNECTED
int ConnectAgentTests();
static int cyclus_agent_tests_connected = ConnectAgentTests();
#define CYCLUS_AGENT_TESTS_CONNECTED cyclus_agent_tests_connected
#endif  // CYCLUS_AGENT_TESTS_CONNECTED
INSTANTIATE_TEST_CASE_P(fpp, FacilityTests,
                        ::testing::Values(&fppConstructor));
INSTANTIATE_TEST_CASE_P(fpp, AgentTests,
                        ::testing::Values(&fppConstructor));
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
