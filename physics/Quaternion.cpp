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


#include "Quaternion.h"

#include "Vector3D.h"

#include "common/log.h"

#include <wfmath/quaternion.h>
#include <wfmath/error.h>

template<class V>
const Quaternion quaternionFromTo(const V & from, const V & to)
{
    Quaternion q;
    try {
        q.rotation(from, to);
    } catch (const WFMath::ColinearVectors<3>& e) {
        //The vectors are parallel, which causes an error in WFMath. Just ignore here.
    }
    return q;
}

template
const Quaternion quaternionFromTo<Vector3D>(const Vector3D &, const Vector3D&);
