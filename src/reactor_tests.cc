#include <gtest/gtest.h>

#include "agent_tests.h"
#include "context.h"
#include "facility_tests.h"
#include "pyhooks.h"
#include "reactor.h"

using tricycle::Reactor;

using cyclus::CompMap;
using cyclus::Cond;
using cyclus::Material;
using cyclus::QueryResult;
using cyclus::toolkit::MatQuery;
using pyne::nucname::id;

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
TEST_F(ReactorTest, TickCoreNotLoaded) {
  //  Test Reactor specific behaviors of the Tick function here

 std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 10;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  
  std::vector<Cond> conds;
  conds.push_back(Cond("Status", "==", std::string("Shut-down")));
  QueryResult qr = sim.db().Query("ReactorStatus", &conds);
  double qr_rows = qr.rows.size();

  EXPECT_EQ(simdur, qr_rows);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, TickNotOperational) {
  //  Test Reactor specific behaviors of the Tick function here

 std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>0.1</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 10;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();
  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  
  std::vector<Cond> conds;
  conds.push_back(Cond("Status", "==", std::string("Shut-down")));
  QueryResult qr = sim.db().Query("ReactorStatus", &conds);
  double qr_rows = qr.rows.size();

  EXPECT_EQ(simdur, qr_rows);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, TickCoreReplenish) {
  //  Test Reactor specific behaviors of the Tick function here

 std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 4;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();
  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  
  std::vector<Cond> conds;
  conds.push_back(Cond("TritiumCore", "==", std::string("2.121")));
  QueryResult qr = sim.db().Query("ReactorInventories", &conds);
  double qr_rows = qr.rows.size();

  //If the core isn't replenished during tick it will report less than
  //startup_inventory during some timestep. This checks that in every
  //timestep it's what it should be

  EXPECT_EQ(simdur, qr_rows);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, TickBlanketCycle) {
  //  Test Reactor specific behaviors of the Tick function here

 std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <blanket_turnover_rate>0.03</blanket_turnover_rate>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 4;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();
  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  
  std::vector<Cond> conds;
  conds.push_back(Cond("Event", "==", std::string("Blanket Cycled")));
  QueryResult qr = sim.db().Query("ReactorOperationsLog", &conds);
  std::string msg = qr.GetVal<std::string>("Value");

  std::string expected_msg = "30.000000kg of blanket removed";
  EXPECT_EQ(expected_msg, msg);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, TickBlanketOverCycle) {
  //  Test Reactor specific behaviors of the Tick function here

 std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <blanket_turnover_rate>0.65</blanket_turnover_rate>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();
  sim.AddSource("Enriched_Lithium").capacity(500).recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  
  std::vector<Cond> conds;
  conds.push_back(Cond("Event", "==", std::string("Blanket Not Cycled")));
  QueryResult qr = sim.db().Query("ReactorOperationsLog", &conds);
  std::string msg = qr.GetVal<std::string>("Value");

  std::string expected_msg = "Total blanket material (498.999548) insufficient to extract 650.000000kg!";
  EXPECT_EQ(expected_msg, msg);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, Tock) {
  // Test Reactor specific behaviors of the Tock function here
  // Note: All specific tock functionality tested in other places.

  EXPECT_NO_THROW(facility->Tock());
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, NormalStartup) {
  // Test normal startup behavior of the Startup function

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  // under these conditions, we expect the reactor to be able to startup on
  // timestep 0
  std::vector<Cond> conds;
  conds.push_back(Cond("Time", "==", std::string("0")));
  QueryResult qr = sim.db().Query("ReactorEvents", &conds);
  std::string event = qr.GetVal<std::string>("Event");

  EXPECT_EQ("Startup", event);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, FuelConstrainedStartup) {
  // Test fuel constrained startup behavior of the Startup function

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").capacity(8.0).Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  // Under these conditions, we expect the reactor to be able to startup on
  // timestep 1 but not before then (buys 8kg of T on each timestep, and
  // needs 8.121kg to startup). NOTE: startup occurs after DRE.
  std::vector<Cond> conds0;
  conds0.push_back(Cond("Time", "==", std::string("0")));
  QueryResult qr0 = sim.db().Query("ReactorOperationsLog", &conds0);
  std::string event0 = qr0.GetVal<std::string>("Event");

  EXPECT_EQ("Startup Error", event0);

  std::vector<Cond> conds1;
  conds1.push_back(Cond("Time", "==", std::string("1")));
  QueryResult qr1 = sim.db().Query("ReactorEvents", &conds1);
  std::string event1 = qr1.GetVal<std::string>("Event");

  EXPECT_EQ("Startup", event1);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, NoFuelStartup) {
  // Test no fuel startup behavior of the Startup function

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 10;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  // Under these conditions, we expect the reactor to never start, and to record
  // that it failed to start every timestep.
  std::vector<Cond> conds;
  conds.push_back(Cond("Event", "==", std::string("Startup Error")));
  QueryResult qr = sim.db().Query("ReactorOperationsLog", &conds);
  double qr_rows = qr.rows.size();

  EXPECT_EQ(simdur, qr_rows);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, WrongFuelStartup) {
  // Test wrong fuel startup behavior of the Startup function

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Enriched_Lithium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>Lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 3;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  // Under these conditions, we expect the reactor to never start, and to record
  // that it failed to start every timestep.
  std::vector<Cond> conds;
  conds.push_back(Cond("Event", "==", std::string("Startup Error")));
  QueryResult qr = sim.db().Query("ReactorOperationsLog", &conds);
  double qr_rows = qr.rows.size();

  EXPECT_EQ(3, qr_rows);

  // We also expect that it will record a specific message to show that the fuel
  // input commodity is wrong:

  std::vector<Cond> conds2;
  conds2.push_back(Cond("Event", "==", std::string("Startup Error")));
  QueryResult qr2 = sim.db().Query("ReactorOperationsLog", &conds2);
  std::string value = qr2.GetVal<std::string>("Value");

  std::string expected_message =
      "Startup Failed: Fuel incommod not as expected. "
      "Expected Composition: {{10030000,1.000000}}. Fuel Incommod "
      "Composition: {{30060000,0.300000},{30070000,0.700000}}";
  EXPECT_EQ(value, expected_message);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, DecayInventory) {
  // Test behaviors of the DecayInventory function here
  EXPECT_NO_THROW(facility->DecayInventory(facility->tritium_core));

  // We use unintuitive values for reserve inventory and startup inventory here
  // because they were the values we were originally testing with, and we had
  // already done all the calculations.
  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>Lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  std::vector<Cond> conds;
  conds.push_back(Cond("Time", "==", std::string("1")));
  QueryResult qr = sim.db().Query("ReactorInventories", &conds);
  double he3 = qr.GetVal<double>("HeliumStorage");

  // calculator only gave me 7 decimals, otherwise this should probably
  // be 1e-10.
  EXPECT_NEAR(0.0379868, he3, 1e-7);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, GetComp) {
  // Test behaviors of the GetComp function here

  cyclus::Material::Ptr Li =
      cyclus::Material::CreateUntracked(1, enriched_lithium());
  cyclus::Material::Ptr T = cyclus::Material::CreateUntracked(1, tritium());
  std::string comp_Li = facility->GetComp(Li);
  std::string comp_T = facility->GetComp(T);

  EXPECT_EQ("{{30060000,0.300000},{30070000,0.700000}}", comp_Li);
  EXPECT_EQ("{{10030000,1.000000}}", comp_T);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, CombineInventory) {
  // Test behaviors of the CombineInventory function here

  cyclus::toolkit::ResBuf<cyclus::Material> test_buf;

  cyclus::Material::Ptr T1 = cyclus::Material::CreateUntracked(2.5, tritium());
  cyclus::Material::Ptr T2 = cyclus::Material::CreateUntracked(1, tritium());

  test_buf.Push(T1);
  test_buf.Push(T2);

  EXPECT_EQ(3.5, test_buf.quantity());

  cyclus::Material::Ptr mat = test_buf.Pop();

  EXPECT_EQ(2.5, mat->quantity());

  test_buf.Push(mat);

  facility->CombineInventory(test_buf);

  EXPECT_EQ(3.5, test_buf.quantity());

  cyclus::Material::Ptr combined_mat = test_buf.Pop();

  EXPECT_EQ(3.5, combined_mat->quantity());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, CombineInventoryOneElement) {
  // Test behaviors of the CombineInventory function here

  // do I need the pragma here?
  // #pragma cyclus var {"capacity" : "1000"} //capacity set to 1000 arbitrarily
  cyclus::toolkit::ResBuf<cyclus::Material> test_buf;

  cyclus::Material::Ptr T1 = cyclus::Material::CreateUntracked(2.5, tritium());

  test_buf.Push(T1);

  EXPECT_NO_THROW(facility->CombineInventory(test_buf));

  EXPECT_EQ(2.5, test_buf.quantity());

  cyclus::Material::Ptr combined_mat = test_buf.Pop();

  EXPECT_EQ(2.5, combined_mat->quantity());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, CombineEmptyInventory) {
  // Test behaviors of the CombineInventory function here

  // do I need the pragma here?
  // #pragma cyclus var {"capacity" : "1000"} //capacity set to 1000 arbitrarily
  cyclus::toolkit::ResBuf<cyclus::Material> test_buf;

  EXPECT_NO_THROW(facility->CombineInventory(test_buf));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, ExtractHelium) {
  // Test behaviors of the ExtractHelium function here

  cyclus::toolkit::ResBuf<cyclus::Material> test_buf;
  cyclus::Material::Ptr test_mat =
      cyclus::Material::CreateUntracked(1.0, decayed_tritium());

  std::string comp_original = facility->GetComp(test_mat);

  EXPECT_EQ("{{10030000,0.900000},{20030000,0.100000}}", comp_original);

  test_buf.Push(test_mat);
  facility->ExtractHelium(test_buf);
  cyclus::Material::Ptr extracted_mat = test_buf.Pop();

  std::string comp_extracted = facility->GetComp(extracted_mat);
  EXPECT_EQ("{{10030000,1.000000}}", comp_extracted);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, ExtractNoHelium) {
  // Test behaviors of the ExtractHelium function here

  cyclus::toolkit::ResBuf<cyclus::Material> test_buf;
  cyclus::Material::Ptr test_mat =
      cyclus::Material::CreateUntracked(1.0, tritium());

  std::string comp_original = facility->GetComp(test_mat);

  EXPECT_EQ("{{10030000,1.000000}}", comp_original);

  test_buf.Push(test_mat);
  facility->ExtractHelium(test_buf);
  cyclus::Material::Ptr extracted_mat = test_buf.Pop();

  std::string comp_extracted = facility->GetComp(extracted_mat);
  EXPECT_EQ("{{10030000,1.000000}}", comp_extracted);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, RecordEvent) {
  // Test behaviors of the RecordEvent function here
  // This test is identical to Normal Startup... Redundant?

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  std::vector<Cond> conds;
  conds.push_back(Cond("Time", "==", std::string("0")));
  QueryResult qr = sim.db().Query("ReactorEvents", &conds);
  std::string event = qr.GetVal<std::string>("Event");

  EXPECT_EQ("Startup", event);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, RecordInventories) {
  // Test behaviors of the RecordInventories function here

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 1;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  std::vector<Cond> conds;
  conds.push_back(Cond("Time", "==", std::string("0")));
  QueryResult qr = sim.db().Query("ReactorInventories", &conds);
  double tritium_core = qr.GetVal<double>("TritiumCore");
  double tritium_reserve = qr.GetVal<double>("TritiumReserve");
  double tritium_storage = qr.GetVal<double>("TritiumStorage");
  double blanket = qr.GetVal<double>("LithiumBlanket");
  double helium_storage = qr.GetVal<double>("HeliumStorage");

  EXPECT_EQ(2.121, tritium_core);
  EXPECT_EQ(6.0, tritium_reserve);
  EXPECT_EQ(0.0, tritium_storage);
  EXPECT_EQ(1000.0, blanket);
  EXPECT_EQ(0.0, helium_storage);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, RecordStatus) {
  // Test behaviors of the RecordStatus function here

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  std::vector<Cond> conds0;
  conds0.push_back(Cond("Time", "==", std::string("0")));
  QueryResult qr0 = sim.db().Query("ReactorStatus", &conds0);
  std::string status0 = qr0.GetVal<std::string>("Status");
  double power0 = qr0.GetVal<double>("Power");

  EXPECT_EQ("Shut-down", status0);
  EXPECT_EQ(0.0, power0);

  std::vector<Cond> conds1;
  conds1.push_back(Cond("Time", "==", std::string("1")));
  QueryResult qr1 = sim.db().Query("ReactorStatus", &conds1);
  std::string status1 = qr1.GetVal<std::string>("Status");
  double power1 = qr1.GetVal<double>("Power");

  EXPECT_EQ("Online", status1);
  EXPECT_EQ(300.0, power1);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, DepleteBlanket) {
  // Test behaviors of the DepleteBlanket function here

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  std::vector<Cond> conds;
  conds.push_back(Cond("Time", "==", std::string("1")));
  QueryResult qr = sim.db().Query("ReactorOperationsLog", &conds);
  std::string event = qr.GetVal<std::string>("Event");

  EXPECT_EQ("Blanket Depletion", event);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, OverDepleteBlanket) {
  // Test behaviors of the DepleteBlanket function here

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.50</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium")
      .recipe("enriched_lithium")
      .capacity(10)
      .Finalize();

  int id = sim.Run();

  std::vector<Cond> conds;
  conds.push_back(Cond("Time", "==", std::string("1")));
  QueryResult qr = sim.db().Query("ReactorOperationsLog", &conds);
  std::string event = qr.GetVal<std::string>("Event");

  EXPECT_EQ("Breeding Error", event);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, BreedTritium) {
  // Test behaviors of the BreedTritium function here

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.05</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  std::vector<Cond> conds;
  conds.push_back(Cond("Event", "==", std::string("Bred Tritium")));
  QueryResult qr = sim.db().Query("ReactorOperationsLog", &conds);
  std::string msg = qr.GetVal<std::string>("Value");

  double val = (55.8 * (300.0 / 1000.0) / 31536000.0 * 2629846.0) * 1.05;
  std::string expected_msg = std::to_string(val) + " kg of T bred from blanket";

  EXPECT_EQ(expected_msg, msg);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, OperateReactorSustainingTBR) {
  // Test behaviors of the OperateReactor function here

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.05</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 10;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  
  std::vector<Cond> conds_1;
  conds_1.push_back(Cond("Status", "==", std::string("Online")));
  QueryResult qr_1 = sim.db().Query("ReactorStatus", &conds_1);
  double qr_1_rows = qr_1.rows.size();

  //Reactor always starts offline for initial fuel loading
  EXPECT_EQ(simdur-1, qr_1_rows);

  std::vector<Cond> conds_2;
  conds_2.push_back(Cond("Time", "==", std::string("9")));
  QueryResult qr_2 = sim.db().Query("ReactorInventories", &conds_2);
  double storage_quantity = qr_2.GetVal<double>("TritiumStorage");

  //We should have extra Tritium
  EXPECT_LT(0, storage_quantity);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, OperateReactorNonSustainingTBR) {
  // Test behaviors of the OperateReactor function here
  // Expect very similar behavior to Sustaining scenario
  // except no tritium in storage.

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>0.8</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 10;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  
  std::vector<Cond> conds_1;
  conds_1.push_back(Cond("Status", "==", std::string("Online")));
  QueryResult qr_1 = sim.db().Query("ReactorStatus", &conds_1);
  double qr_1_rows = qr_1.rows.size();

  //Reactor always starts offline for initial fuel loading
  EXPECT_EQ(9, qr_1_rows);

  std::vector<Cond> conds_2;
  conds_2.push_back(Cond("Time", "==", std::string("9")));
  QueryResult qr_2 = sim.db().Query("ReactorInventories", &conds_2);
  double storage_quantity = qr_2.GetVal<double>("TritiumStorage");

  //We should have extra Tritium
  EXPECT_EQ(0, storage_quantity);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, OperateReactorShutdownLackOfTritium) {
  // Test behaviors of the OperateReactor function here

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>0.0</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 25;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").capacity(1).Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  
  std::vector<Cond> conds;
  conds.push_back(Cond("Event", "==", std::string("Core Shut-down")));
  QueryResult qr = sim.db().Query("ReactorEvents", &conds);
  std::string msg = qr.GetVal<std::string>("Value");

  std::string expected_msg = "Not enough tritium to operate";
  EXPECT_EQ(expected_msg, msg);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, OperateReactorCoreSizeTooSmall) {
  // Test behaviors of the OperateReactor function here

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.05</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>0.1</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 10;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();

  
  std::vector<Cond> conds_1;
  conds_1.push_back(Cond("Status", "==", std::string("Shut-down")));
  QueryResult qr_1 = sim.db().Query("ReactorStatus", &conds_1);
  double qr_1_rows = qr_1.rows.size();

  EXPECT_EQ(simdur, qr_1_rows);

  std::vector<Cond> conds_2;
  conds_2.push_back(Cond("Time", "==", std::string("1")));
  QueryResult qr_2 = sim.db().Query("ReactorOperationsLog", &conds_2);
  std::string event = qr_2.GetVal<std::string>("Event");
  std::string msg = qr_2.GetVal<std::string>("Value");

  std::string expected_event = "Operational Error";
  std::string expected_msg = "core startup_inventory of 0.100000 kg insufficient to support fuel_usage of 1.395980kg/timestep!";
  
  EXPECT_EQ(expected_event, event);
  EXPECT_EQ(expected_msg, msg);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, EnterNotifyInitialFillDefault) {
  // Test behaviors of the OperateReactor function here

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

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

  EXPECT_NEAR(0.0379868, quantity_2, 1e-7);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, EnterNotifyScheduleFill) {
  // Test behaviors of the OperateReactor function here

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <buy_quantity>0.1</buy_quantity>"
      "  <buy_frequency>1</buy_frequency>"
      "  <refuel_mode>schedule</refuel_mode>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

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
TEST_F(ReactorTest, EnterNotifyInvalidFill) {
  // Test behaviors of the OperateReactor function here

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.00</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <buy_quantity>0.1</buy_quantity>"
      "  <buy_frequency>1</buy_frequency>"
      "  <refuel_mode>kjnsfdhn</refuel_mode>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 2;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").recipe("tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  EXPECT_THROW(int id = sim.Run(), cyclus::KeyError);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(ReactorTest, EnterNotifySellPolicy) {
  // Test behaviors of the OperateReactor function here

  std::string config =
      "  <fusion_power>300</fusion_power> "
      "  <TBR>1.30</TBR> "
      "  <reserve_inventory>6</reserve_inventory>"
      "  <startup_inventory>2.121</startup_inventory>"
      "  <fuel_incommod>Tritium</fuel_incommod>"
      "  <blanket_incommod>Enriched_Lithium</blanket_incommod>"
      "  <blanket_inrecipe>enriched_lithium</blanket_inrecipe>"
      "  <blanket_size>1000</blanket_size>"
      "  <he3_outcommod>Helium_3</he3_outcommod>";

  int simdur = 10;
  cyclus::MockSim sim(cyclus::AgentSpec(":tricycle:Reactor"), config, simdur);

  sim.AddRecipe("tritium", tritium());
  sim.AddRecipe("enriched_lithium", enriched_lithium());

  sim.AddSource("Tritium").capacity(100).recipe("tritium").Finalize();
  sim.AddSink("Tritium").Finalize();

  sim.AddSource("Enriched_Lithium").recipe("enriched_lithium").Finalize();

  int id = sim.Run();


  std::vector<Cond> conds;
  conds.push_back(Cond("TritiumStorage", "==", std::string("0")));
  QueryResult qr = sim.db().Query("ReactorInventories", &conds);
  double qr_rows = qr.rows.size();
  
  EXPECT_EQ(simdur, qr_rows);

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