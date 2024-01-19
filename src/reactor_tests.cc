#include <gtest/gtest.h>

#include "reactor.h"

#include "agent_tests.h"
#include "context.h"
#include "facility_tests.h"
#include "pyhooks.h"

using tricycle::Reactor;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class ReactorTest : public ::testing::Test {
 protected:
  cyclus::TestContext tc;
  Reactor* facility;

  virtual void SetUp() {
    cyclus::PyStart();
    facility = new Reactor(tc.get());
  }

  virtual void TearDown() {
    delete facility;
    cyclus::PyStop();
  }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, InitialState) {
  // Test things about the initial state of the facility here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, Print) {
  EXPECT_NO_THROW(std::string s = facility->str());
  // Test Reactor specific aspects of the print method here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, Tick) {
  ASSERT_NO_THROW(facility->Tick());
  // Test Reactor specific behaviors of the Tick function here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, Tock) {
  EXPECT_NO_THROW(facility->Tock());
  // Test Reactor specific behaviors of the Tock function here
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Do Not Touch! Below section required for connection with Cyclus
cyclus::Agent* ReactorConstructor(cyclus::Context* ctx) {
  return new Reactor(ctx);
}
// Required to get functionality in cyclus agent unit tests library
#ifndef CYCLUS_AGENT_TESTS_CONNECTED
int ConnectAgentTests();
static int cyclus_agent_tests_connected = ConnectAgentTests();
#define CYCLUS_AGENT_TESTS_CONNECTED cyclus_agent_tests_connected
#endif  // CYCLUS_AGENT_TESTS_CONNECTED
INSTANTIATE_TEST_CASE_P(Reactor, FacilityTests,
                        ::testing::Values(&ReactorConstructor));
INSTANTIATE_TEST_CASE_P(Reactor, AgentTests,
                        ::testing::Values(&ReactorConstructor));
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
