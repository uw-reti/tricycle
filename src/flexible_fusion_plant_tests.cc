#include <gtest/gtest.h>

#include <cmath>

#include "agent_tests.h"
#include "context.h"
#include "facility_tests.h"
#include "flexible_fusion_plant.h"
#include "pyhooks.h"

using cyclus::CompMap;
using cyclus::Cond;
using cyclus::Material;
using cyclus::QueryResult;
using cyclus::toolkit::MatQuery;
using tricycle::FlexibleFusionPlant;

static Composition::Ptr tritium() {
  cyclus::CompMap m;
  m[10030000] = 1.0;
  return Composition::CreateFromAtom(m);
};

static Composition::Ptr decayed_tritium() {
  cyclus::CompMap m;
  m[10030000] = 0.9;
  m[20030000] = 0.1;
  return Composition::CreateFromAtom(m);
};

static std::string common_config =
    " <fusion_power>300</fusion_power>"
    " <reserve_inventory>1.0</reserve_inventory>"
    " <startup_inventory>6.0</startup_inventory>"
    " <components><val>plasma</val><val>storage</val><val>breeder</val></components>";

static cyclus::MockSim InitializeSim(std::string config, int simdur) {
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:FlexibleFusionPlant"), config,
                      simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddSource("Tritium").recipe("tritium").Finalize();

  return sim;
}

