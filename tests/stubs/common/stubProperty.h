// AUTOGENERATED file, created by the tool generate_stub.py, don't edit!
// If you want to add your own functionality, instead edit the stubProperty_custom.h file.

#ifndef STUB_COMMON_PROPERTY_H
#define STUB_COMMON_PROPERTY_H

#include "common/Property.h"
#include "stubProperty_custom.h"

#ifndef STUB_PropertyUtil_flagsForPropertyName
//#define STUB_PropertyUtil_flagsForPropertyName
   std::uint32_t PropertyUtil::flagsForPropertyName(const std::string& name)
  {
    return 0;
  }
#endif //STUB_PropertyUtil_flagsForPropertyName

#ifndef STUB_PropertyUtil_isValidName
//#define STUB_PropertyUtil_isValidName
   bool PropertyUtil::isValidName(const std::string& name)
  {
    return false;
  }
#endif //STUB_PropertyUtil_isValidName

#ifndef STUB_PropertyUtil_parsePropertyModification
//#define STUB_PropertyUtil_parsePropertyModification
   std::pair<ModifierType, std::string> PropertyUtil::parsePropertyModification(const std::string& propertyName)
  {
    return *static_cast< std::pair<ModifierType, std::string>*>(nullptr);
  }
#endif //STUB_PropertyUtil_parsePropertyModification


#ifndef STUB_PropertyCore_PropertyCore
//#define STUB_PropertyCore_PropertyCore
  template <typename EntityT>
   PropertyCore<EntityT>::PropertyCore(std::uint32_t flags )
    : OperationsListener(flags)
  {
    
  }
#endif //STUB_PropertyCore_PropertyCore

#ifndef STUB_PropertyCore_install
//#define STUB_PropertyCore_install
  template <typename EntityT>
  void PropertyCore<EntityT>::install(EntityT *, const std::string &)
  {
    
  }
#endif //STUB_PropertyCore_install

#ifndef STUB_PropertyCore_install
//#define STUB_PropertyCore_install
  template <typename EntityT>
  void PropertyCore<EntityT>::install(TypeNode *, const std::string &)
  {
    
  }
#endif //STUB_PropertyCore_install

#ifndef STUB_PropertyCore_remove
//#define STUB_PropertyCore_remove
  template <typename EntityT>
  void PropertyCore<EntityT>::remove(EntityT *, const std::string & name)
  {
    
  }
#endif //STUB_PropertyCore_remove

#ifndef STUB_PropertyCore_apply
//#define STUB_PropertyCore_apply
  template <typename EntityT>
  void PropertyCore<EntityT>::apply(EntityT *)
  {
    
  }
#endif //STUB_PropertyCore_apply

#ifndef STUB_PropertyCore_get
//#define STUB_PropertyCore_get
  template <typename EntityT>
  int PropertyCore<EntityT>::get(Atlas::Message::Element & val) const
  {
    return 0;
  }
#endif //STUB_PropertyCore_get

#ifndef STUB_PropertyCore_set
//#define STUB_PropertyCore_set
  template <typename EntityT>
  void PropertyCore<EntityT>::set(const Atlas::Message::Element & val)
  {
    
  }
#endif //STUB_PropertyCore_set

#ifndef STUB_PropertyCore_add
//#define STUB_PropertyCore_add
  template <typename EntityT>
  void PropertyCore<EntityT>::add(const std::string & key, Atlas::Message::MapType & map) const
  {
    
  }
#endif //STUB_PropertyCore_add

#ifndef STUB_PropertyCore_add
//#define STUB_PropertyCore_add
  template <typename EntityT>
  void PropertyCore<EntityT>::add(const std::string & key, const Atlas::Objects::Entity::RootEntity & ent) const
  {
    
  }
#endif //STUB_PropertyCore_add

#ifndef STUB_PropertyCore_operation
//#define STUB_PropertyCore_operation
  template <typename EntityT>
  HandlerResult PropertyCore<EntityT>::operation(LocatedEntity *, const Operation &, OpVector &)
  {
    return *static_cast<HandlerResult*>(nullptr);
  }
#endif //STUB_PropertyCore_operation

