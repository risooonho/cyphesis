// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2000,2001 Alistair Riddoch
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


#ifndef RULESETS_CHARACTER_H
#define RULESETS_CHARACTER_H

#include "Thing.h"
#include <sigc++/connection.h>
#include <sigc++/trackable.h>

class ProxyMind;
class ExternalMind;
class Link;
class Movement;
class Task;



/// \brief This is the class for anything controlled by an AI mind, or
/// external controller like a player client.
///
/// It currently contains large amounts of code for handling movement
/// which should probably be moved into Thing (PhysicalEntity).
/// This class currently assumes it is used to handle all animals,
/// so handles food, nourishment and intoxication.
/// The mind interface includes operation processing for operations from
/// the mind or client to determine what the result on the world is, and
/// operation verification for checking if an in-game operation should be
/// passed to the mind.
/// \ingroup EntityClasses
class Character : public Thing, public virtual sigc::trackable {

  public:

    /// \brief Stores relay data, as generated by a Relay operation.
    ///
    /// Whenever a Relay operation is used it's with the intention that any response
    /// should be relayed back to the originating entity (using serialno and refno).
    /// This struct is used to keep track of this routing data, so that the operation
    /// can be correctly sent on to its destination.
    struct Relay
    {
            /// The entity/router id to which the op was sent or should be sent.
            std::string destination;

            /// The original serial no which the relayed operation should have.
            long int serialno;
    };


  protected:
    /// \brief Internal AI mind keeping track of what this character experiences.
    Ref<ProxyMind> m_proxyMind;


    /// \brief Energy loss by metabolism per tick
    static const double energyConsumption;
    /// \brief Food consumed by digestion per tick
    static const double foodConsumption;
    /// \brief Proportion of weight converted to energy per tick when starving
    static const double weightConsumption;
    /// \brief Energy converted to weight by metabolism per tick
    static const double energyLaidDown;
    /// \brief Weight gained from excess energy by metabolism per tick
    static const double weightGain;

    /// \brief Holds a connection to the containered signal of any wielded entity.
    ///
    /// FIXME This is a hack, to be removed once we've migrated to using Outfit
    /// for wielded entities.
    sigc::connection m_rightHandWieldConnection;

    void metabolise(OpVector &, double ammount = 1);
    void wieldDropped();
    LocatedEntity * findInContains(LocatedEntity * ent, const std::string & id);
    LocatedEntity * findInInventory(const std::string & id);

    friend class Movement;
  public:

    explicit Character(const std::string & id, long intId);
    ~Character() override;

    void destroy() override;

    void NourishOperation(const Operation & op, OpVector &) override;
    void WieldOperation(const Operation & op, OpVector &) override;


    friend class Charactertest;
};

#endif // RULESETS_CHARACTER_H