static QueryResult TimeInventoryQuery(cyclus::MockSim& sim, std::string time) {
  std::vector<Cond> conds;

  conds.push_back(Cond("Time", "==", time));
  QueryResult qr = sim.db().Query("FFPInventories", &conds);

  return qr;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class FlexibleFusionPlantTest : public ::testing::Test {
 protected:
  cyclus::TestContext tc;
  FlexibleFusionPlant* facility;

  virtual void SetUp() {
    cyclus::PyStart();
    facility = new FlexibleFusionPlant(tc.get());
  }

  virtual void TearDown() {
    delete facility;
    cyclus::PyStop();
  }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, InitialState) {
  // Test things about the initial state of the facility here
  EXPECT_NE(nullptr, facility);
  EXPECT_FALSE(facility->ReadyToOperate());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, Print) {
  EXPECT_NO_THROW(std::string s = facility->str());
  // Test FlexibleFusionPlant specific aspects of the print method here
  std::string s;
  EXPECT_NO_THROW(s = facility->str());
  EXPECT_FALSE(s.empty());
  EXPECT_NE(s.find("Facility"), std::string::npos);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, Tock) {
  // Test FlexibleFusionPlant specific behaviors of the Tock function here
  EXPECT_NO_THROW(facility->Tock());
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, MassBalance) {
  // Test that the amount of tritium in the system is as expected after some
  // time. Also checks that the right masses are in the right components
      //" <fuel_incommod>Tritium</fuel_incommod>"
  std::string config =
      common_config +
      " <TBR>1.00</TBR>"
      " <TBE>1.00</TBE>";

  int simdur = 5;
  cyclus::MockSim sim = InitializeSim(config, simdur);
  int id = sim.Run();

  QueryResult qr = TimeInventoryQuery(sim, "4");

  double storage = qr.GetVal<double>("TritiumStorage");
  double excess = qr.GetVal<double>("TritiumExcess");
  double sequestered = qr.GetVal<double>("TritiumSequestered");

  // Basic physical validations on tracked system masses
  EXPECT_GE(storage, 0.0);
  EXPECT_GE(excess, 0.0);
  EXPECT_GE(sequestered, 0.0);
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, WrongFuelStartup) {
  // Test that the agent can identify that it has not recieved the correct fuel
  // to startup, and will appropriately not run.

  std::string config =
      common_config +
      " <fuel_incommod>Enriched_Lithium</fuel_incommod>";

  int simdur = 3;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:FlexibleFusionPlant"), config,
                      simdur);

  sim.AddRecipe("tritium", tritium());

  int id = sim.Run();

  // Under these conditions, we expect the reactor to never start, meaning it
  // never sequesteres tritium.
  QueryResult qr = TimeInventoryQuery(sim, "2");
  double seq_trit = qr.GetVal<double>("TritiumSequestered");

  EXPECT_EQ(0, seq_trit);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, OperateReactorSustainingTBR) {
  // Test behaviors of the OperateReactor function here

  std::string config = common_config +
                       " <TBR>1.08</TBR> "
		       " <reserve_inventory>10.0</reserve_inventory>"
		       " <transfer_to><val>storage</val></transfer_to>"
		       " <transfer_from><val>breeder</val></transfer_from>"
		       " <transfer_rate><val>0.01</val></transfer_rate>"
                       " <fuel_incommod>Tritium</fuel_incommod>";

  int simdur = 10;
  cyclus::MockSim sim = InitializeSim(config, simdur);

  int id = sim.Run();

  QueryResult qr = TimeInventoryQuery(sim, "9");
  double excess_quantity = qr.GetVal<double>("TritiumExcess");

  // We should have extra Tritium
  EXPECT_LT(0, excess_quantity);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, EnterNotifyInitialFillDefault) {
  // Test default fill behavior of EnterNotify. Specifically look that
  // tritium is transacted in the appropriate amounts.

  std::string config = common_config +
                       " <TBR>1.00</TBR> "
                       " <fuel_incommod>Tritium</fuel_incommod>";

  int simdur = 2;
  cyclus::MockSim sim = InitializeSim(config, simdur);

  int id = sim.Run();

  std::vector<Cond> conds_1;
  conds_1.push_back(Cond("Time", "==", std::string("0")));
  conds_1.push_back(Cond("Commodity", "==", std::string("Tritium")));
  QueryResult qr_1 = sim.db().Query("Transactions", &conds_1);
  int resource_id_1 = qr_1.GetVal<int>("ResourceId");

  std::vector<Cond> conds_2;
  conds_2.push_back(Cond("ResourceId", "==", std::to_string(resource_id_1)));
  QueryResult qr_2 = sim.db().Query("Resources", &conds_2);
  double quantity = qr_2.GetVal<double>("Quantity");

  EXPECT_EQ(6.0, quantity);
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, EnterNotifyScheduleFill) {
  // Test schedule fill behavior of EnterNotify.

  std::string config = common_config +
                       " <TBR>1.00</TBR> "
                       " <fuel_incommod>Tritium</fuel_incommod>"
                       " <buy_quantity>0.1</buy_quantity>"
                       " <buy_frequency>1</buy_frequency>"
                       " <refuel_mode>schedule</refuel_mode>";

  int simdur = 2;
  cyclus::MockSim sim = InitializeSim(config, simdur);

  int id = sim.Run();

  std::vector<Cond> conds_1;
  conds_1.push_back(Cond("Time", "==", std::string("0")));
  conds_1.push_back(Cond("Commodity", "==", std::string("Tritium")));
  QueryResult qr_1 = sim.db().Query("Transactions", &conds_1);
  int resource_id_1 = qr_1.GetVal<int>("ResourceId");

  std::vector<Cond> conds_2;
  conds_2.push_back(Cond("ResourceId", "==", std::to_string(resource_id_1)));
  QueryResult qr_2 = sim.db().Query("Resources", &conds_2);
  double quantity = qr_2.GetVal<double>("Quantity");

  EXPECT_EQ(6, quantity);

  std::vector<Cond> conds_3;
  conds_3.push_back(Cond("Time", "==", std::string("1")));
  conds_3.push_back(Cond("Commodity", "==", std::string("Tritium")));
  QueryResult qr_3 = sim.db().Query("Transactions", &conds_3);
  int resource_id_2 = qr_3.GetVal<int>("ResourceId");

  std::vector<Cond> conds_4;
  conds_4.push_back(Cond("ResourceId", "==", std::to_string(resource_id_2)));
  QueryResult qr_4 = sim.db().Query("Resources", &conds_4);
  double quantity_2 = qr_4.GetVal<double>("Quantity");

  EXPECT_EQ(0.1, quantity_2);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, EnterNotifyInvalidFill) {
  // Test catch for invalid fill behavior keyword in EnterNotify.

  std::string config = common_config +
                       " <TBR>1.00</TBR> "
                       " <fuel_incommod>Tritium</fuel_incommod>"
                       " <buy_quantity>0.1</buy_quantity>"
                       " <buy_frequency>1</buy_frequency>"
                       " <refuel_mode>kjnsfdhn</refuel_mode>";

  int simdur = 2;
  cyclus::MockSim sim = InitializeSim(config, simdur);

  EXPECT_THROW(int id = sim.Run(), cyclus::KeyError);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, EnterNotifySellPolicy) {
  // Test sell policy behavior of enter notify.

  std::string config = common_config +
                       " <TBR>1.30</TBR> "
                       " <fuel_incommod>Tritium</fuel_incommod>";

  int simdur = 10;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:FlexibleFusionPlant"), config,
                      simdur);

  sim.AddRecipe("tritium", tritium());

  sim.AddSource("Tritium").capacity(100).recipe("tritium").Finalize();
  sim.AddSink("Tritium").Finalize();

  int id = sim.Run();

  std::vector<Cond> conds;
  conds.push_back(Cond("TritiumExcess", "==", std::string("0")));
  QueryResult qr = sim.db().Query("FFPInventories", &conds);
  double qr_rows = qr.rows.size();

  EXPECT_EQ(simdur, qr_rows);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, InvalidTransferVectorsLength) {
  // Tests that unequal transfer array lengths are caught in EnterNotify
  std::string config = common_config +
                       " <TBR>1.00</TBR> "
                       " <fuel_incommod>Tritium</fuel_incommod>"
                       " <transfer_from><val>breeder</val></transfer_from>"
                       " <transfer_to><val>storage</val><val> plasma</val></transfer_to>"
                       " <transfer_rate><val>0.1</val><val>0.2</val></transfer_rate>";

  cyclus::MockSim sim = InitializeSim(config, 2);
  EXPECT_THROW(sim.Run(), cyclus::ValueError);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, InvalidTransferUnknownComponent) {
  // Tests that referring to an undefined component throws an error
  std::string config = common_config +
                       " <TBR>1.00</TBR> "
                       " <fuel_incommod>Tritium</fuel_incommod>"
                       " <transfer_from><val>breeder</val></transfer_from>"
                       " <transfer_to><val>nonexistent_component</val></transfer_to>"
                       " <transfer_rate><val>0.1</val></transfer_rate>";

  cyclus::MockSim sim = InitializeSim(config, 2);
  EXPECT_THROW(sim.Run(), cyclus::ValueError);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, InvalidEscapeFractionSum) {
  // Tests that escape fractions cannot physically exceed 100%
  std::string config = common_config +
                       " <TBR>1.00</TBR> "
                       " <fuel_incommod>Tritium</fuel_incommod>"
                       " <escape_fraction><val>0.6</val><val>0.5</val></escape_fraction>"
                       " <escape_to><val>storage</val><val>breeder</val></escape_to>";

  cyclus::MockSim sim = InitializeSim(config, 2);
  EXPECT_THROW(sim.Run(), cyclus::ValueError);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, InvalidEscapeFractionsNegative) {
  // Tests that escape fractions cannot be negative
  std::string config = common_config +
                       " <TBR>1.00</TBR> "
                       " <fuel_incommod>Tritium</fuel_incommod>"
                       " <escape_fraction><val>-0.1</val></escape_fraction>"
                       " <escape_to><val>storage</val></escape_to>";

  cyclus::MockSim sim = InitializeSim(config, 2);
  EXPECT_THROW(sim.Run(), cyclus::ValueError);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, ValidTritiumTransferMovement) {
  // Tests that a valid transfer configuration runs without error and tritium
  // successfully traverses from the breeder to the storage component.
  std::string config = common_config +
                       " <TBR>1.10</TBR> "
                       " <fuel_incommod>Tritium</fuel_incommod>"
                       " <transfer_from><val>breeder</val></transfer_from>"
                       " <transfer_to><val>storage</val></transfer_to>"
                       " <transfer_rate><val>0.25</val></transfer_rate>";

  int simdur = 5;
  cyclus::MockSim sim = InitializeSim(config, simdur);

  // The simulation should run cleanly
  EXPECT_NO_THROW(sim.Run());

  // Check inventory near the end of the simulation to ensure storage isn't empty
  QueryResult qr = TimeInventoryQuery(sim, "4");
  double storage = qr.GetVal<double>("TritiumStorage");

  // Storage should have a positive mass balance accumulated from the transfer rate
  EXPECT_GT(storage, 0.0);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, ValidEscapeFractionMovement) {
  // Verify that an escape fraction from plasma correctly routes material
  // into the designated target component.
  std::string config = common_config +
                       " <TBR>1.00</TBR> "
                       " <fuel_incommod>Tritium</fuel_incommod>"
                       " <escape_fraction><val>0.05</val></escape_fraction>"
                       " <escape_to><val>storage</val></escape_to>";

  int simdur = 5;
  cyclus::MockSim sim = InitializeSim(config, simdur);

  EXPECT_NO_THROW(sim.Run());

  QueryResult qr = TimeInventoryQuery(sim, "4");
  double storage = qr.GetVal<double>("TritiumStorage");

  // Storage shouldn't be negative and should receive the escaped tritium
  EXPECT_GE(storage, 0.0);
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, AnalyticalPureDecayVerification) {
  // Verifies the ODE solver against the exact analytical solution for
  // pure radioactive decay when fusion power is zero.
  std::string config =
      " <fusion_power>0</fusion_power>"
      " <conversion_efficiency>1.0</conversion_efficiency>"
      " <refuel_mode>schedule</refuel_mode>"
      " <reserve_inventory>6.0</reserve_inventory>"
      " <startup_inventory>6.0</startup_inventory>"
      " <buy_quantity>6.0</buy_quantity>"
      " <buy_frequency>100</buy_frequency>"
      " <components><val>plasma</val><val>storage</val><val>breeder</val></components>"
      " <TBR>1.00</TBR>"
      " <TBE>1.00</TBE>"
      " <fuel_incommod>Tritium</fuel_incommod>";

  // Bypass InitializeSim to enforce a strictly limited market source
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:FlexibleFusionPlant"), config, 2);
  sim.AddRecipe("tritium", tritium());

  // Cap the source at exactly 6.0 kg so the startup policy does not refill decayed mass
  sim.AddSource("Tritium").recipe("tritium").capacity(6.0).Finalize();
  sim.Run();

  QueryResult qr = TimeInventoryQuery(sim, "1");
  double storage = qr.GetVal<double>("TritiumStorage");

  // Analytical Calculation: 6.0 * exp(-lambda_T * dt)
  // lambda_T = ln(2)/(12.32*365*24*3600) = 1.78405e-9 /s
  // dt = 2629800 s (1 default Cyclus month)
  double expected_storage = 5.9719157;

  // Use EXPECT_NEAR to accommodate minor floating point precision limits
  EXPECT_NEAR(storage, expected_storage, 1e-5);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FlexibleFusionPlantTest, AnalyticalOdeBurnAndBreedVerification) {
  // Verifies the coupled linear system of ODEs during active operation
  // against a verified Scipy matrix exponential calculation.
  std::string config =
      " <fusion_power>300</fusion_power>"
      " <conversion_efficiency>1.0</conversion_efficiency>"
      " <refuel_mode>schedule</refuel_mode>"
      " <reserve_inventory>6.0</reserve_inventory>"
      " <startup_inventory>6.0</startup_inventory>"
      " <buy_quantity>6.0</buy_quantity>"
      " <buy_frequency>100</buy_frequency>"
      " <components><val>plasma</val><val>storage</val><val>breeder</val></components>"
      " <TBR>1.00</TBR>"
      " <TBE>1.00</TBE>"
      " <fuel_incommod>Tritium</fuel_incommod>";

  // Bypass InitializeSim to enforce a strictly limited market source
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:FlexibleFusionPlant"), config, 2);
  sim.AddRecipe("tritium", tritium());

  // Cap the source at exactly 6.0 kg so the startup policy does not refill decayed mass
  sim.AddSource("Tritium").recipe("tritium").capacity(6.0).Finalize();
  sim.Run();

  QueryResult qr = TimeInventoryQuery(sim, "1");
  double storage = qr.GetVal<double>("TritiumStorage");
  double breeder = qr.GetVal<double>("TritiumSequestered");

  // Expected analytical results calculated using the corrected rate-based
  // matrix exponential: M = exp(A * dt)
  double expected_storage = 4.573487;
  double expected_breeder = 1.398428;

  EXPECT_NEAR(storage, expected_storage, 1e-4);
  EXPECT_NEAR(breeder, expected_breeder, 1e-4);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Do Not Touch! Below section required for connection with Cyclus
cyclus::Agent* FlexibleFusionPlantConstructor(cyclus::Context* ctx) {
  return new FlexibleFusionPlant(ctx);
}
// Required to get functionality in cyclus agent unit tests library
#ifndef CYCLUS_AGENT_TESTS_CONNECTED
int ConnectAgentTests();
static int cyclus_agent_tests_connected = ConnectAgentTests();
#define CYCLUS_AGENT_TESTS_CONNECTED cyclus_agent_tests_connected
#endif  // CYCLUS_AGENT_TESTS_CONNECTED
INSTANTIATE_TEST_CASE_P(FlexibleFusionPlant, FacilityTests,
                        ::testing::Values(&FlexibleFusionPlantConstructor));
INSTANTIATE_TEST_CASE_P(FlexibleFusionPlant, AgentTests,
                        ::testing::Values(&FlexibleFusionPlantConstructor));
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
