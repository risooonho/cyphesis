// AUTOGENERATED file, created by the tool generate_stub.py, don't edit!
// If you want to add your own functionality, instead edit the stubDecaysProperty_custom.h file.

#include "rulesets/DecaysProperty.h"
#include "stubDecaysProperty_custom.h"

#ifndef STUB_RULESETS_DECAYSPROPERTY_H
#define STUB_RULESETS_DECAYSPROPERTY_H

#ifndef STUB_DecaysProperty_install
//#define STUB_DecaysProperty_install
  void DecaysProperty::install(LocatedEntity*, const std::string&)
  {
    
  }
#endif //STUB_DecaysProperty_install

#ifndef STUB_DecaysProperty_remove
//#define STUB_DecaysProperty_remove
  void DecaysProperty::remove(LocatedEntity*, const std::string&)
  {
    
  }
#endif //STUB_DecaysProperty_remove

#ifndef STUB_DecaysProperty_operation
//#define STUB_DecaysProperty_operation
  HandlerResult DecaysProperty::operation(LocatedEntity*, const Operation&, OpVector&)
  {
    return *static_cast<HandlerResult*>(nullptr);
  }
#endif //STUB_DecaysProperty_operation

#ifndef STUB_DecaysProperty_copy
//#define STUB_DecaysProperty_copy
  DecaysProperty* DecaysProperty::copy() const
  {
    return nullptr;
  }
#endif //STUB_DecaysProperty_copy

#ifndef STUB_DecaysProperty_del_handler
//#define STUB_DecaysProperty_del_handler
  HandlerResult DecaysProperty::del_handler(LocatedEntity* e, const Operation&, OpVector& res)
  {
    return *static_cast<HandlerResult*>(nullptr);
  }
#endif //STUB_DecaysProperty_del_handler


#endif