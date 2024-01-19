#include "reactor.h"

namespace tricycle {

//-----------------------------------------------------------//
//                       Constructor                         //
//-----------------------------------------------------------//
Reactor::Reactor(cyclus::Context* ctx) : cyclus::Facility(ctx) {}

std::string Reactor::str() {
  return Facility::str();
}

//-----------------------------------------------------------//
//                      Tick and Tock                        //
//-----------------------------------------------------------//
void Reactor::Tick() {
  std::cout << "Timestep: " << context()->time() <<std::endl;
  if(operational){
    OperateReactor(TBR);
  }
  DecayInventory(tritium_storage);
  DecayInventory(tritium_core);
}

void Reactor::Tock() {
  if(!operational){
    try {
      Startup();
      fuel_startup_policy.Stop();
      blanket_startup_policy.Stop();
      fuel_schedule_policy.Start();
      operational = true;
    }
    catch (const std::exception& e) {
      std::cerr << "Exception caught: " << e.what() << std::endl;
    }
  }
  
  CombineInventory(tritium_storage);

  std::cout << "Tritium in Core: " << tritium_core.quantity() << std::endl;
  std::cout << "Tritium in Storage: " << tritium_storage.quantity() << std::endl;
  std::cout << "Lithium in Storage: " << blanket.quantity() <<std::endl;
  std::cout << "Helium in Storage: " << helium_storage.quantity() << std::endl << std::endl;

}

//---------------------------------------------------------------------------//
//                          Buy and Sell Policies                            //
//---------------------------------------------------------------------------//
void Reactor::EnterNotify() {
  cyclus::Facility::EnterNotify();
  fuel_startup_policy.Init(this, &tritium_storage, std::string("Tritium Storage"),reserve_inventory+startup_inventory, 6).Set(fuel_incommod).Start();
  fuel_schedule_policy.Init(this, &tritium_storage, std::string("Input"), buy_quantity).Set(fuel_incommod);
  blanket_startup_policy.Init(this, &blanket, std::string("Blanket"),blanket_size).Set(blanket_incommod).Start();
  //sell_policy.Init(this, &helium_storage, std::string("Helium Storage"),throughput).Set(outcommod).Start();

  //For Version 0, we will ignore the purchasing of Li, and instead simply create some. This will need to be changed in Version 1.
  //cyclus::CompMap enriched_Li = {{30060000, lithium_enrichment}, {30070000, 1-lithium_enrichment}};
  //cyclus::Material::Ptr breeding_blanket = cyclus::Material::Create(this,blanket_size,Li_recipe);
  //blanket.Push(breeding_blanket);
  //std::cout<< "TEST: " <<blanket.quantity() <<std::endl;
}

//---------------------------------------------------------------------------//
//                              Test Functions                               //
//---------------------------------------------------------------------------//
void Reactor::PrintComp(cyclus::Material::Ptr mat){
  cyclus::CompMap c = mat->comp()->atom();
  cyclus::compmath::Normalize(&c, 1);
  for(std::map<const int, double>::const_iterator it = c.begin();it != c.end(); ++it){
      std::cout << it->first << " " << it->second << "\n";
  }

}

//-----------------------------------------------------------//
//                     Fusion Functions                      //
//-----------------------------------------------------------//

void Reactor::Startup(){
  cyclus::Material::Ptr initial_reserve = tritium_storage.Pop();
  try{
    cyclus::Material::Ptr initial_core = initial_reserve->ExtractQty(startup_inventory);
    tritium_core.Push(initial_core);
    tritium_storage.Push(initial_reserve);
  }
  catch (const std::exception& e) {
    std::cerr << "Exception caught: " << e.what() << std::endl;
    tritium_storage.Push(initial_reserve);
    throw e;
  }
}

void Reactor::DecayInventory(cyclus::toolkit::ResBuf<cyclus::Material> &inventory){
  if(!inventory.empty()){
    cyclus::Material::Ptr mat = inventory.Pop();  
    mat->Decay(context()->time());
    inventory.Push(mat);
  }
  else{
    std::cout<<"Resource Buffer is Empty!"<<std::endl;
  }
}

void Reactor::CombineInventory(cyclus::toolkit::ResBuf<cyclus::Material> &inventory){
  cyclus::Material::Ptr base = inventory.Pop();
  int count = inventory.count();
  for(int i=0; i<count; i++){
    cyclus::Material::Ptr m = inventory.Pop();
    base->Absorb(m);
  }

  inventory.Push(base);
}

void Reactor::ExtractHelium(cyclus::Material::Ptr mat){
  cyclus::CompMap c = mat->comp()->atom();
  cyclus::compmath::Normalize(&c,mat->quantity());

  cyclus::CompMap He3 = {{20030000, 1}};
  
  cyclus::Material::Ptr helium = mat->ExtractComp(c[20030000], cyclus::Composition::CreateFromAtom(He3));
  helium_storage.Push(helium);
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

  //normalize the compmap to the correct masses of the constituent parts
  cyclus::CompMap b = blanket_mat->comp()->mass();
  cyclus::compmath::Normalize(&b, blanket_mat->quantity());

  double Li7_contribution = 0;

  

  //Define the new composition of the blanket
  cyclus::CompMap depleted_comp = {{30070000, b[30070000] - Li7_contribution*7.0/3.0*bred_tritium_mass}, \
                                   {30060000, b[30060000] - (1-Li7_contribution)*2*bred_tritium_mass}, \
                                   {10030000, b[10030000] + bred_tritium_mass},\
                                   {20040000, b[20040000] + 4.0/3.0*bred_tritium_mass}};


  //Account for the added mass of the absorbed neutrons
  double neutron_mass_correction = 1.0/3.0*bred_tritium_mass*(1-Li7_contribution);
  cyclus::Material::Ptr additional_mass = cyclus::Material::Create(this, neutron_mass_correction, cyclus::Composition::CreateFromMass(depleted_comp));

  blanket_mat->Transmute(cyclus::Composition::CreateFromMass(depleted_comp));
  blanket_mat->Absorb(additional_mass);


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
  
  //If more fuel is bred than burned, add the difference to storage
  if(TBR >= 1){
    //Pull all the fuel out so we can work with it:
    cyclus::Material::Ptr storage_fuel = tritium_storage.Pop();
    cyclus::Material::Ptr core_fuel = tritium_core.Pop();

    //This will break if fuel_usage > core_fuel, which may not be what we want... Fix Later?

    //Burn the appropriate amount of fuel then replenish it with the bread fuel
    cyclus::Material::Ptr used_fuel = core_fuel->ExtractQty(fuel_usage);
    core_fuel->Absorb(BreedTritium(fuel_usage, TBR));
    storage_fuel->Absorb(core_fuel);

    ExtractHelium(storage_fuel);

    cyclus::Material::Ptr homoginized_core_fuel = storage_fuel->ExtractQty(startup_inventory);

    //Add all the fuel back to the core/storage
    tritium_core.Push(homoginized_core_fuel);
    tritium_storage.Push(storage_fuel);  

    Reactor::Record("Online", fusion_power);
    
  }
  //If more fuel is burned than bred, remove the difference from the core, and
  //then replenish the core from storage
  else{
    //Calculate how much fuel we need to remove
    //double fuel_qty_to_remove = fuel_usage - bred_fuel_qty;
    
    //Pull all the fuel out
    cyclus::Material::Ptr storage_fuel = tritium_storage.Pop();
    cyclus::Material::Ptr core_fuel = tritium_core.Pop();

    //Burn the appropriate amount of fuel then replenish it with the bread fuel
    cyclus::Material::Ptr used_fuel = core_fuel->ExtractQty(fuel_usage);
    core_fuel->Absorb(BreedTritium(fuel_usage, TBR));

    //Extract Helium:
    ExtractHelium(storage_fuel);
    ExtractHelium(core_fuel);

    //test that there's at least enough tritium in the system to start-up the reactor
    if(((storage_fuel->quantity() + core_fuel->quantity()) > startup_inventory)){
      //cyclus::Material::Ptr used_fuel = core_fuel->ExtractQty(fuel_qty_to_remove); 
      double core_deficit = startup_inventory - core_fuel->quantity(); 

      cyclus::Material::Ptr fuel_to_core;

      if(storage_fuel->quantity() >= core_deficit){
        fuel_to_core = storage_fuel->ExtractQty(core_deficit);
      }
      else{
        fuel_to_core = storage_fuel->ExtractQty(storage_fuel->quantity());
      }
      //Take the difference out of storage and put it into the core
      core_fuel->Absorb(fuel_to_core);

      //Put the fuel back
      tritium_core.Push(core_fuel);
      tritium_storage.Push(storage_fuel);

      Reactor::Record("Online", fusion_power);
    }
    else{
      //We don't have enough fuel to replenish the core, so we shutdown

      storage_fuel->Absorb(core_fuel);
      tritium_storage.Push(storage_fuel);
      fuel_schedule_policy.Stop();
      fuel_startup_policy.Start();
      operational = false;
      Reactor::Record("Shut-down", 0);
    }
  }

}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* ConstructReactor(cyclus::Context* ctx) {
  return new Reactor(ctx);
}

}  // namespace tricycle
