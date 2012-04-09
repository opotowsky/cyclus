// BatchReactor.cpp
// Implements the BatchReactor class
#include "BatchReactor.h"

#include "Logger.h"
#include "GenericResource.h"
#include "CycException.h"
#include "InputXML.h"
#include "Timer.h"

#include <iostream>
#include <queue>
#include <sstream>

using namespace std;

/**
  TICK
  TOCK
  RECIEVE MATERIAL
  SEND MATERIAL
 */

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void BatchReactor::init() { 
  preCore_ = new MatBuff();
  inCore_ = new MatBuff();
  postCore_ = new MatBuff();
  ordersWaiting_ = new deque<msg_ptr>();
  fuelPairs_ = new deque<FuelPair>();
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void BatchReactor::init(xmlNodePtr cur) { 
  FacilityModel::init(cur);
  
  // move XML pointer to current model
  cur = XMLinput->get_xpath_element(cur,"model/BatchReactor");
  
  // initialize facility parameters
  setCycleLength( strtod( XMLinput->get_xpath_content(cur,"cyclelength"), NULL ) );
  setLifetime( strtol( XMLinput->get_xpath_content(cur,"lifetime"), NULL, 10 ) ); 
  setCoreLoading( strtod( XMLinput->get_xpath_content(cur,"coreloading"), NULL ) );
  setNBatches( strtol( XMLinput->get_xpath_content(cur,"batchespercore"), NULL, 10 ) ); 
  setBatchLoading( core_loading_ / batches_per_core_ );
  setTimeInOperation(0);

  // all facilities require commodities - possibly many
  string recipe_name;
  string in_commod;
  string out_commod;
  xmlNodeSetPtr nodes = XMLinput->get_xpath_elements(cur, "fuelpair");

  // for each fuel pair, there is an in and an out commodity
  for (int i = 0; i < nodes->nodeNr; i++) {
    xmlNodePtr pair_node = nodes->nodeTab[i];

    // get commods
    in_commod = XMLinput->get_xpath_content(pair_node,"incommodity");
    out_commod = XMLinput->get_xpath_content(pair_node,"outcommodity");

    // get in_recipe
    recipe_name = XMLinput->get_xpath_content(pair_node,"inrecipe");
    setInRecipe(IsoVector::recipe(recipe_name));

    // get out_recipe
    recipe_name = XMLinput->get_xpath_content(pair_node,"outrecipe");
    setOutRecipe(IsoVector::recipe(recipe_name));

    fuelPairs_->push_back(make_pair(make_pair(in_commod,in_recipe_),
          make_pair(out_commod, out_recipe_)));
  }

  setPhase(BEGIN);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
void BatchReactor::copy(BatchReactor* src) {

  FacilityModel::copy(src);

  setCycleLength( src->cycleLength() ); 
  setLifetime( src->lifetime() );
  setCoreLoading( src->coreLoading() );
  setNBatches( src->nBatches() );
  setBatchLoading( coreLoading() / nBatches() ); 
  setInRecipe( src->inRecipe() );
  setOutRecipe( src->outRecipe() );
  setTimeInOperation(0);
  fuelPairs_ = src->fuelPairs_;

  setPhase(BEGIN);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void BatchReactor::copyFreshModel(Model* src) {
  copy(dynamic_cast<BatchReactor*>(src));
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void BatchReactor::print() { 
  FacilityModel::print(); 
  LOG(LEV_DEBUG2, "BReact") << "    converts commodity {"
      << fuelPairs_->front().first.first
      << "} into commodity {"
      << this->fuelPairs_->front().second.first
      << "}.";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void BatchReactor::receiveMessage(msg_ptr msg) {
  // is this a message from on high? 
  if(msg->supplier()==this){
    // file the order
    ordersWaiting_->push_front(msg);
    LOG(LEV_INFO5, "BReact") << name() << " just received an order.";
  }
  else {
    throw CycException("BatchReactor is not the supplier of this msg.");
  }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void BatchReactor::sendMessage(Communicator* recipient, Transaction trans){
      msg_ptr msg(new Message(this, recipient, trans)); 
      msg->setNextDest(facInst());
      msg->sendOn();
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void BatchReactor::handleOrders() {
  while(!ordersWaiting_->empty()){
    msg_ptr order = ordersWaiting_->front();
    order->approveTransfer();
    ordersWaiting_->pop_front();
  };
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
vector<rsrc_ptr> BatchReactor::removeResource(msg_ptr order) {
  Transaction trans = order->trans();
  return ResourceBuff::toRes(postCore_->popQty(trans.resource->quantity()));
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void BatchReactor::handleTick(int time) {
  LOG(LEV_INFO3, "BReact") << name() << " is ticking {";

  // end the facility's life if its time
  if (lifetimeReached()) {
    setPhase(END);
  }
  // request fuel if needed
  if (requestAmt() > EPS_KG) {
    makeRequest(requestAmt());
  }
  // offer used fuel if needed
  if (!postCore_->empty()) {
    makeOffers();
  }

  LOG(LEV_INFO3, "BReact") << "}";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void BatchReactor::handleTock(int time) {
  LOG(LEV_INFO3, "BReact") << name() << " is tocking {";
  
  handleOrders();

  switch(phase()) {
  case BEGIN:
  case REFUEL:
    if (requestMet()) {
      loadCore();
      setPhase(OPERATION);
    }
    else {
      setRequestAmt(requestAmt() - receivedAmt());
    } 
    break; // end BEGIN || REFUEL 
  case OPERATION:
    increaseCycleTimer();
    if (cycleComplete()) {
      setPhase(REFUEL);
    }
    break; // end OPERATION
  case END:
    if (postCore_->empty()) {
      dynamic_cast<InstModel*>(parent())->decommission(this);
    }
    break; // end END
  }
  increaseOperationTimer();

  LOG(LEV_INFO3, "BReact") << "}";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void BatchReactor::setPhase(Phase p) {
  switch (p) {
  case BEGIN:
    setRequestAmt(coreLoading());
    break;
  case REFUEL:
    offloadBatch();
    setRequestAmt(batchLoading());
    break;
  case OPERATION:
    resetRequestAmt();
    resetCycleTimer();
    break;
  case END:
    resetRequestAmt();
    offloadCore();
    break;
  default:
    stringstream err("");
    err << "BatchReactor " << this->name() << " does not have a phase "
        << "enumerated by " << p << ".";
    throw CycOverrideException(err.str()); 
    break;
  }
  phase_ = p;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
bool BatchReactor::requestMet() {
  double remaining = requestAmt() - receivedAmt();
  if (remaining > EPS_KG) {
    return false;
  }
  else if (remaining < -1*EPS_KG) {
    stringstream err("");
    err << "BatchReactor " << this->name() << " received more fuel than was "
        << "expected, which is not currently acceptable behavior; it has a "
        << "surplus of " << -1*remaining << ".";
    throw CycOverrideException(err.str()); 
  }
  return true;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void BatchReactor::interactWithMarket(string commod, double amt, bool offer) {
  LOG(LEV_INFO4, "BReact") << " making requests {";  
  // get the market
  MarketModel* market = MarketModel::marketForCommod(commod);
  Communicator* recipient = dynamic_cast<Communicator*>(market);
  // set the price
  double commod_price = 0;
  // request a generic resource
  gen_rsrc_ptr trade_res = gen_rsrc_ptr(new GenericResource(commod, "kg", amt));
  // build the transaction and message
  Transaction trans;
  trans.commod = commod;
  trans.minfrac = 1.0;
  trans.is_offer = offer;
  trans.price = commod_price;
  trans.resource = trade_res;
  // log the event
  string text;
  if (offer) {
    text = " has offered ";
  }
  else {
    text = " has requested ";
  }
  LOG(LEV_INFO5, "BReact") << name() << text << amt
                           << " kg of " << commod << ".";
  // send the message
  sendMessage(recipient, trans);
  LOG(LEV_INFO4, "BReact") << "}";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void BatchReactor::addFuelPair(std::string incommod, IsoVector inFuel,
                                std::string outcommod, IsoVector outFuel) {
  fuelPairs_->push_back(make_pair(make_pair(incommod, inFuel),
                                 make_pair(outcommod, outFuel)));
}

/* ------------------- */ 


/* --------------------
 * all MODEL classes have these members
 * --------------------
 */

extern "C" Model* constructBatchReactor() {
  return new BatchReactor();
}

/* ------------------- */ 

