// AUTOGENERATED file, created by the tool generate_stub.py, don't edit!
// If you want to add your own functionality, instead edit the stubPropertyRuleHandler_custom.h file.

#include "server/PropertyRuleHandler.h"
#include "stubPropertyRuleHandler_custom.h"

#ifndef STUB_SERVER_PROPERTYRULEHANDLER_H
#define STUB_SERVER_PROPERTYRULEHANDLER_H

#ifndef STUB_PropertyRuleHandler_check
//#define STUB_PropertyRuleHandler_check
  int PropertyRuleHandler::check(const Atlas::Objects::Root & desc)
  {
    return 0;
  }
#endif //STUB_PropertyRuleHandler_check

#ifndef STUB_PropertyRuleHandler_install
//#define STUB_PropertyRuleHandler_install
  int PropertyRuleHandler::install(const std::string &, const std::string &, const Atlas::Objects::Root & desc, std::string &, std::string &, std::map<const TypeNode*, TypeNode::PropertiesUpdate>& changes)
  {
    return 0;
  }
#endif //STUB_PropertyRuleHandler_install

#ifndef STUB_PropertyRuleHandler_update
//#define STUB_PropertyRuleHandler_update
  int PropertyRuleHandler::update(const std::string &, const Atlas::Objects::Root & desc, std::map<const TypeNode*, TypeNode::PropertiesUpdate>& changes)
  {
    return 0;
  }
#endif //STUB_PropertyRuleHandler_update


#endif