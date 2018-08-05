// AUTOGENERATED file, created by the tool generate_stub.py, don't edit!
// If you want to add your own functionality, instead edit the stubWorldRouter_custom.h file.

#ifndef STUB_SERVER_WORLDROUTER_H
#define STUB_SERVER_WORLDROUTER_H

#include "server/WorldRouter.h"
#include "stubWorldRouter_custom.h"

#ifndef STUB_WorldRouter_shouldBroadcastPerception
//#define STUB_WorldRouter_shouldBroadcastPerception
  bool WorldRouter::shouldBroadcastPerception(const Atlas::Objects::Operation::RootOperation &) const
  {
    return false;
  }
#endif //STUB_WorldRouter_shouldBroadcastPerception

#ifndef STUB_WorldRouter_deliverTo
//#define STUB_WorldRouter_deliverTo
  void WorldRouter::deliverTo(const Atlas::Objects::Operation::RootOperation &, Ref<LocatedEntity>)
  {
    
  }
#endif //STUB_WorldRouter_deliverTo

#ifndef STUB_WorldRouter_resumeWorld
//#define STUB_WorldRouter_resumeWorld
  void WorldRouter::resumeWorld()
  {
    
  }
#endif //STUB_WorldRouter_resumeWorld

#ifndef STUB_WorldRouter_WorldRouter
//#define STUB_WorldRouter_WorldRouter
   WorldRouter::WorldRouter(const SystemTime & systemTime, Ref<LocatedEntity> baseEntity)
    : BaseWorld(systemTime, baseEntity)
  {
    
  }
#endif //STUB_WorldRouter_WorldRouter

#ifndef STUB_WorldRouter_WorldRouter_DTOR
//#define STUB_WorldRouter_WorldRouter_DTOR
   WorldRouter::~WorldRouter()
  {
    
  }
#endif //STUB_WorldRouter_WorldRouter_DTOR

#ifndef STUB_WorldRouter_idle
//#define STUB_WorldRouter_idle
  bool WorldRouter::idle()
  {
    return false;
  }
#endif //STUB_WorldRouter_idle

#ifndef STUB_WorldRouter_secondsUntilNextOp
//#define STUB_WorldRouter_secondsUntilNextOp
  double WorldRouter::secondsUntilNextOp() const
  {
    return 0;
  }
#endif //STUB_WorldRouter_secondsUntilNextOp

#ifndef STUB_WorldRouter_addEntity
//#define STUB_WorldRouter_addEntity
  Ref<LocatedEntity> WorldRouter::addEntity(const Ref<LocatedEntity>& obj)
  {
    return *static_cast<Ref<LocatedEntity>*>(nullptr);
  }
#endif //STUB_WorldRouter_addEntity

#ifndef STUB_WorldRouter_addNewEntity
//#define STUB_WorldRouter_addNewEntity
  Ref<LocatedEntity> WorldRouter::addNewEntity(const std::string & type, const Atlas::Objects::Entity::RootEntity &)
  {
    return *static_cast<Ref<LocatedEntity>*>(nullptr);
  }
#endif //STUB_WorldRouter_addNewEntity

#ifndef STUB_WorldRouter_delEntity
//#define STUB_WorldRouter_delEntity
  void WorldRouter::delEntity(LocatedEntity * obj)
  {
    
  }
#endif //STUB_WorldRouter_delEntity

#ifndef STUB_WorldRouter_createSpawnPoint
//#define STUB_WorldRouter_createSpawnPoint
  int WorldRouter::createSpawnPoint(const Atlas::Message::MapType &, LocatedEntity *)
  {
    return 0;
  }
#endif //STUB_WorldRouter_createSpawnPoint

#ifndef STUB_WorldRouter_removeSpawnPoint
//#define STUB_WorldRouter_removeSpawnPoint
  int WorldRouter::removeSpawnPoint(LocatedEntity * ent)
  {
    return 0;
  }
#endif //STUB_WorldRouter_removeSpawnPoint

#ifndef STUB_WorldRouter_getSpawnList
//#define STUB_WorldRouter_getSpawnList
  int WorldRouter::getSpawnList(Atlas::Message::ListType & data)
  {
    return 0;
  }
#endif //STUB_WorldRouter_getSpawnList

#ifndef STUB_WorldRouter_spawnNewEntity
//#define STUB_WorldRouter_spawnNewEntity
  LocatedEntity* WorldRouter::spawnNewEntity(const std::string &, const std::string &, const Atlas::Objects::Entity::RootEntity &)
  {
    return nullptr;
  }
#endif //STUB_WorldRouter_spawnNewEntity

#ifndef STUB_WorldRouter_moveToSpawn
//#define STUB_WorldRouter_moveToSpawn
  int WorldRouter::moveToSpawn(const std::string & name, Location& location)
  {
    return 0;
  }
#endif //STUB_WorldRouter_moveToSpawn

#ifndef STUB_WorldRouter_newArithmetic
//#define STUB_WorldRouter_newArithmetic
  ArithmeticScript* WorldRouter::newArithmetic(const std::string &, LocatedEntity *)
  {
    return nullptr;
  }
#endif //STUB_WorldRouter_newArithmetic

#ifndef STUB_WorldRouter_operation
//#define STUB_WorldRouter_operation
  void WorldRouter::operation(const Atlas::Objects::Operation::RootOperation &, Ref<LocatedEntity>)
  {
    
  }
#endif //STUB_WorldRouter_operation

#ifndef STUB_WorldRouter_addPerceptive
//#define STUB_WorldRouter_addPerceptive
  void WorldRouter::addPerceptive(LocatedEntity *)
  {
    
  }
#endif //STUB_WorldRouter_addPerceptive

#ifndef STUB_WorldRouter_message
//#define STUB_WorldRouter_message
  void WorldRouter::message(const Atlas::Objects::Operation::RootOperation &, LocatedEntity &)
  {
    
  }
#endif //STUB_WorldRouter_message

#ifndef STUB_WorldRouter_messageToClients
//#define STUB_WorldRouter_messageToClients
  void WorldRouter::messageToClients(const Atlas::Objects::Operation::RootOperation &)
  {
    
  }
#endif //STUB_WorldRouter_messageToClients

#ifndef STUB_WorldRouter_findByName
//#define STUB_WorldRouter_findByName
  LocatedEntity* WorldRouter::findByName(const std::string & name)
  {
    return nullptr;
  }
#endif //STUB_WorldRouter_findByName

#ifndef STUB_WorldRouter_findByType
//#define STUB_WorldRouter_findByType
  LocatedEntity* WorldRouter::findByType(const std::string & type)
  {
    return nullptr;
  }
#endif //STUB_WorldRouter_findByType

#ifndef STUB_WorldRouter_isQueueDirty
//#define STUB_WorldRouter_isQueueDirty
  bool WorldRouter::isQueueDirty() const
  {
    return false;
  }
#endif //STUB_WorldRouter_isQueueDirty

#ifndef STUB_WorldRouter_markQueueAsClean
//#define STUB_WorldRouter_markQueueAsClean
  void WorldRouter::markQueueAsClean()
  {
    
  }
#endif //STUB_WorldRouter_markQueueAsClean

#ifndef STUB_WorldRouter_getDefaultLocation
//#define STUB_WorldRouter_getDefaultLocation
  LocatedEntity& WorldRouter::getDefaultLocation() const
  {
    return *static_cast<LocatedEntity*>(nullptr);
  }
#endif //STUB_WorldRouter_getDefaultLocation


#endif