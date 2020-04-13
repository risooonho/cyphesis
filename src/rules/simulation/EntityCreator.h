/*
 Copyright (C) 2020 Erik Ogenvik

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef CYPHESIS_ENTITYCREATOR_H
#define CYPHESIS_ENTITYCREATOR_H

class LocatedEntity;
class BaseWorld;

struct EntityCreator
{
    virtual Ref<LocatedEntity> newEntity(const std::string& id,
                                         long intId,
                                         const std::string& type,
                                         const Atlas::Objects::Entity::RootEntity& attrs,
                                         const BaseWorld& world) const = 0;
};

#endif //CYPHESIS_ENTITYCREATOR_H