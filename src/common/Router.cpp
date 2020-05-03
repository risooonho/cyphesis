// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2008 Alistair Riddoch
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA


#include "Router.h"

#include "compose.hpp"
#include "log.h"

#include <Atlas/Objects/Anonymous.h>
#include <Atlas/Objects/Operation.h>

Router::Router(std::string id, long intId) :
        m_id(std::move(id)),
        m_intId(intId)
{
}

Router::~Router() = default;

void Router::buildError(const Operation& op,
                        const std::string& errstring,
                        const Operation& e,
                        const std::string& to) const
{
    std::vector<Atlas::Objects::Root>& args = e->modifyArgs();

    Atlas::Objects::Entity::Anonymous arg1;
    arg1->setAttr("message", errstring);
    args.push_back(arg1);
    args.push_back(op);

    if (!to.empty()) {
        if (!op->isDefaultSerialno()) {
            e->setRefno(op->getSerialno());
        }
        e->setTo(to);
    }
}

/// \brief Report an Error.
///
/// The error reported is noted in the log, and an error operation is
/// genereated.
/// @param op The operation that caused the error.
/// @param errstring A message describing the error.
/// @param res The resulting error operation is returned here.
/// @param to The error operation should be directed to this ID.
void Router::error(const Operation& op,
                   const std::string& errstring,
                   OpVector& res,
                   const std::string& to) const
{
    Atlas::Objects::Operation::Error e;

    log(NOTICE, String::compose("ERROR generated by router with '%1' with message \"%2\"",
                                getId(), errstring));

    buildError(op, errstring, e, to);

    res.push_back(e);
}

/// \brief Report an Error to a client.
///
/// The error reported generates an error operation.
/// This is used instead of error() when an event occurs which is of no
/// interest to the server admin, or world builder, and should only be
/// be reported to the client. It stops the logs from getting filled
/// with reports of authentication failures, and other similar occurences.
/// @param op The operation that caused the error.
/// @param errstring A message describing the error.
/// @param res The resulting error operation is returned here.
/// @param to The error operation should be directed to this ID.
void Router::clientError(const Operation& op,
                         const std::string& errstring,
                         OpVector& res,
                         const std::string& to) const
{
    Atlas::Objects::Operation::Error e;

    buildError(op, errstring, e, to);

    res.push_back(e);
}

/// \brief Process an operation from an external source
///
/// The ownership of the operation passed in at this point is handed
/// over to the router. The calling code must not modify the operation
/// after passing it to here, or expect the attributes
/// of the operaration to remain the same.
/// @param op The operation to be processed.
void Router::externalOperation(const Operation& op, Link&)
{
}

/// \brief Dispatch an operation that is to this object
void Router::operation(const Operation& op, OpVector& res)
{
}

/// \brief Copy the attribute values of this object to an Atlas Message
void Router::addToMessage(Atlas::Message::MapType& omap) const
{
    omap["objtype"] = "obj";
    omap["id"] = getId();
}

/// \brief Copy the attribute values of this object to an Atlas Entity
void Router::addToEntity(const Atlas::Objects::Entity::RootEntity& ent) const
{
    ent->setObjtype("obj");
    ent->setId(getId());
}
