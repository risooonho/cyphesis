// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2005 Alistair Riddoch
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

#include "common/AtlasFileLoader.h"

#include "common/log.h"

#include <Atlas/Message/Element.h>
#include <Atlas/Codecs/XML.h>

using Atlas::Message::Element;
using Atlas::Message::MapType;

void AtlasFileLoader::messageArrived(const MapType & msg)
{
    MapType o = msg;
    MapType::iterator I = o.find("id");
    if (I == o.end()) {
        log(WARNING, "Message without ID read from file");
        return;
    }
    Element & id = I->second;
    if (!id.isString()) {
        log(WARNING, "Message without non-string ID read from file");
        return;
    }
    std::string msg_id = id.String();
    o.erase(I);
    m_messages[msg_id] = o;
    ++m_count;
}

AtlasFileLoader::AtlasFileLoader(const std::string & filename,
                                 MapType & m) :
                m_file(filename.c_str(), std::ios::in),
                m_count(0), m_messages(m)
{
    m_codec = new Atlas::Codecs::XML(m_file, *this);
}
