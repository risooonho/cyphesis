// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2000,2001 Alistair Riddoch

#ifndef COMMON_GENERIC_H
#define COMMON_GENERIC_H

// This operation cannot be instantiated directly, it can only be given
// a parent string in order to create an operation instance with
// parents that none of the hardcoded types support. Operations of
// this type should only be created and interpreted in scripts. If
// operations must be understood by C++ code, then add a new class.

namespace Atlas { namespace Objects { namespace Operation {

class Generic : public RootOperation {
  private:
    Generic(const string & p) : RootOperation(p, "root_operation") {
    }
  public:
    virtual ~Generic() { }
    static Generic Instantiate(const string & p) {
        Generic value(p);
        value.SetParents(Message::Object::ListType(1,p));
        value.SetObjtype(string("op"));
        return value;
    }
};

} } }

#endif // COMMON_GENERIC_H
