#include <gtest/gtest.h>

#include "fusion_power_plant.h"

#include "agent_tests.h"
#include "context.h"
#include "facility_tests.h"
#include "pyhooks.h"

using tricycle::FusionPowerPlant;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class FusionPowerPlantTest : public ::testing::Test {
 protected:
  cyclus::TestContext tc;
  FusionPowerPlant* facility;

  virtual void SetUp() {
    cyclus::PyStart();
    facility = new FusionPowerPlant(tc.get());
  }

  virtual void TearDown() {
    delete facility;
    cyclus::PyStop();
  }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, InitialState) {
  // Test things about the initial state of the facility here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, Print) {
  EXPECT_NO_THROW(std::string s = facility->str());
  // Test FusionPowerPlant specific aspects of the print method here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, Tick) {
  ASSERT_NO_THROW(facility->Tick());
  // Test FusionPowerPlant specific behaviors of the Tick function here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, Tock) {
  EXPECT_NO_THROW(facility->Tock());
  // Test FusionPowerPlant specific behaviors of the Tock function here
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Do Not Touch! Below section required for connection with Cyclus
cyclus::Agent* FusionPowerPlantConstructor(cyclus::Context* ctx) {
  return new FusionPowerPlant(ctx);
}
// Required to get functionality in cyclus agent unit tests library
#ifndef CYCLUS_AGENT_TESTS_CONNECTED
int ConnectAgentTests();
static int cyclus_agent_tests_connected = ConnectAgentTests();
#define CYCLUS_AGENT_TESTS_CONNECTED cyclus_agent_tests_connected
#endif  // CYCLUS_AGENT_TESTS_CONNECTED
INSTANTIATE_TEST_CASE_P(FusionPowerPlant, FacilityTests,
                        ::testing::Values(&FusionPowerPlantConstructor));
INSTANTIATE_TEST_CASE_P(FusionPowerPlant, AgentTests,
                        ::testing::Values(&FusionPowerPlantConstructor));
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
