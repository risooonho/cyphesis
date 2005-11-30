// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2004 Alistair Riddoch

#ifndef COMMON_PROPERTY_H
#define COMMON_PROPERTY_H

#include <Atlas/Message/Element.h>
#include <Atlas/Objects/ObjectsFwd.h>

#include <sigc++/object.h>
#include <sigc++/signal.h>

/// \brief Interface for Entity properties
class PropertyBase {
  protected:
    /// \brief Flags indicating how this Property should be handled
    const unsigned int m_flags;
    explicit PropertyBase(unsigned int);
  public:
    virtual ~PropertyBase();

    /// \brief Accessor for Property flags
    unsigned int flags() const { return m_flags; }

    /// \brief Copy the value of the property into an Atlas Message
    virtual void get(Atlas::Message::Element &) const = 0;
    /// \brief Read the value of the property from an Atlas Message
    virtual void set(const Atlas::Message::Element &) = 0;
    /// \brief Add the value as an attribute to an Atlas map
    virtual void add(const std::string &, Atlas::Message::MapType & map) const;
    /// \brief Add the value as an attribute to an Atlas entity
    virtual void add(const std::string &, const Atlas::Objects::Entity::RootEntity &) const;
};

/// \brief Entity property template for properties with single data values
/// that cannot be modified directly.
///
/// Properties like CONTAINS, LOC and POS are accessed this way, as they
/// are only ever modified as a result of a move operation.
template <typename T>
class ImmutableProperty : public PropertyBase {
  protected:
    /// \brief Reference to variable holding the value of this Property
    const T & m_data;
  public:
    explicit ImmutableProperty(const T & data, unsigned int flags = 0);

    virtual void get(Atlas::Message::Element &) const;
    virtual void set(const Atlas::Message::Element &);
    virtual void add(const std::string &, Atlas::Message::MapType & map) const;
    virtual void add(const std::string &, const Atlas::Objects::Entity::RootEntity &) const;
};

/// \brief Entity property template for properties with single data values
template <typename T>
class Property : public ImmutableProperty<T> {
  protected:
    /// \brief Reference to variable holding the value of this Property
    T & m_modData;
  public:
    explicit Property(T & data, unsigned int flags);

    virtual void set(const Atlas::Message::Element &);
};

/// \brief Entity property template for properties with single data values
template <typename T>
class SignalProperty : public Property<T>, virtual public SigC::Object {
  public:
    explicit SignalProperty(T & data, unsigned int flags);

    virtual void set(const Atlas::Message::Element &);

    SigC::Signal0<void> modified;
};

#endif // COMMON_PROPERTY_H
