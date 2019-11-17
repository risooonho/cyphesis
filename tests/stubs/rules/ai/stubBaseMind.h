// AUTOGENERATED file, created by the tool generate_stub.py, don't edit!
// If you want to add your own functionality, instead edit the stubBaseMind_custom.h file.

#ifndef STUB_RULES_AI_BASEMIND_H
#define STUB_RULES_AI_BASEMIND_H

#include "rules/ai/BaseMind.h"
#include "stubBaseMind_custom.h"

#ifndef STUB_BaseMind_BaseMind
//#define STUB_BaseMind_BaseMind
   BaseMind::BaseMind(const std::string & mindId, std::string entityId)
    : Router(mindId, entityId)
    , m_scriptFactory(nullptr)
  {
    
  }
#endif //STUB_BaseMind_BaseMind

#ifndef STUB_BaseMind_BaseMind_DTOR
//#define STUB_BaseMind_BaseMind_DTOR
   BaseMind::~BaseMind()
  {
    
  }
#endif //STUB_BaseMind_BaseMind_DTOR

#ifndef STUB_BaseMind_init
//#define STUB_BaseMind_init
  void BaseMind::init(OpVector& res)
  {
    
  }
#endif //STUB_BaseMind_init

#ifndef STUB_BaseMind_destroy
//#define STUB_BaseMind_destroy
  void BaseMind::destroy()
  {
    
  }
#endif //STUB_BaseMind_destroy

#ifndef STUB_BaseMind_getTypeStore
//#define STUB_BaseMind_getTypeStore
  const TypeStore& BaseMind::getTypeStore() const
  {
    return *static_cast<const TypeStore*>(nullptr);
  }
#endif //STUB_BaseMind_getTypeStore

#ifndef STUB_BaseMind_sightCreateOperation
//#define STUB_BaseMind_sightCreateOperation
  void BaseMind::sightCreateOperation(const Operation &, OpVector &)
  {
    
  }
#endif //STUB_BaseMind_sightCreateOperation

#ifndef STUB_BaseMind_sightDeleteOperation
//#define STUB_BaseMind_sightDeleteOperation
  void BaseMind::sightDeleteOperation(const Operation &, OpVector &)
  {
    
  }
#endif //STUB_BaseMind_sightDeleteOperation

#ifndef STUB_BaseMind_sightMoveOperation
//#define STUB_BaseMind_sightMoveOperation
  void BaseMind::sightMoveOperation(const Operation &, OpVector &)
  {
    
  }
#endif //STUB_BaseMind_sightMoveOperation

#ifndef STUB_BaseMind_sightSetOperation
//#define STUB_BaseMind_sightSetOperation
  void BaseMind::sightSetOperation(const Operation &, OpVector &)
  {
    
  }
#endif //STUB_BaseMind_sightSetOperation

#ifndef STUB_BaseMind_addPropertyScriptCallback
//#define STUB_BaseMind_addPropertyScriptCallback
  void BaseMind::addPropertyScriptCallback(std::string propertyName, std::string scriptMethod)
  {
    
  }
#endif //STUB_BaseMind_addPropertyScriptCallback

#ifndef STUB_BaseMind_operation
//#define STUB_BaseMind_operation
  void BaseMind::operation(const Operation &, OpVector &)
  {
    
  }
#endif //STUB_BaseMind_operation

#ifndef STUB_BaseMind_externalOperation
//#define STUB_BaseMind_externalOperation
  void BaseMind::externalOperation(const Operation & op, Link &)
  {
    
  }
#endif //STUB_BaseMind_externalOperation

#ifndef STUB_BaseMind_SightOperation
//#define STUB_BaseMind_SightOperation
  void BaseMind::SightOperation(const Operation &, OpVector &)
  {
    
  }
#endif //STUB_BaseMind_SightOperation

#ifndef STUB_BaseMind_SoundOperation
//#define STUB_BaseMind_SoundOperation
  void BaseMind::SoundOperation(const Operation &, OpVector &)
  {
    
  }
#endif //STUB_BaseMind_SoundOperation

#ifndef STUB_BaseMind_AppearanceOperation
//#define STUB_BaseMind_AppearanceOperation
  void BaseMind::AppearanceOperation(const Operation &, OpVector &)
  {
    
  }
#endif //STUB_BaseMind_AppearanceOperation

#ifndef STUB_BaseMind_DisappearanceOperation
//#define STUB_BaseMind_DisappearanceOperation
  void BaseMind::DisappearanceOperation(const Operation &, OpVector &)
  {
    
  }
#endif //STUB_BaseMind_DisappearanceOperation

#ifndef STUB_BaseMind_UnseenOperation
//#define STUB_BaseMind_UnseenOperation
  void BaseMind::UnseenOperation(const Operation &, OpVector &)
  {
    
  }
#endif //STUB_BaseMind_UnseenOperation

#ifndef STUB_BaseMind_ThinkOperation
//#define STUB_BaseMind_ThinkOperation
  void BaseMind::ThinkOperation(const Operation &, OpVector &)
  {
    
  }
#endif //STUB_BaseMind_ThinkOperation

#ifndef STUB_BaseMind_InfoOperation
//#define STUB_BaseMind_InfoOperation
  void BaseMind::InfoOperation(const Operation&, OpVector&)
  {
    
  }
#endif //STUB_BaseMind_InfoOperation

#ifndef STUB_BaseMind_callSightOperation
//#define STUB_BaseMind_callSightOperation
  void BaseMind::callSightOperation(const Operation &, OpVector &)
  {
    
  }
#endif //STUB_BaseMind_callSightOperation

#ifndef STUB_BaseMind_callSoundOperation
//#define STUB_BaseMind_callSoundOperation
  void BaseMind::callSoundOperation(const Operation &, OpVector &)
  {
    
  }
#endif //STUB_BaseMind_callSoundOperation

#ifndef STUB_BaseMind_setScript
//#define STUB_BaseMind_setScript
  void BaseMind::setScript(std::unique_ptr<Script> scrpt)
  {
    
  }
#endif //STUB_BaseMind_setScript

#ifndef STUB_BaseMind_setOwnEntity
//#define STUB_BaseMind_setOwnEntity
  void BaseMind::setOwnEntity(OpVector& res, Ref<MemEntity> ownEntity)
  {
    
  }
#endif //STUB_BaseMind_setOwnEntity


#endif