// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2000,2001 Alistair Riddoch

#include "Persistance.h"

#include "Admin.h"
#include "Player.h"

#include "rulesets/Entity.h"

#include "common/log.h"
#include "common/const.h"
#include "common/Database.h"

Persistance * Persistance::m_instance = NULL;

Persistance::Persistance() : m_connection(*Database::instance())
{
}

Persistance * Persistance::instance()
{
    if (m_instance == NULL) {
        m_instance = new Persistance();
    }
    return m_instance;
}

bool Persistance::init()
{
    Persistance * p = instance();
    if (!p->m_connection.initConnection(false)) {
        return false;
    }
    bool i = p->m_connection.initRule(true);

    if (!p->m_connection.registerEntityIdGenerator()) {
        log(ERROR, "Faled to register Id generator in database.");
    }

    Element::MapType tableDesc;
    tableDesc["username"] = "                                                                                ";
    tableDesc["password"] = "                                                                                ";
    tableDesc["type"] = "          ";
    bool j = p->m_connection.registerSimpleTable("accounts", tableDesc);
    bool k = p->m_connection.registerRelation("character");

    if (!p->findAccount("admin")) {
        std::cout << "BOOTSTRAP ADMIN ACCOUNT" << std::endl << std::flush;
        std::string adminAccountId;
        p->m_connection.getEntityId(adminAccountId);
        
        Admin dummyAdminAccount(0, "admin", consts::defaultAdminPasswordHash,
                                adminAccountId);
        
        p->putAccount(dummyAdminAccount);
    }

    return (i && j && k);
}

void Persistance::shutdown()
{
    Persistance * p = m_instance;
    if (p == NULL) { return; }
    p->m_connection.shutdownConnection();
    delete &p->m_connection;
    delete p;
    m_instance = NULL;
}

bool Persistance::findAccount(const std::string & name)
{
    std::string namestr = "'" + name + "'";
    DatabaseResult dr = m_connection.selectSimpleRowBy("accounts", "username", namestr);
    if (dr.error()) {
        log(ERROR, "Failure while find account.");
        return false;
    }
    if (dr.empty()) {
        dr.clear();
        return false;
    }
    if (dr.size() > 1) {
        log(ERROR, "Duplicate username in accounts database.");
    }
    dr.clear();
    return true;
}

Account * Persistance::getAccount(const std::string & name)
{
    std::string namestr = "'" + name + "'";
    DatabaseResult dr = m_connection.selectSimpleRowBy("accounts", "username", namestr);
    if (dr.error()) {
        log(ERROR, "Failure while find account.");
        return 0;
    }
    if (dr.empty()) {
        dr.clear();
        return 0;
    }
    if (dr.size() > 1) {
        log(ERROR, "Duplicate username in accounts database.");
    }
    const char * c = dr.field("id");
    if (c == 0) {
        dr.clear();
        log(ERROR, "Unable to find id field in accounts database.");
        return 0;
    }
    std::string id = c;
    c = dr.field("password");
    if (c == 0) {
        dr.clear();
        log(ERROR, "Unable to find password field in accounts database.");
        return 0;
    }
    std::string passwd = c;
    c = dr.field("type");
    if (c == 0) {
        dr.clear();
        log(ERROR, "Unable to find type field in accounts database.");
        return 0;
    }
    std::string type = c;
    dr.clear();
    if (type == "admin") {
        return new Admin(0, name, passwd, id);
    } else {
        return new Player(0, name, passwd, id);
    }
}

void Persistance::putAccount(const Account & ac)
{
    std::string columns = "username, type, password";
    std::string values = "'";
    values += ac.m_username;
    values += "', '";
    values += ac.getType();
    values += "', '";
    values += ac.m_password;
    values += "'";
    m_connection.createSimpleRow("accounts", ac.getId(), columns, values);
}

void Persistance::registerCharacters(Account & ac,
                                     const EntityDict & worldObjects)
{
    DatabaseResult dr = m_connection.selectRelation("character", ac.getId());
    if (dr.error()) {
        log(ERROR, "Failure while find account.");
    }
    DatabaseResult::const_iterator I = dr.begin();
    for(; I != dr.end(); ++I) {
        const char * c = I.column(0);
        if (c == 0) {
            log(ERROR, "No data in relation when examing characters");
            continue;
        }
        std::string id(c);
        EntityDict::const_iterator J = worldObjects.find(id);
        if (J == worldObjects.end()) {
            log(WARNING, "Persistance: Got character id from database which does not exist in world");
            continue;
        }
        ac.addCharacter(J->second);
    }
    dr.clear();
}

void Persistance::addCharacter(const Account & ac, const Entity & e)
{
    m_connection.createRelationRow("character", ac.getId(), e.getId());
}

void Persistance::delCharacter(const std::string & id)
{
    m_connection.removeRelationRowByOther("character", id);
}

bool Persistance::getRules(Element::MapType & m)
{
    return m_connection.getTable(m_connection.rule(), m);
}

bool Persistance::clearRules()
{
    return m_connection.clearTable(m_connection.rule());
}