#ifndef STUB_PropertyCore_copy
//#define STUB_PropertyCore_copy
  template <typename EntityT>
  PropertyCore* PropertyCore<EntityT>::copy() const
  {
    return nullptr;
  }
#endif //STUB_PropertyCore_copy

#ifndef STUB_PropertyCore_operator_EQUALS
//#define STUB_PropertyCore_operator_EQUALS
  template <typename EntityT>
  bool PropertyCore<EntityT>::operator==(const PropertyCore& rhs) const
  {
    return false;
  }
#endif //STUB_PropertyCore_operator_EQUALS


#ifndef STUB_Property_Property
//#define STUB_Property_Property
  template <typename T>
   Property<T>::Property(unsigned int flags )
    : PropertyBase(flags)
  {
    
  }
#endif //STUB_Property_Property

#ifndef STUB_Property_get
//#define STUB_Property_get
  template <typename T>
  int Property<T>::get(Atlas::Message::Element & val) const
  {
    return 0;
  }
#endif //STUB_Property_get

#ifndef STUB_Property_set
//#define STUB_Property_set
  template <typename T>
  void Property<T>::set(const Atlas::Message::Element &)
  {
    
  }
#endif //STUB_Property_set

#ifndef STUB_Property_add
//#define STUB_Property_add
  template <typename T>
  void Property<T>::add(const std::string & key, Atlas::Message::MapType & map) const
  {
    
  }
#endif //STUB_Property_add

#ifndef STUB_Property_add
//#define STUB_Property_add
  template <typename T>
  void Property<T>::add(const std::string & key, const Atlas::Objects::Entity::RootEntity & ent) const
  {
    
  }
#endif //STUB_Property_add

#ifndef STUB_Property_copy
//#define STUB_Property_copy
  template <typename T>
  Property<T>* Property<T>::copy() const
  {
    return nullptr;
  }
#endif //STUB_Property_copy


#ifndef STUB_SoftProperty_SoftProperty
//#define STUB_SoftProperty_SoftProperty
   SoftProperty::SoftProperty(Atlas::Message::Element  data)
    : PropertyBase(data)
  {
    
  }
#endif //STUB_SoftProperty_SoftProperty

#ifndef STUB_SoftProperty_get
//#define STUB_SoftProperty_get
  int SoftProperty::get(Atlas::Message::Element & val) const
  {
    return 0;
  }
#endif //STUB_SoftProperty_get

#ifndef STUB_SoftProperty_set
//#define STUB_SoftProperty_set
  void SoftProperty::set(const Atlas::Message::Element & val)
  {
    
  }
#endif //STUB_SoftProperty_set

#ifndef STUB_SoftProperty_copy
//#define STUB_SoftProperty_copy
  SoftProperty* SoftProperty::copy() const
  {
    return nullptr;
  }
#endif //STUB_SoftProperty_copy

#ifndef STUB_SoftProperty_data
//#define STUB_SoftProperty_data
  Atlas::Message::Element& SoftProperty::data()
  {
    return *static_cast<Atlas::Message::Element*>(nullptr);
  }
#endif //STUB_SoftProperty_data

#ifndef STUB_SoftProperty_data
//#define STUB_SoftProperty_data
  const Atlas::Message::Element& SoftProperty::data() const
  {
    return *static_cast<const Atlas::Message::Element*>(nullptr);
  }
#endif //STUB_SoftProperty_data


#ifndef STUB_BoolProperty_get
//#define STUB_BoolProperty_get
  int BoolProperty::get(Atlas::Message::Element & val) const
  {
    return 0;
  }
#endif //STUB_BoolProperty_get

#ifndef STUB_BoolProperty_set
//#define STUB_BoolProperty_set
  void BoolProperty::set(const Atlas::Message::Element & val)
  {
    
  }
#endif //STUB_BoolProperty_set

#ifndef STUB_BoolProperty_copy
//#define STUB_BoolProperty_copy
  BoolProperty* BoolProperty::copy() const
  {
    return nullptr;
  }
#endif //STUB_BoolProperty_copy

#ifndef STUB_BoolProperty_isTrue
//#define STUB_BoolProperty_isTrue
  bool BoolProperty::isTrue() const
  {
    return false;
  }
#endif //STUB_BoolProperty_isTrue


#endif