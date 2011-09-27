// InstModel.h
#if !defined(_INSTMODEL_H)
#define _INSTMODEL_H
#include <string>


#include "TimeAgent.h"
#include "Communicator.h"
#include "RegionModel.h"

using namespace std;

//-----------------------------------------------------------------------------
/*
 * The InstModel class is the abstract class/interface used by all institution models
 * 
 * This InstModel is intended as a skeleton to guide the implementation of new
 * Models.
 */
//-----------------------------------------------------------------------------
class InstModel : public TimeAgent, public Communicator
{

/* --------------------
 * all MODEL classes have these members
 * --------------------
 */
public:
  /// Default constructor for InstModel Class
  InstModel() {
    setModelType("Inst");
    commType_=INST_COMM;
  };

  /// every model should be destructable
  virtual ~InstModel() {};
  
  // every model needs a method to initialize from XML
  virtual void init(xmlNodePtr cur);

  // every model needs a method to copy one object to another
  virtual void copy(InstModel* src);



  /**
   * This drills down the dependency tree to initialize all relevant parameters/containers.
   *
   * Note that this function must be defined only in the specific model in question and not in any 
   * inherited models preceding it.
   *
   * @param src the pointer to the original (initialized ?) model to be copied
   */
  virtual void copyFreshModel(Model* src)=0;



  // every model should be able to print a verbose description
  virtual void print();

public:
  /// default InstModel receiver is to ignore message.
  virtual void receiveMessage(Message* msg);

  
  /**
   * Each inst is prompted to do its beginning-of-life-step
   * stuff before the simulation begins.
   *
   * Normally, inst.s simply hand the command down to facilities.
   *
   */
  virtual void handlePreHistory();

  /**
   * Each institution is prompted to do its beginning-of-time-step
   * stuff at the tick of the timer.
   * Default behavior is to ignore the tick.
   *
   * @param time is the time to perform the tick
   */
  virtual void handleTick(int time);

  /**
   * Each institution is prompted to its end-of-time-step
   * stuff on the tock of the timer.
   * Default behavior is to ignore the tock.
   * 
   * @param time is the time to perform the tock
   */
  virtual void handleTock(int time);



protected:


/* ------------------- */ 
/* --------------------
 * all INSTMODEL classes have these members
 * --------------------
 */

public:
  /// sets this institution's region
  void setRegion(Model* my_region) { region_ = my_region; };

  /// returns this institution's region
  RegionModel* getRegion() { return (dynamic_cast<RegionModel*>(region_)); };

  /// adds a facility to this model
  void addFacility(Model* new_fac){ facilities_.push_back(new_fac);};

  /// reports number of facilities in this inst
  int getNumFacilities(){ return facilities_.size();};

  /// queries the power capacity of each facility in the institution
  double getPowerCapacity();

  /// attempts to build another facility of type fac
  virtual bool pleaseBuild(Model* fac);

protected:
  /**
   * Each institution is a member of exactly one region
   */
  Model* region_;

  /**
   * Each institution keeps a list of its facilities;
   */
  vector<Model*> facilities_;

/* ------------------- */ 
  
};

#endif



