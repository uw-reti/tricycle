#include <gtest/gtest.h>

#include <cmath>

#include "agent_tests.h"
#include "context.h"
#include "facility_tests.h"
#include "fusion_power_plant.h"
#include "pyhooks.h"

using cyclus::CompMap;
using cyclus::Cond;
using cyclus::Material;
using cyclus::QueryResult;
using cyclus::toolkit::MatQuery;
using tricycle::FusionPowerPlant;

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

Composition::Ptr enriched_lithium() {
  cyclus::CompMap m;
  m[30060000] = 0.3;
  m[30070000] = 0.7;
  return Composition::CreateFromAtom(m);
};

std::string common_config =
    " <fusion_power>300</fusion_power>"
    " <reserve_inventory>6.0</reserve_inventory>"
    " <sequestered_equilibrium>2.121</sequestered_equilibrium>"
    " <blanket_incommod>Enriched_Lithium</blanket_incommod>"
    " <blanket_outcommod>Depleted_Lithium</blanket_outcommod>"
    " <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
    " <blanket_size>1000</blanket_size>"
    " <he3_outcommod>Helium_3</he3_outcommod>";

cyclus::MockSim InitializeSim(std::string config, int simdur) {
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:FusionPowerPlant"), config,
                      simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());
  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();
  sim.AddSource("Tritium").recipe("tritium").Finalize();

  return sim;
}

