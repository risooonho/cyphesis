// AUTOGENERATED file, created by the tool generate_stub.py, don't edit!
// If you want to add your own functionality, instead edit the stubSpawn_custom.h file.

#include "server/Spawn.h"
#include "stubSpawn_custom.h"

#ifndef STUB_SERVER_SPAWN_H
#define STUB_SERVER_SPAWN_H

#ifndef STUB_Spawn_spawnEntity
//#define STUB_Spawn_spawnEntity
  int Spawn::spawnEntity(const std::string & type, const Atlas::Objects::Entity::RootEntity & dsc) const
  {
    return 0;
  }
#endif //STUB_Spawn_spawnEntity

#ifndef STUB_Spawn_addToMessage
//#define STUB_Spawn_addToMessage
  int Spawn::addToMessage(Atlas::Message::MapType & msg) const
  {
    return 0;
  }
#endif //STUB_Spawn_addToMessage

#ifndef STUB_Spawn_placeInSpawn
//#define STUB_Spawn_placeInSpawn
  int Spawn::placeInSpawn(Location& location) const
  {
    return 0;
  }
#endif //STUB_Spawn_placeInSpawn


#endif