#include "reactor.h"
#include "boost/shared_ptr.hpp"

namespace tricycle {

Reactor::Reactor(cyclus::Context* ctx) : cyclus::Facility(ctx) {}

std::string Reactor::str() {
  return Facility::str();
}

void Reactor::Tick() {
  std::cout << "Timestep: " << context()->time() <<std::endl;
  if(operational){
    OperateReactor(TBR);
    blanket_refill_policy.Start();
    Reactor::Record("Online", fusion_power);
  }
  else{
    Reactor::Record("Shut-down", 0);
  }

  DecayInventory(tritium_core);
  DecayInventory(tritium_reserve);
  DecayInventory(tritium_storage);

  
  ExtractHelium(tritium_core);
  ExtractHelium(tritium_reserve);
  ExtractHelium(tritium_storage);
  
  if(!tritium_reserve.empty() && operational){
    double core_deficit = startup_inventory - tritium_core.quantity();
    double surplus = std::max(tritium_reserve.quantity() - reserve_inventory - core_deficit, 0.0);

    cyclus::Material::Ptr reserve_fuel = tritium_reserve.Pop();
    cyclus::Material::Ptr core_fuel = tritium_core.Pop();

    core_fuel->Absorb(reserve_fuel->ExtractQty(core_deficit));
    cyclus::Material::Ptr excess_tritium = reserve_fuel->ExtractQty(surplus);

    tritium_reserve.Push(reserve_fuel);
    tritium_core.Push(core_fuel);
    tritium_storage.Push(excess_tritium);
    CombineInventory(tritium_storage);
  }

  //0.85 is arbitrary, but corresponds to 3 missed purchases at default value for turnover
  if(!blanket.empty() && blanket.quantity() > 0.85*blanket_size){
    std::cout<<"Hello"<<std::endl;
    cyclus::Material::Ptr blanket_mat = blanket.Pop();
    cyclus::Material::Ptr spent_blanket = blanket_mat->ExtractQty(blanket_size*blanket_turnover_rate);
    blanket.Push(blanket_mat);
  }
}

void Reactor::Tock() {
  if(!operational){
    try {
      Startup();
      fuel_startup_policy.Stop();
      blanket_startup_policy.Stop();
      fuel_refill_policy.Start();
      operational = true;
    }
    catch (const std::exception& e) {
      std::cerr << "Exception caught: " << e.what() << std::endl;
    }
  }
  
  CombineInventory(tritium_reserve);
  CombineInventory(blanket);

  std::cout << "Tritium in Core: " << tritium_core.quantity() << std::endl;
  std::cout << "Tritium in Reserve: " << tritium_reserve.quantity() << std::endl;
  std::cout << "Tritium in Storage: " << tritium_storage.quantity() << std::endl;
  std::cout << "Lithium in Storage: " << blanket.quantity() <<std::endl;
  std::cout << "Helium in Storage: " << helium_storage.quantity() << std::endl << std::endl;

}

void Reactor::EnterNotify() {
  cyclus::Facility::EnterNotify();
  
  fuel_startup_policy.Init(this, &tritium_reserve, std::string("Tritium Storage"),reserve_inventory+startup_inventory, 6).Set(fuel_incommod).Start();
  blanket_startup_policy.Init(this, &blanket, std::string("Blanket Startup"),blanket_size).Set(blanket_incommod).Start();
  blanket_refill_policy.Init(this, &blanket, std::string("Blanket Refill"), blanket_size, blanket_size).Set(blanket_incommod);

  //Tritium Buy Policy Selection:
  if(refuel_mode == "schedule"){
    //boost::shared_ptr<cyclus::random_number_generator::DoubleDistribution> quantity_dist = boost::shared_ptr<cyclus::random_number_generator::FixedDoubleDist>(new cyclus::random_number_generator::FixedDoubleDist(buy_quantity));
    //boost::shared_ptr<cyclus::IntDistribution> active_dist = boost::shared_ptr<cyclus::FixedIntDist>(new cyclus::FixedIntDist(1));
    //boost::shared_ptr<cyclus::IntDistribution> dormant_dist = boost::shared_ptr<cyclus::FixedIntDist>(new cyclus::FixedIntDist(buy_frequency-1));
    //boost::shared_ptr<cyclus::IntDistribution> size_dist = boost::shared_ptr<cyclus::FixedIntDist>(new cyclus::FixedIntDist(1));
    //fuel_refill_policy.Init(this, &tritium_reserve, std::string("Input"), quantity_dist, active_dist, dormant_dist, size_dist).Set(fuel_incommod);
  }
  else if(refuel_mode == "fill"){
    fuel_refill_policy.Init(this, &tritium_reserve, std::string("Input"), reserve_inventory, reserve_inventory).Set(fuel_incommod);
  }
  else{
    throw cyclus::KeyError("Refill mode " + refuel_mode + " not recognized! Try 'schedule' or 'fill'.");
  }

  tritium_sell_policy.Init(this, &tritium_storage, std::string("Excess Tritium")).Set(fuel_incommod).Start();
  helium_sell_policy.Init(this, &helium_storage, std::string("Helium-3")).Set(he3_outcommod).Start();
}

void Reactor::PrintComp(cyclus::Material::Ptr mat){
  cyclus::CompMap c = mat->comp()->atom();
  cyclus::compmath::Normalize(&c, 1);
  for(std::map<const int, double>::const_iterator it = c.begin();it != c.end(); ++it){
      std::cout << it->first << " " << it->second << "\n";
  }
}

void Reactor::Startup(){
  cyclus::Material::Ptr initial_reserve = tritium_reserve.Pop();
  try{
    cyclus::Material::Ptr initial_core = initial_reserve->ExtractQty(startup_inventory);
    tritium_core.Push(initial_core);
    tritium_reserve.Push(initial_reserve);
  }
  catch (const std::exception& e) {
    std::cerr << "Exception caught: " << e.what() << std::endl;
    tritium_reserve.Push(initial_reserve);
    throw e;
  }
}

void Reactor::DecayInventory(cyclus::toolkit::ResBuf<cyclus::Material> &inventory){
  if(!inventory.empty()){
    cyclus::Material::Ptr mat = inventory.Pop();  
    mat->Decay(context()->time());
    inventory.Push(mat);
  }
}

void Reactor::CombineInventory(cyclus::toolkit::ResBuf<cyclus::Material> &inventory){
  if(!inventory.empty()){
    cyclus::Material::Ptr base = inventory.Pop();
    int count = inventory.count();
    for(int i=0; i<count; i++){
      cyclus::Material::Ptr m = inventory.Pop();
      base->Absorb(m);
    }

    inventory.Push(base);
  }
}

void Reactor::ExtractHelium(cyclus::toolkit::ResBuf<cyclus::Material> &inventory){
  if(!inventory.empty()){
    cyclus::Material::Ptr mat = inventory.Pop();
    cyclus::CompMap c = mat->comp()->atom();
    cyclus::compmath::Normalize(&c,mat->quantity());

    cyclus::CompMap He3 = {{20030000, 1}};
    
    cyclus::Material::Ptr helium = mat->ExtractComp(c[20030000], cyclus::Composition::CreateFromAtom(He3));
    helium_storage.Push(helium);
    inventory.Push(mat);
  }
}

void Reactor::Record(std::string status, double power){
    context()
      ->NewDatum("ReactorData")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("Status", status)
      ->AddVal("Power", power)
      ->Record();
}

void Reactor::DepleteBlanket(double bred_tritium_mass){
  cyclus::Material::Ptr blanket_mat = blanket.Pop();

  cyclus::CompMap b = blanket_mat->comp()->mass();
  cyclus::compmath::Normalize(&b, blanket_mat->quantity());

  //Percent (as decimal) of T which comes from Li-7 instead of Li-6
  double Li7_contribution = 0.15;

  cyclus::CompMap depleted_comp; 
  
  //This is ALMOST the correct behavior... Fix later?
  if((b[30060000] - (1-Li7_contribution)*2*bred_tritium_mass > 0) && (b[30070000] - Li7_contribution*7.0/3.0*bred_tritium_mass > 0)){
    depleted_comp = {{30070000, b[30070000] - Li7_contribution*7.0/3.0*bred_tritium_mass}, \
                    {30060000, b[30060000] - (1-Li7_contribution)*2*bred_tritium_mass}, \
                    {10030000, b[10030000] + bred_tritium_mass},\
                    {20040000, b[20040000] + 4.0/3.0*bred_tritium_mass}};
    
    //Account for the added mass of the absorbed neutrons
    double neutron_mass_correction = 1.0/3.0*bred_tritium_mass*(1-Li7_contribution);
    cyclus::Material::Ptr additional_mass = cyclus::Material::Create(this, neutron_mass_correction, cyclus::Composition::CreateFromMass(depleted_comp));

    blanket_mat->Transmute(cyclus::Composition::CreateFromMass(depleted_comp));
    blanket_mat->Absorb(additional_mass);
  }
  /*else{
    depleted_comp --> pull out any available T. Not sure how I want to do this yet.
  }*/
  
  blanket.Push(blanket_mat);
}

cyclus::Material::Ptr Reactor::BreedTritium(double fuel_usage, double TBR){
  DepleteBlanket(fuel_usage*TBR);
  cyclus::Material::Ptr mat = blanket.Pop();

  cyclus::CompMap c = mat->comp()->mass();
  cyclus::compmath::Normalize(&c,mat->quantity());

  cyclus::CompMap T = {{10030000, 1}};
  
  cyclus::Material::Ptr bred_fuel = mat->ExtractComp(c[10030000], cyclus::Composition::CreateFromAtom(T));
  blanket.Push(mat);

  return bred_fuel;
}

void Reactor::OperateReactor(double TBR, double burn_rate){
  int seconds_per_year = 31536000;
  double fuel_usage = burn_rate * (fusion_power / 1000) / seconds_per_year * context()->dt();

  cyclus::Material::Ptr storage_fuel = tritium_reserve.Pop();
  cyclus::Material::Ptr core_fuel = tritium_core.Pop();

  cyclus::Material::Ptr used_fuel = core_fuel->ExtractQty(fuel_usage);
  core_fuel->Absorb(BreedTritium(fuel_usage, TBR));


  if(((storage_fuel->quantity() + core_fuel->quantity()) > startup_inventory)){
    storage_fuel->Absorb(core_fuel);

    if(storage_fuel->quantity() >= startup_inventory){
      tritium_core.Push(storage_fuel->ExtractQty(startup_inventory));
    }
    else{
      tritium_core.Push(storage_fuel->ExtractQty(storage_fuel->quantity()));
    }

    tritium_reserve.Push(storage_fuel);

  }
  else{
    storage_fuel->Absorb(core_fuel);
    tritium_reserve.Push(storage_fuel);
    fuel_refill_policy.Stop();
    blanket_refill_policy.Stop();
    fuel_startup_policy.Start();
    operational = false;
  }
  //}

}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructReactor(cyclus::Context* ctx) {
  return new Reactor(ctx);
}

}  // namespace tricycle
