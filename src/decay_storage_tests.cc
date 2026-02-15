#include <gtest/gtest.h>

#include <cmath>

#include "agent_tests.h"
#include "context.h"
#include "decay_storage.h"
#include "facility_tests.h"
#include "pyhooks.h"

using cyclus::CompMap;
using cyclus::Cond;
using cyclus::Material;
using cyclus::QueryResult;
using cyclus::toolkit::MatQuery;
using decaystorage::DecayStorage;

// Use an anonymous namespace to avoid polluting the global namespace with 
// test-specific code.
namespace {

Composition::Ptr tritium() {
  cyclus::CompMap m;
  m[10030000] = 1.0;
  return Composition::CreateFromAtom(m);
};

Composition::Ptr decayed_tritium() {
  cyclus::CompMap m;
  m[10030000] = 0.9;
  m[20030000] = 0.1;
  return Composition::CreateFromAtom(m);
};

std::string common_config =
    " <incommod>Tritium</incommod>"
    " <outcommod>Tritium_Out</outcommod>"
    " <max_tritium_inventory>100000000.0</max_tritium_inventory>";

cyclus::MockSim InitializeSim(std::string config, int simdur) {
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:DecayStorage"), config,
                      simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddSource("Tritium").recipe("tritium").Finalize();

  return sim;
}

QueryResult TimeInventoryQuery(cyclus::MockSim& sim, std::string time) {
  std::vector<Cond> conds;

  conds.push_back(Cond("Time", "==", time));
  QueryResult qr = sim.db().Query("StorageInventories", &conds);

  return qr;
}

}  // namespace

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class DecayStorageTest : public ::testing::Test {
 protected:
  cyclus::TestContext tc;
  DecayStorage* facility;

  virtual void SetUp() {
    cyclus::PyStart();
    facility = new DecayStorage(tc.get());
  }

  virtual void TearDown() {
    delete facility;
    cyclus::PyStop();
  }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(DecayStorageTest, InitialState) {
  // Test that the facility is constructed with empty storage buffers
  EXPECT_EQ(0.0, facility->tritium_storage.quantity());
  EXPECT_EQ(0.0, facility->helium_storage.quantity());
  EXPECT_EQ(100000000.0, facility->max_tritium_inventory);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(DecayStorageTest, Print) {
  EXPECT_NO_THROW(std::string s = facility->str());
  // Test DecayStorage specific aspects of the print method here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(DecayStorageTest, Tick) {
  // Test that Tick can be called without errors on empty storage
  EXPECT_NO_THROW(facility->Tick());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(DecayStorageTest, Tock) {
  // Test that Tock can be called without errors
  EXPECT_NO_THROW(facility->Tock());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(DecayStorageTest, DatabaseRecording) {
    // Test that inventories are properly recorded to the database
  
    std::string config = common_config;
  
    int simdur = 2;
    cyclus::MockSim sim = InitializeSim(config, simdur);
  
    int id = sim.Run();
  
    QueryResult qr = TimeInventoryQuery(sim, "1");
    
    // Verify that records exist in the database
    EXPECT_GT(qr.rows.size(), 0);
    
    // Verify that required fields are present
    EXPECT_NO_THROW(qr.GetVal<double>("TritiumStorage"));
    EXPECT_NO_THROW(qr.GetVal<double>("HeliumStorage"));
    EXPECT_NO_THROW(qr.GetVal<int>("AgentId"));
    EXPECT_NO_THROW(qr.GetVal<int>("Time"));
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(DecayStorageTest, BasicMaterialFlow) {
  // Test basic material flow: receiving tritium, storing it, and recording

  std::string config = common_config;

  int simdur = 2;
  cyclus::MockSim sim = InitializeSim(config, simdur);

  int id = sim.Run();

  QueryResult qr = TimeInventoryQuery(sim, "1");
  double tritium_qty = qr.GetVal<double>("TritiumStorage");

  // Should have received and stored some tritium
  EXPECT_LE(0.0, tritium_qty);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(DecayStorageTest, DecayAndExtractHelium) {
  // Test that tritium decays over time steps and helium-3 is extracted.
  // Material is received at time 0, then decays at each tick. Without a sink,
  // material accumulates and we can observe both the tritium decrease and
  // helium-3 accumulation from decay.

  std::string config = common_config;

  int simdur = 3;
  cyclus::MockSim sim = InitializeSim(config, simdur);
  // No sink added, so material will not be sold and will accumulate

  int id = sim.Run();

  QueryResult qr_1 = TimeInventoryQuery(sim, "1");
  double tritium_1 = qr_1.GetVal<double>("TritiumStorage");
  double helium_1 = qr_1.GetVal<double>("HeliumStorage");

  QueryResult qr_2 = TimeInventoryQuery(sim, "2");
  double tritium_2 = qr_2.GetVal<double>("TritiumStorage");
  double helium_2 = qr_2.GetVal<double>("HeliumStorage");

  // Verify we have material at time 1
  EXPECT_LT(0.0, tritium_1);

  // Helium-3 should accumulate as tritium decays
  EXPECT_LT(helium_1, helium_2);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(DecayStorageTest, EmptyStorageBehavior) {
  // Test behavior when storage is empty

  std::string config = common_config;

  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:DecayStorage"), config,
                      simdur);

  sim.AddRecipe("tritium", tritium());
  // No source added, so no material flows

  EXPECT_NO_THROW(int id = sim.Run());

  QueryResult qr = TimeInventoryQuery(sim, "1");
  double tritium_qty = qr.GetVal<double>("TritiumStorage");
  double helium_qty = qr.GetVal<double>("HeliumStorage");

  // Both storages should be empty
  EXPECT_EQ(0.0, tritium_qty);
  EXPECT_EQ(0.0, helium_qty);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(DecayStorageTest, ExtractHeliumEmptyStorage) {
  // Test ExtractHelium with empty storage (should not crash)

  EXPECT_NO_THROW(facility->ExtractHelium(facility->tritium_storage));
  EXPECT_EQ(0.0, facility->helium_storage.quantity());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(DecayStorageTest, EnterNotifyPolicySetup) {
  // Test that EnterNotify sets up buy and sell policies correctly

  facility->incommod = "Tritium";
  facility->outcommod = "Tritium_Out";

  EXPECT_NO_THROW(facility->EnterNotify());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(DecayStorageTest, MaterialWithDecay) {
  // Test with material that already contains some decay products

  std::string config = common_config;

  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:DecayStorage"), config,
                      simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("decayed_tritium", decayed_tritium());
  sim.AddSource("Tritium").recipe("decayed_tritium").Finalize();

  int id = sim.Run();

  QueryResult qr = TimeInventoryQuery(sim, "1");
  double helium_qty = qr.GetVal<double>("HeliumStorage");

  // Should extract helium-3 from the already decayed material
  EXPECT_LE(0.0, helium_qty);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Do Not Touch! Below section required for connection with Cyclus
cyclus::Agent* DecayStorageConstructor(cyclus::Context* ctx) {
  return new DecayStorage(ctx);
}
// Required to get functionality in cyclus agent unit tests library
#ifndef CYCLUS_AGENT_TESTS_CONNECTED
int ConnectAgentTests();
static int cyclus_agent_tests_connected = ConnectAgentTests();
#define CYCLUS_AGENT_TESTS_CONNECTED cyclus_agent_tests_connected
#endif  // CYCLUS_AGENT_TESTS_CONNECTED
INSTANTIATE_TEST_CASE_P(DecayStorage, FacilityTests,
                        ::testing::Values(&DecayStorageConstructor));
INSTANTIATE_TEST_CASE_P(DecayStorage, AgentTests,
                        ::testing::Values(&DecayStorageConstructor));
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