QueryResult TimeInventoryQuery(cyclus::MockSim& sim, std::string time) {
  std::vector<Cond> conds;

  conds.push_back(Cond("Time", "==", time));
  QueryResult qr = sim.db().Query("FPPInventories", &conds);

  return qr;
}

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
TEST_F(FusionPowerPlantTest, Tock) {
  EXPECT_NO_THROW(facility->Tock());
  // Test FusionPowerPlant specific behaviors of the Tock function here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, BlanketCycle) {
  // Test that the agent correctly removes and replaces a portion of the
  // blanket every blanket turnover period. The default period is 1 timestep
  // so it is left undefined here.

  std::string config =
      common_config +
      " <fusion_power>300</fusion_power>"
      " <TBR>1.00</TBR>"
      " <fuel_incommod>Tritium</fuel_incommod>"
      " <blanket_turnover_fraction>0.03</blanket_turnover_fraction>";

  int simdur = 4;
  cyclus::MockSim sim = InitializeSim(config, simdur);

  int id = sim.Run();

  QueryResult qr = TimeInventoryQuery(sim, "3");
  double waste = qr.GetVal<double>("BlanketWaste");

  // We expect there to be some amount of waste greater than 0.0 kg
  EXPECT_LT(0, waste);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, BlanketOverCycle) {
  // Test the catch for an overcycle of the blanket. The simulation
  // should not crash when this happens.

  std::string config =
      common_config +
      "  <TBR>1.00</TBR>"
      " <fuel_incommod>Tritium</fuel_incommod>"
      " <blanket_turnover_fraction>0.03</blanket_turnover_fraction>";

  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:FusionPowerPlant"), config,
                      simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();
  sim.AddSource("Enriched_Lithium")
      .capacity(0.5)
      .recipe("enriched_lithium")
      .Finalize();

  EXPECT_NO_THROW(sim.Run());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, WrongFuelStartup) {
  // Test that the agent can identify that it has not recieved the correct fuel
  // to startup, and will appropriately not run.

  std::string config =
      common_config +
      "  <TBR>1.00</TBR>"
      " <fuel_incommod>Enriched_Lithium</fuel_incommod>"
      " <blanket_turnover_fraction>0.03</blanket_turnover_fraction>";

  int simdur = 3;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:FusionPowerPlant"), config,
                      simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());
  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  // Under these conditions, we expect the reactor to never start, meaning it
  // never sequesteres tritium.
  QueryResult qr = TimeInventoryQuery(sim, "2");
  double seq_trit = qr.GetVal<double>("TritiumSequestered");

  EXPECT_EQ(0, seq_trit);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, DecayInventoryExtractHelium) {
  // Test behaviors of the DecayInventory and ExtractHelium function here
  EXPECT_NO_THROW(facility->DecayInventories());

  // We use unintuitive values for reserve inventory and startup inventory here
  // because they were the values we were originally testing with, and we had
  // already done all the calculations.
  std::string config = common_config +
                       " <TBR>1.00</TBR> "
                       " <fuel_incommod>Tritium</fuel_incommod>";

  int simdur = 2;
  cyclus::MockSim sim = InitializeSim(config, simdur);

  int id = sim.Run();

  QueryResult qr = TimeInventoryQuery(sim, "1");
  double he3 = qr.GetVal<double>("HeliumExcess");

  double reserve = 6.0;
  double sequestered = 2.121;

  double init_quant = reserve + sequestered;
  // Lambda in base 2, not base e (see Decay.cc for more info)
  double lambda = 2.57208504984001213e-09;
  double t = 2629846;

  double expected_decay = (init_quant - sequestered) -
                          (init_quant - sequestered) * std::pow(2, -lambda * t);

  EXPECT_NEAR(expected_decay, he3, 1e-6);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, Li7EdgeCases) {
  // Test that FPP does the same thing regardless of Li-7 Contribution

  std::string config_1 = common_config +
                         " <TBR>1.08</TBR> "
                         " <fuel_incommod>Tritium</fuel_incommod>"
                         " <he3_outcommod>Helium_3</he3_outcommod>"
                         " <Li7_contribution>0.00</Li7_contribution>";

  std::string config_2 = common_config +
                         " <TBR>1.08</TBR> "
                         " <fuel_incommod>Tritium</fuel_incommod>"
                         " <Li7_contribution>1.00</Li7_contribution>";

  int simdur = 2;
  cyclus::MockSim sim_1 = InitializeSim(config_1, simdur);

  int id_1 = sim_1.Run();

  cyclus::MockSim sim_2 = InitializeSim(config_2, simdur);

  int id_2 = sim_2.Run();

  QueryResult qr_1 = TimeInventoryQuery(sim_1, "1");
  double excess_1 = qr_1.GetVal<double>("TritiumExcess");

  QueryResult qr_2 = TimeInventoryQuery(sim_2, "1");
  double excess_2 = qr_2.GetVal<double>("TritiumExcess");

  EXPECT_NEAR(excess_1, excess_2, 1e-3);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, OperateReactorSustainingTBR) {
  // Test behaviors of the OperateReactor function here

  std::string config = common_config +
                       " <TBR>1.08</TBR> "
                       " <fuel_incommod>Tritium</fuel_incommod>";

  int simdur = 10;
  cyclus::MockSim sim = InitializeSim(config, simdur);

  int id = sim.Run();

  QueryResult qr = TimeInventoryQuery(sim, "9");
  double excess_quantity = qr.GetVal<double>("TritiumExcess");

  // We should have extra Tritium
  EXPECT_LT(0, excess_quantity);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, EnterNotifyInitialFillDefault) {
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

  EXPECT_EQ(8.121, quantity);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, EnterNotifyScheduleFill) {
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

  EXPECT_EQ(8.121, quantity);

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

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, EnterNotifyInvalidFill) {
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
// Unfinished
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(FusionPowerPlantTest, EnterNotifySellPolicy) {
  // Test sell policy behavior of enter notify.

  std::string config = common_config +
                       " <TBR>1.30</TBR> "
                       " <fuel_incommod>Tritium</fuel_incommod>";

  int simdur = 10;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:FusionPowerPlant"), config,
                      simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").capacity(100).recipe("tritium").Finalize();
  sim.AddSink("Tritium").Finalize();
  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  std::vector<Cond> conds;
  conds.push_back(Cond("TritiumExcess", "==", std::string("0")));
  QueryResult qr = sim.db().Query("FPPInventories", &conds);
  double qr_rows = qr.rows.size();

  EXPECT_EQ(simdur, qr_rows);
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
