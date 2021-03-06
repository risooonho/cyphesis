/*
 Copyright (C) 2014 Erik Ogenvik

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

#include "PhysicalDomain.h"

#include "TerrainProperty.h"
#include "rules/LocatedEntity.h"
#include "PropelProperty.h"
#include "rules/simulation/GeometryProperty.h"
#include "rules/simulation/AngularFactorProperty.h"
#include "TerrainModProperty.h"
#include "PhysicalWorld.h"

#include "physics/Convert.h"

#include "common/debug.h"
#include "common/operations/Tick.h"
#include "rules/simulation/BaseWorld.h"
#include "PerceptionSightProperty.h"
#include "rules/BBoxProperty.h"
#include "rules/SolidProperty.h"
#include "rules/ScaleProperty.h"
#include "SimulationSpeedProperty.h"
#include "ModeDataProperty.h"
#include "VisibilityDistanceProperty.h"
#include "common/Inheritance.h"
#include "Remotery/Remotery.h"

#include <Mercator/Segment.h>
#include <Mercator/TerrainMod.h>

#include <Atlas/Objects/Operation.h>
#include <Atlas/Objects/Anonymous.h>

#include <wfmath/atlasconv.h>

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>

#include <sigc++/bind.h>

#include <memory>
#include <unordered_set>
#include <chrono>
#include <boost/optional.hpp>
#include <algorithm>

static const bool debug_flag = false;

using Atlas::Message::Element;
using Atlas::Message::MapType;
using Atlas::Objects::Root;
using Atlas::Objects::Entity::RootEntity;
using Atlas::Objects::Entity::Anonymous;
using Atlas::Objects::Operation::Set;
using Atlas::Objects::Operation::Sight;
using Atlas::Objects::Operation::Appearance;
using Atlas::Objects::Operation::Disappearance;
using Atlas::Objects::Operation::Move;

using Atlas::Objects::smart_dynamic_cast;

namespace {
    bool fuzzyEquals(WFMath::CoordType a, WFMath::CoordType b, WFMath::CoordType epsilon)
    {
        return std::abs(a - b) < epsilon;
    }

    bool fuzzyEquals(const WFMath::Point<3>& a, const WFMath::Point<3>& b, WFMath::CoordType epsilon)
    {
        return fuzzyEquals(a.x(), b.x(), epsilon) && fuzzyEquals(a.y(), b.y(), epsilon) && fuzzyEquals(a.z(), b.z(), epsilon);
    }

    bool fuzzyEquals(const WFMath::Vector<3>& a, const WFMath::Vector<3>& b, WFMath::CoordType epsilon)
    {
        return fuzzyEquals(a.x(), b.x(), epsilon) && fuzzyEquals(a.y(), b.y(), epsilon) && fuzzyEquals(a.z(), b.z(), epsilon);
    }

    /**
     * Processes an existing set and a new sorted vector, checking which elements are new, which are removed and which are kept.
     * The supplied set will be altered so that it contains the content of the vector.
     * @tparam T The type of items.
     * @tparam FunctorRemove Called when an item was removed.
     * @tparam FunctorAdd Called when an item was added.
     * @param newCollection The vector of new items. Must be sorted.
     * @param previousCollection The set of existing items. This will be altered to match the vector.
     * @param removedFn Called when an item has been removed.
     * @param addedFn Called when an item has been added.
     */
    template<typename T, typename FunctorRemove, typename FunctorAdd>
    void processDifferences(std::vector<T>& newCollection, std::set<T>& previousCollection, FunctorRemove removedFn, FunctorAdd addedFn)
    {
        auto existingI = previousCollection.begin();
        std::sort(newCollection.begin(), newCollection.end());
        auto newI = newCollection.begin();
        while (existingI != previousCollection.end() && newI != newCollection.end()) {
            if (*existingI == *newI) {
                //elements are equal, the entity was already in previousCollection
                //existedFn(*existingI);
                ++existingI;
                ++newI;
            } else if (*existingI < *newI) {
                //an element is missing from the new collection. This entity was removed.
                removedFn(*existingI);
                existingI = previousCollection.erase(existingI);
            } else {
                //an element wasn't present in the existing previousCollection entities. This entity was added.
                addedFn(*newI);
                existingI = previousCollection.insert(*newI).first;
            }
        }
        //Check if there were more entries in the existing collection.
        for (; existingI != previousCollection.end();) {
            removedFn(*existingI);
            existingI = previousCollection.erase(existingI);
        }

        //Check if there were more new entries.
        for (; newI != newCollection.end(); ++newI) {
            addedFn(*newI);
            previousCollection.insert(*newI);
        }
    }

    template<typename T>
    bool removeAndShift(std::vector<T>& collection, const T& entry)
    {
        for (size_t i = 0; i < collection.size(); ++i) {
            //Find the entry in collection and do a quick delete by moving the last entry and decreasing the size.
            if (collection[i] == entry) {
                //If it's the last entry just decrease the size without moving anything.
                if (i < collection.size() - 1) {
                    collection[i] = collection[collection.size() - 1];
                }
                collection.resize(collection.size() - 1);
                return true;
            }
        }
        return false;
    }
}

int PhysicalDomain::s_processTimeUs = 0;

/**
 * The minimum angular resolution of visibility, expressed as degrees.
 *
 * The "angular resolution" describes the minimum size an object needs to be in order to be viewed, as a degree of the
 * "viewing sphere" that would surround an observer.
 */
const double VISIBILITY_ANGULAR_DEGREE = 1.0;

/**
 * The ratio between the size of an object and how far away it can be seen, based on the minimum angular resolution.
 */
const double VISIBILITY_RATIO = 1.0 / std::tan((WFMath::numeric_constants<double>::pi() / 180.0) * VISIBILITY_ANGULAR_DEGREE);

/**
 * How much objects in the visibility world should be scaled.
 *
 * The Bullet system is designed to handle "normal" sized things, measured in meters.
 * The visibility system uses much larger spheres. Therefore we should scale them down to make them better fit the
 * "normal" world.
 * We'll base this on VISIBILITY_RATIO.
 */
const float VISIBILITY_SCALING_FACTOR = 1.0f / VISIBILITY_RATIO;

/**
 * A list of "thresholds" for visibility distance into which any visibility sphere's radius will be slotted into.
 * The main reason is to improve performance, so visibility checks aren't redone each time an entity changes size.
 */
const std::array<double, 10> VISIBILITY_DISTANCE_THRESHOLDS = {10.0, 20, 30, 50, 75, 100, 300, 500, 1000, 2000};

/**
 * The amount of time between each tick, in seconds.
 * The simulation will happen at 60hz, but the tick size determines how often entities are processed and operations are sent.
 */
const double TICK_SIZE = 1.0 / 15.0;

/**
 * The base size of the view sphere for a perceptive entity. Everything within this radius will be visible.
 */
const double VIEW_SPHERE_RADIUS = 0.5;

/**
 * Mask used by visibility checks for observing entries (i.e. creatures etc.).
 */
const short VISIBILITY_MASK_OBSERVER = 1u << 1u;

/**
 * Mask used by visibility checks for entries that can be observed (i.e. most entities).
 */
const short VISIBILITY_MASK_OBSERVABLE = 1u << 2u;

/**
 * Mask used by visibility checks for entries that only can be observed by admin entities.
 */
const short VISIBILITY_MASK_OBSERVABLE_PRIVATE = 1u << 3u;

/**
 * Mask used by all physical items. They should collide with other physical items, and with the terrain.
 */
const short COLLISION_MASK_PHYSICAL = 1;
/**
 * Mask used by the terrain. It's static.
 */
const short COLLISION_MASK_NON_PHYSICAL = 2;
/**
 * Mask used by all non-physical items. These should only collide with the terrain.
 */
const short COLLISION_MASK_TERRAIN = 4;
/**
 * Mask used by static items (i.e. those with mode "fixed" and "planted").
 */
const short COLLISION_MASK_STATIC = 8;

/**
 * The max amount of entities to do visibility checks for each tick.
 */
const size_t VISIBILITY_CHECK_MAX_ENTRIES = 20;

const float CCD_MOTION_FACTOR = 0.2f;

const float CCD_SPHERE_FACTOR = 0.2f;

/**
 * Set on btCollisionObjects which are water bodies.
 */
const int USER_INDEX_WATER_BODY = 1;

struct PhysicalDomain::PhysicalMotionState : public btMotionState
{
    BulletEntry& m_bulletEntry;
    btRigidBody& m_rigidBody;
    PhysicalDomain& m_domain;
    btTransform m_worldTrans;
    btTransform m_centerOfMassOffset;

    PhysicalMotionState(BulletEntry& bulletEntry, btRigidBody& rigidBody, PhysicalDomain& domain,
                        const btTransform& startTrans, const btTransform& centerOfMassOffset = btTransform::getIdentity())
            :
            m_bulletEntry(bulletEntry),
            m_rigidBody(rigidBody),
            m_domain(domain),
            m_worldTrans(startTrans),
            m_centerOfMassOffset(centerOfMassOffset)
    {
    }

    ///synchronizes world transform from user to physics
    void getWorldTransform(btTransform& centerOfMassWorldTrans) const override
    {
        centerOfMassWorldTrans = m_worldTrans * m_centerOfMassOffset.inverse();
    }

    ///synchronizes world transform from physics to user
    ///Bullet only calls the update of worldtransform for active objects
    void setWorldTransform(const btTransform& centerOfMassWorldTrans) override
    {
        if (!(m_bulletEntry.lastTransform == centerOfMassWorldTrans)) {
            m_bulletEntry.lastTransform = centerOfMassWorldTrans;

            LocatedEntity& entity = m_bulletEntry.entity;
            if (!m_bulletEntry.addedToMovingList) {
                m_domain.m_movingEntities.emplace_back(&m_bulletEntry);
                m_bulletEntry.addedToMovingList = true;
                m_bulletEntry.markedAsMovingLastFrame = false;
            }
            m_bulletEntry.markedAsMovingThisFrame = true;
            if (!m_bulletEntry.markedForVisibilityRecalculation) {
                //Mark the entity for visibility recalculation, but don't move the visibility and view sphere here.
                //Instead rely on that being done by the calling code.
                m_domain.m_visibilityRecalculateQueue.emplace_back(&m_bulletEntry);
                m_bulletEntry.markedForVisibilityRecalculation = true;
            }

            //            debug_print(
            //                    "setWorldTransform: "<< m_entity.describeEntity() << " (" << centerOfMassWorldTrans.getOrigin().x() << "," << centerOfMassWorldTrans.getOrigin().y() << "," << centerOfMassWorldTrans.getOrigin().z() << ")");

            auto& bulletTransform = m_rigidBody.getCenterOfMassTransform();
            btTransform newTransform = bulletTransform * m_centerOfMassOffset;

            entity.m_location.m_pos = Convert::toWF<WFMath::Point<3>>(newTransform.getOrigin());
            entity.m_location.m_orientation = Convert::toWF(newTransform.getRotation());
            entity.m_location.m_angularVelocity = Convert::toWF<WFMath::Vector<3>>(m_rigidBody.getAngularVelocity());
            entity.m_location.m_velocity = Convert::toWF<WFMath::Vector<3>>(m_rigidBody.getLinearVelocity());

            //If the magnitude is small enough, consider the velocity to be zero.
            if (entity.m_location.m_velocity.sqrMag() < 0.001f) {
                entity.m_location.m_velocity.zero();
            }
            if (entity.m_location.m_angularVelocity.sqrMag() < 0.001f) {
                entity.m_location.m_angularVelocity.zero();
            }
            entity.removeFlags(entity_pos_clean | entity_orient_clean);
        }
    }
};

struct PhysicalDomain::WaterCollisionCallback : public btOverlappingPairCallback
{
    PhysicalDomain* m_domain;

    void addToWater(BulletEntry* waterEntry, BulletEntry* otherEntry)
    {
        otherEntry->waterNearby = waterEntry;
        if (!otherEntry->addedToMovingList) {
            m_domain->m_movingEntities.emplace_back(otherEntry);
            otherEntry->addedToMovingList = true;
            otherEntry->markedAsMovingThisFrame = false;
            otherEntry->markedAsMovingLastFrame = false;
        }
    }

    void removeFromWater(BulletEntry* waterEntry, BulletEntry* otherEntry)
    {
        if (otherEntry->waterNearby == waterEntry) {
            otherEntry->waterNearby = otherEntry;
            if (!otherEntry->addedToMovingList) {
                m_domain->m_movingEntities.emplace_back(otherEntry);
                otherEntry->addedToMovingList = true;
                otherEntry->markedAsMovingThisFrame = false;
                otherEntry->markedAsMovingLastFrame = false;
            }
        }
    }

    virtual btBroadphasePair* addOverlappingPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1)
    {
        btCollisionObject* colObj0 = (btCollisionObject*) proxy0->m_clientObject;
        btCollisionObject* colObj1 = (btCollisionObject*) proxy1->m_clientObject;
        if (colObj0->getUserIndex() == USER_INDEX_WATER_BODY) {
            //Don't let two water bodies affect each other
            if (colObj1->getUserIndex() != USER_INDEX_WATER_BODY) {
                auto waterBodyEntry = (BulletEntry*) colObj0->getUserPointer();
                auto otherEntry = (BulletEntry*) colObj1->getUserPointer();
                addToWater(waterBodyEntry, otherEntry);
            }
        } else if (colObj1->getUserIndex() == USER_INDEX_WATER_BODY) {
            auto waterBodyEntry = (BulletEntry*) colObj1->getUserPointer();
            auto otherEntry = (BulletEntry*) colObj0->getUserPointer();
            addToWater(waterBodyEntry, otherEntry);
        }
        return 0;
    }

    virtual void* removeOverlappingPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1, btDispatcher* dispatcher)
    {
        btCollisionObject* colObj0 = (btCollisionObject*) proxy0->m_clientObject;
        btCollisionObject* colObj1 = (btCollisionObject*) proxy1->m_clientObject;
        if (colObj0->getUserIndex() == USER_INDEX_WATER_BODY) {
            //Don't let two water bodies affect each other
            if (colObj1->getUserIndex() != USER_INDEX_WATER_BODY) {
                auto waterBodyEntry = (BulletEntry*) colObj0->getUserPointer();
                auto otherEntry = (BulletEntry*) colObj1->getUserPointer();
                removeFromWater(waterBodyEntry, otherEntry);
            }
        } else if (colObj1->getUserIndex() == USER_INDEX_WATER_BODY) {
            auto waterBodyEntry = (BulletEntry*) colObj1->getUserPointer();
            auto otherEntry = (BulletEntry*) colObj0->getUserPointer();
            removeFromWater(waterBodyEntry, otherEntry);
        }
        return 0;
    }

    virtual void removeOverlappingPairsContainingProxy(btBroadphaseProxy* /*proxy0*/, btDispatcher* /*dispatcher*/)
    {
    }
};

struct PhysicalDomain::VisibilityPairCallback : public btOverlappingPairCallback
{
    btBroadphasePair* addOverlappingPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) override
    {
        bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
        collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);
        if (collides) {
            btCollisionObject* movedObject = (btCollisionObject*) proxy0->m_clientObject;
            auto movedEntry = (BulletEntry*) movedObject->getUserPointer();
            btCollisionObject* otherObject = (btCollisionObject*) proxy1->m_clientObject;
            auto otherEntry = (BulletEntry*) otherObject->getUserPointer();
            if (movedEntry != otherEntry) {
                if (movedObject == movedEntry->viewSphere.get()) {
                    //An observer was moved
                    movedEntry->observedByThisChanges.emplace_back(otherEntry, BulletEntry::VisibilityQueueOperationType::Add);
                } else {
                    //An observable was moved
                    movedEntry->observingThisChanges.emplace_back(otherEntry, BulletEntry::VisibilityQueueOperationType::Add);
                }
            }
        }
        return nullptr;
    }

    void* removeOverlappingPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1, btDispatcher* dispatcher) override
    {
        bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
        collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);
        if (collides) {
            btCollisionObject* movedObject = (btCollisionObject*) proxy0->m_clientObject;
            auto movedEntry = (BulletEntry*) movedObject->getUserPointer();
            btCollisionObject* otherObject = (btCollisionObject*) proxy1->m_clientObject;
            auto otherEntry = (BulletEntry*) otherObject->getUserPointer();
            if (movedEntry != otherEntry) {
                if (movedObject == movedEntry->viewSphere.get()) {
                    //An observer was moved
                    movedEntry->observedByThisChanges.emplace_back(otherEntry, BulletEntry::VisibilityQueueOperationType::Remove);
                } else {
                    //An observable was moved
                    movedEntry->observingThisChanges.emplace_back(otherEntry, BulletEntry::VisibilityQueueOperationType::Remove);
                }
            }
        }
        return nullptr;
    }

    void removeOverlappingPairsContainingProxy(btBroadphaseProxy* proxy0, btDispatcher* dispatcher) override
    {}
};

std::chrono::steady_clock::duration postDuration;

PhysicalDomain::PhysicalDomain(LocatedEntity& entity) :
        Domain(entity),
        mWorldInfo{&m_propellingEntries, &m_steppingEntries},
        //default config for now
        m_collisionConfiguration(new btDefaultCollisionConfiguration()),
        m_dispatcher(new btCollisionDispatcher(m_collisionConfiguration.get())),
        m_constraintSolver(new btSequentialImpulseConstraintSolver()),
        //We'll use a dynamic broadphase for the main world. It's not as fast as SAP variants, but it's faster when dynamic objects are at rest.
        m_broadphase(new btDbvtBroadphase()),
        // m_broadphase(new btAxisSweep3(Convert::toBullet(entity.m_location.bBox().lowCorner()),
        //                                              Convert::toBullet(entity.m_location.bBox().highCorner()))),
        m_dynamicsWorld(new PhysicalWorld(m_dispatcher.get(), m_broadphase.get(), m_constraintSolver.get(), m_collisionConfiguration.get())),
        m_visibilityPairCallback(new VisibilityPairCallback()),
        m_visibilityDispatcher(new btCollisionDispatcher(m_collisionConfiguration.get())),
        //We'll use a SAP broadphase for the visibility. This is more efficient than a dynamic one.
        //TODO: how to handle the limit for 16384 entries? Perhaps use the bt32BitAxisSweep3 with a custom max entries setting (to avoid it eating all memory).
        m_visibilityBroadphase(new btAxisSweep3(Convert::toBullet(entity.m_location.bBox().lowCorner()) * VISIBILITY_SCALING_FACTOR,
                                                Convert::toBullet(entity.m_location.bBox().highCorner()) * VISIBILITY_SCALING_FACTOR)),
        m_visibilityWorld(new btCollisionWorld(m_visibilityDispatcher.get(),
                                               m_visibilityBroadphase.get(),
                                               m_collisionConfiguration.get())),
        m_visibilityCheckCountdown(0),
        mContainingEntityEntry{entity},
        m_terrain(nullptr),
        m_ghostPairCallback(new WaterCollisionCallback())
{
    m_ghostPairCallback->m_domain = this;
    m_dynamicsWorld->getPairCache()->setInternalGhostPairCallback(m_ghostPairCallback.get());
    m_visibilityBroadphase->setOverlappingPairUserCallback(m_visibilityPairCallback.get());

    //This is to prevent us from sliding down slopes.
    //m_dynamicsWorld->getDispatchInfo().m_allowedCcdPenetration = 0.0001f;

    //By default all collision objects have their aabbs updated each tick; we'll disable it for performance.
    m_dynamicsWorld->setForceUpdateAllAabbs(false);

    m_visibilityWorld->setForceUpdateAllAabbs(false);

    auto terrainProperty = m_entity.getPropertyClassFixed<TerrainProperty>();
    if (terrainProperty) {
        m_terrain = &terrainProperty->getData(m_entity);
    }

    createDomainBorders();

    //Update the linear velocity of all self propelling entities each tick.
    auto preTickCallback = [](btDynamicsWorld* world, btScalar timeStep) {
        rmt_ScopedCPUSample(PhysicalDomain_preTickCallback, 0)
        auto worldInfo = static_cast<WorldInfo*>(world->getWorldUserInfo());
        auto propellingEntries = worldInfo->propellingEntries;
        for (auto& entry : *propellingEntries) {
            float verticalVelocity = entry.second.rigidBody->getLinearVelocity().y();

            //TODO: check if we're on the ground, in the water or flying and apply different speed modifiers
            double speed;
            if (entry.second.bulletEntry->mode == ModeProperty::Mode::Submerged) {
                speed = entry.second.bulletEntry->speedWater;
            } else {
                speed = entry.second.bulletEntry->speedGround;
            }
            btVector3 finalSpeed = entry.second.velocity * speed;

            //Apply gravity
            if (!WFMath::Equal(verticalVelocity, 0, WFMath::numeric_constants<float>::epsilon())) {
                verticalVelocity += entry.second.rigidBody->getGravity().y() * timeStep;
                entry.second.rigidBody->setLinearVelocity(finalSpeed + btVector3(0, verticalVelocity, 0));
            } else {
                entry.second.rigidBody->setLinearVelocity(finalSpeed);
            }

            //When entities are being propelled they will have low friction. When propelling stop the friction will be returned in setVelocity.
            entry.second.bulletEntry->collisionObject->setFriction(0.5);
            entry.second.bulletEntry->collisionObject->activate();
        }
    };

    auto postTickCallback = [](btDynamicsWorld* world, btScalar timeStep) {
        rmt_ScopedCPUSample(PhysicalDomain_postTickCallback, 0)
        auto start = std::chrono::steady_clock::now();
        auto worldInfo = static_cast<WorldInfo*>(world->getWorldUserInfo());
        auto steppingEntries = worldInfo->steppingEntries;
        for (auto& entry : *steppingEntries) {
            auto collisionObject = btRigidBody::upcast(entry.second.first->collisionObject.get());
            //Check that the object has moved, and if so check if it should be clamped to the ground
            if (collisionObject->getInterpolationLinearVelocity().length2() > 0.001) {
                struct : btCollisionWorld::ContactResultCallback
                {
                    bool isHit = false;

                    btScalar addSingleResult(btManifoldPoint& cp, const btCollisionObjectWrapper* colObj0Wrap,
                                             int partId0, int index0, const btCollisionObjectWrapper* colObj1Wrap,
                                             int partId1, int index1) override
                    {
                        //If the normal points upwards it's below us.
                        if (cp.m_normalWorldOnB.y() > 0) {
                            isHit = true;
                        }
                        return btScalar(1.0);
                    }
                } collideCallback;
                collideCallback.m_collisionFilterMask = collisionObject->getBroadphaseHandle()->m_collisionFilterMask;
                collideCallback.m_collisionFilterGroup = collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;

                world->contactTest(collisionObject, collideCallback);
                if (!collideCallback.isHit) {
                    //The entity isn't standing on top of anything. Check that we're not jumping; if not we should try to clamp it to the ground.
                    if (!entry.second.first->isJumping) {
                        //Cast a ray from the bottom and check if we're already colliding with the object we hit with our ray.
                        //If we don't collide it means that we're in the air, and should be clamped to the ground.
                        auto& worldTransform = collisionObject->getWorldTransform();
                        btVector3 aabbMin, aabbMax;
                        collisionObject->getCollisionShape()->getAabb(worldTransform, aabbMin, aabbMax);
                        btVector3 bottomOfObject = worldTransform.getOrigin();
                        bottomOfObject.setY(aabbMin.y());
                        btVector3 bottomOfRay = bottomOfObject;
                        float rayDistance = entry.second.second * (aabbMax.y() - aabbMin.y());

                        bottomOfRay.setY(bottomOfRay.y() - rayDistance);

                        btCollisionWorld::ClosestRayResultCallback callback(bottomOfObject, bottomOfRay);
                        callback.m_collisionFilterMask = collisionObject->getBroadphaseHandle()->m_collisionFilterMask;
                        callback.m_collisionFilterGroup = collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;

                        world->rayTest(bottomOfObject, bottomOfRay, callback);

                        if (callback.hasHit()) {
                            float distance = bottomOfObject.y() - callback.m_hitPointWorld.y();
                            if (distance > 0.2) {
                                worldTransform.getOrigin().setY(worldTransform.getOrigin().y() - distance);
                                collisionObject->setWorldTransform(worldTransform);
                                //Dampen the vertical velocity a bit
                                auto velocity = collisionObject->getLinearVelocity();
                                velocity.setY(velocity.y() * 0.1f);
                                collisionObject->setLinearVelocity(velocity);
                            }
                        }
                    }
                } else {
                    //The entity is standing on top of something, make sure it's not marked as jumping anymore.
                    entry.second.first->isJumping = false;
                }
            }
        }
        if (debug_flag) {
            postDuration += std::chrono::steady_clock::now() - start;
        }
    };

    m_dynamicsWorld->setInternalTickCallback(preTickCallback, &mWorldInfo, true);
    m_dynamicsWorld->setInternalTickCallback(postTickCallback, &mWorldInfo, false);

    mContainingEntityEntry.mode = ModeProperty::Mode::Fixed;

    m_entries.insert(std::make_pair(entity.getIntId(), &mContainingEntityEntry));

    buildTerrainPages();

    m_entity.propertyApplied.connect(sigc::mem_fun(this, &PhysicalDomain::entityPropertyApplied));
}

PhysicalDomain::~PhysicalDomain()
{
    for (auto& planeBody : m_borderPlanes) {
        m_dynamicsWorld->removeCollisionObject(planeBody.first.get());
    }
    m_borderPlanes.clear();

    for (auto& entry : m_terrainSegments) {
        m_dynamicsWorld->removeCollisionObject(entry.second.rigidBody.get());
    }
    m_terrainSegments.clear();

    //Remove our own entry first, since we own the memory
    m_entries[m_entity.getIntId()].release();
    m_entries.erase(m_entity.getIntId());

    for (auto& entry : m_entries) {
        if (entry.second->collisionObject) {
            m_dynamicsWorld->removeCollisionObject(entry.second->collisionObject.get());
        }

        entry.second->collisionShape.reset();

        entry.second->propertyUpdatedConnection.disconnect();
    }

    m_propertyAppliedConnection.disconnect();
}

void PhysicalDomain::installDelegates(LocatedEntity* entity, const std::string& propertyName)
{
    entity->installDelegate(Atlas::Objects::Operation::TICK_NO, propertyName);
    auto tickOp = scheduleTick(*entity);
    entity->sendWorld(tickOp);
}

Atlas::Objects::Operation::RootOperation PhysicalDomain::scheduleTick(LocatedEntity& entity)
{
    Atlas::Objects::Entity::Anonymous tick_arg;
    tick_arg->setName("domain");
    Atlas::Objects::Operation::Tick tickOp;
    tickOp->setTo(entity.getId());
    tickOp->setArgs1(tick_arg);

    return tickOp;
}

HandlerResult PhysicalDomain::operation(LocatedEntity* entity, const Operation& op, OpVector& res)
{
    return tick_handler(entity, op, res);
}

HandlerResult PhysicalDomain::tick_handler(LocatedEntity* entity, const Operation& op, OpVector& res)
{
    if (!op->getArgs().empty() && !op->getArgs().front()->isDefaultName() && op->getArgs().front()->getName() == "domain") {
        double timeNow = op->getSeconds();
        double tickSize = TICK_SIZE;
        Atlas::Message::Element elem;
        if (op->copyAttr("lastTick", elem) == 0 && elem.isFloat()) {
            tickSize = timeNow - elem.Float();
        }

        tick(tickSize, res);
        auto tickOp = scheduleTick(*entity);
        tickOp->setSeconds(timeNow + TICK_SIZE);
        tickOp->setAttr("lastTick", timeNow);
        res.emplace_back(std::move(tickOp));

        return OPERATION_BLOCKED;
    }
    return OPERATION_IGNORED;
}

void PhysicalDomain::buildTerrainPages()
{
    boost::optional<float> friction;
    boost::optional<float> rollingFriction;
    boost::optional<float> spinningFriction;

    {
        auto frictionProp = m_entity.getPropertyType<double>("friction");

        if (frictionProp) {
            friction = (float) frictionProp->data();
        }
    }

    {
        auto frictionProp = m_entity.getPropertyType<double>("friction_roll");

        if (frictionProp) {
            rollingFriction = (float) frictionProp->data();
        }
    }

    {
        auto frictionProp = m_entity.getPropertyType<double>("friction_spin");

        if (frictionProp) {
            spinningFriction = (float) frictionProp->data();
        }
    }
    const auto* terrainProperty = m_entity.getPropertyClassFixed<TerrainProperty>();
    if (terrainProperty) {
        auto& terrain = terrainProperty->getData(m_entity);
        auto segments = terrain.getTerrain();
        for (auto& row : segments) {
            for (auto& entry : row.second) {
                Mercator::Segment* segment = entry.second;
                auto& terrainEntry = buildTerrainPage(*segment);
                if (friction) {
                    terrainEntry.rigidBody->setFriction(*friction);
                }
                if (spinningFriction) {
#if BT_BULLET_VERSION < 285
                    log(WARNING, "Your version of Bullet doesn't support spinning friction.");
#else
                    terrainEntry.rigidBody->setSpinningFriction(*spinningFriction);
#endif
                }
                if (rollingFriction) {
                    terrainEntry.rigidBody->setRollingFriction(*rollingFriction);
                }
            }
        }
    }
}

PhysicalDomain::TerrainEntry& PhysicalDomain::buildTerrainPage(Mercator::Segment& segment)
{
    if (!segment.isValid()) {
        segment.populate();
    }

    int vertexCountOneSide = segment.getSize();

    std::stringstream ss;
    ss << segment.getXRef() << ":" << segment.getZRef();
    TerrainEntry& terrainEntry = m_terrainSegments[ss.str()];
    if (!terrainEntry.data) {
        terrainEntry.data = std::make_unique<std::array<float, 65 * 65>>();
    }
    if (terrainEntry.rigidBody) {
        m_dynamicsWorld->removeRigidBody(terrainEntry.rigidBody.get());
        terrainEntry.rigidBody.reset();
        terrainEntry.shape.reset();
    }
    float* data = terrainEntry.data->data();
    const float* mercatorData = segment.getPoints();

    memcpy(data, mercatorData, vertexCountOneSide * vertexCountOneSide * sizeof(float));

    float min = segment.getMin();
    float max = segment.getMax();

    terrainEntry.shape = std::make_unique<btHeightfieldTerrainShape>(vertexCountOneSide, vertexCountOneSide, data, 1.0f, min, max, 1, PHY_FLOAT, false);

    terrainEntry.shape->setLocalScaling(btVector3(1, 1, 1));

    auto res = (float) segment.getResolution();

    float xPos = segment.getXRef() + (res / 2.0f);
    float yPos = min + ((max - min) * 0.5f);
    float zPos = segment.getZRef() + (res / 2.0f);

    WFMath::Point<3> pos(xPos, yPos, zPos);
    btVector3 btPos = Convert::toBullet(pos);

    btRigidBody::btRigidBodyConstructionInfo segmentCI(.0f, nullptr, terrainEntry.shape.get());
    auto segmentBody = new btRigidBody(segmentCI);
    segmentBody->setWorldTransform(btTransform(btQuaternion::getIdentity(), btPos));

    m_dynamicsWorld->addRigidBody(segmentBody, COLLISION_MASK_TERRAIN, COLLISION_MASK_NON_PHYSICAL | COLLISION_MASK_PHYSICAL);

    terrainEntry.rigidBody.reset(segmentBody);
    terrainEntry.rigidBody->setUserPointer(&mContainingEntityEntry);
    return terrainEntry;
}

void PhysicalDomain::createDomainBorders()
{
    auto& bbox = m_entity.m_location.bBox();
    if (bbox.isValid()) {
        //We'll now place six planes representing the bounding box.

        m_borderPlanes.reserve(6);
        auto createPlane =
                [&](const btVector3& normal, const btVector3& translate) {
                    auto plane = std::make_unique<btStaticPlaneShape>(normal, .0f);
                    auto planeBody = std::make_unique<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(0, nullptr, plane.get()));
                    planeBody->setWorldTransform(btTransform(btQuaternion::getIdentity(), translate));
                    planeBody->setUserPointer(&mContainingEntityEntry);
                    m_dynamicsWorld->addRigidBody(planeBody.get(), COLLISION_MASK_TERRAIN, COLLISION_MASK_NON_PHYSICAL | COLLISION_MASK_PHYSICAL);
                    m_borderPlanes.emplace_back(std::move(planeBody), std::move(plane));
                };

        //Bottom plane
        createPlane(btVector3(0, 1, 0), btVector3(0, bbox.lowerBound(1), 0));

        //Top plane
        createPlane(btVector3(0, -1, 0), btVector3(0, bbox.upperBound(1), 0));

        //Crate surrounding planes
        createPlane(btVector3(1, 0, 0), btVector3(bbox.lowerBound(0), 0, 0));
        createPlane(btVector3(-1, 0, 0), btVector3(bbox.upperBound(0), 0, 0));
        createPlane(btVector3(0, 0, 1), btVector3(0, 0, bbox.lowerBound(2)));
        createPlane(btVector3(0, 0, -1), btVector3(0, 0, bbox.upperBound(2)));
    }
}

bool PhysicalDomain::isEntityVisibleFor(const LocatedEntity& observingEntity, const LocatedEntity& observedEntity) const
{
    //Is it observing the domain entity?
    if (&observedEntity == &m_entity) {
        return true;
    }

    //Is it observing itself?
    if (&observingEntity == &observedEntity) {
        return true;
    }

    //Is it the domain entity?
    if (&observingEntity == &m_entity) {
        return true;
    }

    auto observingI = m_entries.find(observingEntity.getIntId());
    if (observingI == m_entries.end()) {
        return false;
    }
    auto observedI = m_entries.find(observedEntity.getIntId());
    if (observedI == m_entries.end()) {
        return false;
    }

    auto& observedEntry = observedI->second;
    auto& observingEntry = observingI->second;
    return observedEntry->observingThis.find(observingEntry.get()) != observedEntry->observingThis.end();
}

void PhysicalDomain::getVisibleEntitiesFor(const LocatedEntity& observingEntity, std::list<LocatedEntity*>& entityList) const
{
    auto observingI = m_entries.find(observingEntity.getIntId());
    if (observingI != m_entries.end()) {
        const auto& bulletEntry = observingI->second;
        for (const auto& observedEntry : bulletEntry->observedByThis) {
            entityList.push_back(&observedEntry->entity);
        }
    }
}

std::vector<LocatedEntity*> PhysicalDomain::getObservingEntitiesFor(const LocatedEntity& observedEntity) const
{
    std::vector<LocatedEntity*> entityList;

    auto observedI = m_entries.find(observedEntity.getIntId());
    if (observedI != m_entries.end()) {
        auto& bulletEntry = observedI->second;
        for (const auto& observingEntry : bulletEntry->observingThis) {
            entityList.push_back(&observingEntry->entity);
        }
    }

    return entityList;
}

void PhysicalDomain::updateObserverEntry(BulletEntry* bulletEntry, OpVector& res)
{
    if (bulletEntry->viewSphere) {
        //This entry is an observer; check what it can see after it has moved
        auto& viewSphere = bulletEntry->viewSphere;
        if (viewSphere) {
            viewSphere->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(bulletEntry->entity.m_location.m_pos) * VISIBILITY_SCALING_FACTOR));
            m_visibilityWorld->updateSingleAabb(viewSphere.get());
        }

        std::vector<Atlas::Objects::Root> appearArgs;
        std::vector<Atlas::Objects::Root> disappearArgs;

        auto disappearFn = [&](BulletEntry* disappearedEntry) {
            Anonymous that_ent;
            that_ent->setId(disappearedEntry->entity.getId());
            that_ent->setStamp(disappearedEntry->entity.getSeq());

            disappearArgs.push_back(std::move(that_ent));

            disappearedEntry->observingThis.erase(bulletEntry);
        };

        auto appearFn = [&](BulletEntry* appearedEntry) {
            //Send Appear
            // debug_print(" appear: " << viewedEntry->entity.describeEntity() << " for " << bulletEntry->entity.describeEntity());
            Anonymous that_ent;
            that_ent->setId(appearedEntry->entity.getId());
            that_ent->setStamp(appearedEntry->entity.getSeq());
            appearArgs.push_back(std::move(that_ent));

            appearedEntry->observingThis.insert(bulletEntry);
        };

        for (auto& entry : bulletEntry->observedByThisChanges) {
            if (entry.second == BulletEntry::VisibilityQueueOperationType::Add) {
                appearFn(entry.first);
                bulletEntry->observedByThis.insert(entry.first);
            } else {
                disappearFn(entry.first);
                bulletEntry->observedByThis.erase(entry.first);
            }
        }

        if (!appearArgs.empty()) {
            Appearance appear;
            appear->setTo(bulletEntry->entity.getId());
            appear->setArgs(std::move(appearArgs));
            res.push_back(std::move(appear));
        }
        if (!disappearArgs.empty()) {
            Disappearance disappear;
            disappear->setTo(bulletEntry->entity.getId());
            disappear->setArgs(std::move(disappearArgs));
            res.push_back(std::move(disappear));
        }

        bulletEntry->observedByThisChanges.clear();

    }
}

void PhysicalDomain::updateObservedEntry(BulletEntry* bulletEntry, OpVector& res, bool generateOps)
{
    if (bulletEntry->visibilitySphere) {
        //This entry is an observable; check what can see it after it has moved

        auto& visibilitySphere = bulletEntry->visibilitySphere;
        if (visibilitySphere) {
            visibilitySphere->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(bulletEntry->entity.m_location.m_pos) * VISIBILITY_SCALING_FACTOR));
            m_visibilityWorld->updateSingleAabb(visibilitySphere.get());
        }

        auto disappearFn = [&](BulletEntry* existingObserverEntry) {
            if (generateOps) {
                //Send disappearence
                // debug_print(" disappear: " << bulletEntry->entity.describeEntity() << " for " << noLongerObservingEntry->entity.describeEntity());
                Disappearance disappear;
                Anonymous that_ent;
                that_ent->setId(bulletEntry->entity.getId());
                that_ent->setStamp(bulletEntry->entity.getSeq());
                disappear->setArgs1(std::move(that_ent));
                disappear->setTo(existingObserverEntry->entity.getId());
                res.push_back(std::move(disappear));
            }

            existingObserverEntry->observedByThis.erase(bulletEntry);
        };

        auto appearFn = [&](BulletEntry* newObserverEntry) {
            if (generateOps) {
                //Send appear
                // debug_print(" appear: " << bulletEntry->entity.describeEntity() << " for " << viewingEntry->entity.describeEntity());
                Appearance appear;
                Anonymous that_ent;
                that_ent->setId(bulletEntry->entity.getId());
                that_ent->setStamp(bulletEntry->entity.getSeq());
                appear->setArgs1(std::move(that_ent));
                appear->setTo(newObserverEntry->entity.getId());
                res.push_back(std::move(appear));
            }

            newObserverEntry->observedByThis.insert(bulletEntry);
        };

        for (auto& entry : bulletEntry->observingThisChanges) {
            if (entry.second == BulletEntry::VisibilityQueueOperationType::Add) {
                appearFn(entry.first);
                bulletEntry->observingThis.insert(entry.first);
            } else {
                disappearFn(entry.first);
                bulletEntry->observingThis.erase(entry.first);
            }
        }

        bulletEntry->observingThisChanges.clear();
    }
}

void PhysicalDomain::updateVisibilityOfDirtyEntities(OpVector& res)
{
    rmt_ScopedCPUSample(PhysicalDomain_updateVisibilityOfDirtyEntities, 0)

    if (!m_visibilityRecalculateQueue.empty()) {
        size_t i = 0;
        //Handle max VISIBILITY_CHECK_MAX_ENTRIES entities each time.
        auto I = m_visibilityRecalculateQueue.begin();
        for (; I != m_visibilityRecalculateQueue.end() && i < VISIBILITY_CHECK_MAX_ENTRIES; ++i, ++I) {
            auto& bulletEntry = *I;
            updateObservedEntry(bulletEntry, res);
            updateObserverEntry(bulletEntry, res);
            bulletEntry->markedForVisibilityRecalculation = false;
            bulletEntry->entity.onUpdated();
        }
        m_visibilityRecalculateQueue.erase(m_visibilityRecalculateQueue.begin(), I);
    }
}

float PhysicalDomain::getMassForEntity(const LocatedEntity& entity) const
{
    float mass = 0;

    auto massProp = entity.getPropertyType<double>("mass");
    if (massProp) {
        mass = (float) massProp->data();
    }
    return mass;
}

std::shared_ptr<btCollisionShape> PhysicalDomain::createCollisionShapeForEntry(LocatedEntity& entity,
                                                                               const WFMath::AxisBox<3>& bbox, float mass,
                                                                               btVector3& centerOfMassOffset)
{
    auto geometryProp = entity.getPropertyClassFixed<GeometryProperty>();
    if (geometryProp) {
        return geometryProp->createShape(bbox, centerOfMassOffset, mass);
    } else {
        auto size = bbox.highCorner() - bbox.lowCorner();
        auto btSize = Convert::toBullet(size * 0.5).absolute();
        centerOfMassOffset = -Convert::toBullet(bbox.getCenter());
        return std::make_shared<btBoxShape>(btSize);
    }
}

void PhysicalDomain::addEntity(LocatedEntity& entity)
{
    assert(m_entries.find(entity.getIntId()) == m_entries.end());

    if (!entity.m_location.m_pos.isValid()) {
        log(WARNING, String::compose("Tried to add entity %1 to physical domain belonging to %2, but there's no valid position.", entity.describeEntity(), m_entity.describeEntity()));
        return;
    }

    float mass = getMassForEntity(entity);

    WFMath::AxisBox<3> bbox = entity.m_location.bBox();
    btVector3 angularFactor(1, 1, 1);

    auto entry = new BulletEntry{entity};
    m_entries.insert(std::make_pair(entity.getIntId(), entry));

    auto angularFactorProp = entity.getPropertyClassFixed<AngularFactorProperty>();
    if (angularFactorProp && angularFactorProp->data().isValid()) {
        angularFactor = Convert::toBullet(angularFactorProp->data());
    }

    ModeProperty::Mode mode = ModeProperty::Mode::Free;
    auto modeProp = entity.getPropertyClassFixed<ModeProperty>();
    if (modeProp) {
        mode = modeProp->getMode();
    }

    entry->modeChanged = false;
    entry->mode = mode;

    if (mode == ModeProperty::Mode::Planted || mode == ModeProperty::Mode::Fixed) {
        //"fixed" mode means that the entity stays in place, always
        //"planted" mode means it's planted in the ground
        //Zero mass makes the rigid body static
        mass = .0f;
    }

    btQuaternion orientation = entity.m_location.m_orientation.isValid() ? Convert::toBullet(entity.m_location.m_orientation) : btQuaternion::getIdentity();

    short collisionMask;
    short collisionGroup;
    getCollisionFlagsForEntity(entity, collisionGroup, collisionMask);

    auto waterBodyProp = entity.getPropertyClass<BoolProperty>("water_body");
    if (waterBodyProp && waterBodyProp->isTrue()) {

        auto ghostObject = std::make_unique<btGhostObject>();
        entry->collisionObject = std::move(ghostObject);
        entry->collisionObject->setUserIndex(USER_INDEX_WATER_BODY);
        entry->collisionObject->setUserPointer(entry);

        //If there's a valid bbox, use that to create a contained body of water.
        // Otherwise, create an infinitely large body of water (i.e. an "ocean") using a plane.
        if (bbox.isValid()) {
            //"Center of mass offset" is the inverse of the center of the object in relation to origo.
            auto size = bbox.highCorner() - bbox.lowCorner();
            entry->collisionShape = std::make_shared<btBoxShape>(Convert::toBullet(size / 2));
            entry->centerOfMassOffset = -Convert::toBullet(bbox.getCenter());
        } else {
            entry->collisionShape = std::make_shared<btStaticPlaneShape>(btVector3(0, 1, 0), 0);
            entry->centerOfMassOffset = btVector3(0, 0, 0);
        }
        entry->collisionObject->setCollisionShape(entry->collisionShape.get());
        entry->collisionObject->setCollisionFlags(entry->collisionObject->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);

        calculatePositionForEntity(mode, entry, entity.m_location.m_pos);
        entry->collisionObject->setWorldTransform(btTransform(orientation, Convert::toBullet(entity.m_location.m_pos))
                                                  * btTransform(btQuaternion::getIdentity(), entry->centerOfMassOffset).inverse());

        m_dynamicsWorld->addCollisionObject(entry->collisionObject.get(), collisionGroup, collisionMask);
        entry->collisionObject->activate();
    } else {
        if (bbox.isValid()) {
            //"Center of mass offset" is the inverse of the center of the object in relation to origo.
            auto size = bbox.highCorner() - bbox.lowCorner();
            btVector3 inertia(0, 0, 0);

            entry->collisionShape = createCollisionShapeForEntry(entry->entity, bbox, mass, entry->centerOfMassOffset);

            if (mass > 0) {
                entry->collisionShape->calculateLocalInertia(mass, inertia);
            }


            debug_print("PhysicsDomain adding entity " << entity.describeEntity() << " with mass " << mass
                                                       << " and inertia (" << inertia.x() << "," << inertia.y() << "," << inertia.z() << ")")

            btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(mass, nullptr, entry->collisionShape.get(), inertia);

            auto frictionProp = entity.getPropertyType<double>("friction");
            if (frictionProp) {
                rigidBodyCI.m_friction = (btScalar) frictionProp->data();
            }
            auto frictionRollProp = entity.getPropertyType<double>("friction_roll");
            if (frictionRollProp) {
                rigidBodyCI.m_rollingFriction = (btScalar) frictionRollProp->data();
            }
            auto frictionSpinProp = entity.getPropertyType<double>("friction_spin");
            if (frictionSpinProp) {
#if BT_BULLET_VERSION < 285
                log(WARNING, "Your version of Bullet doesn't support spinning friction.");
#else
                rigidBodyCI.m_spinningFriction = (btScalar) frictionSpinProp->data();
#endif
            }

            auto rigidBody = new btRigidBody(rigidBodyCI);
            entry->collisionObject = std::unique_ptr<btCollisionObject>(rigidBody);

            calculatePositionForEntity(mode, entry, entity.m_location.m_pos);

            entry->motionState = std::make_unique<PhysicalMotionState>(*entry, *rigidBody, *this,
                                                                       btTransform(orientation, Convert::toBullet(entity.m_location.m_pos)),
                                                                       btTransform(btQuaternion::getIdentity(), entry->centerOfMassOffset));
            rigidBody->setMotionState(entry->motionState.get());
            rigidBody->setAngularFactor(angularFactor);
            entry->collisionObject->setUserPointer(entry);

            //To prevent tunneling we'll turn on CCD with suitable values.
            float minSize = std::min(size.x(), std::min(size.y(), size.z()));
//          float maxSize = std::max(size.x(), std::max(size.y(), size.z()));
            entry->collisionObject->setCcdMotionThreshold(minSize * CCD_MOTION_FACTOR);
            entry->collisionObject->setCcdSweptSphereRadius(minSize * CCD_SPHERE_FACTOR);

            //Set up cached speed values
            auto speedGroundProp = entity.getPropertyType<double>("speed_ground");
            entry->speedGround = speedGroundProp ? speedGroundProp->data() : 0;

            auto speedWaterProp = entity.getPropertyType<double>("speed_water");
            entry->speedWater = speedWaterProp ? speedWaterProp->data() : 0;

            auto speedFlightProp = entity.getPropertyType<double>("speed_flight");
            entry->speedFlight = speedFlightProp ? speedFlightProp->data() : 0;

            //Only add to world if position is valid. Otherwise this will be done when a new valid position is applied in applyNewPositionForEntity
            if (entity.m_location.m_pos.isValid()) {
                m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);
            }

            //Call to "activate" will be ignored for bodies marked with CF_STATIC_OBJECT
            entry->collisionObject->activate();

            auto propelProp = entity.getPropertyClassFixed<PropelProperty>();
            if (propelProp && propelProp->data().isValid() && propelProp->data() != WFMath::Vector<3>::ZERO()) {
                applyPropel(*entry, propelProp->data());
            }

            auto stepFactorProp = entity.getPropertyType<double>("step_factor");
            if (stepFactorProp && stepFactorProp->data() > 0) {
                m_steppingEntries.emplace(entity.getIntId(), std::make_pair(entry, stepFactorProp->data()));
            }

            //Should we only do this for "free" and "projectile"?
            if (entity.m_location.velocity().isValid()) {
                rigidBody->applyCentralImpulse(Convert::toBullet(entity.m_location.velocity()));
            }
        }
    }

    entry->propertyUpdatedConnection = entity.propertyApplied.connect(sigc::bind(sigc::mem_fun(this, &PhysicalDomain::childEntityPropertyApplied), entry));

    updateTerrainMod(entity, true);

    {
        auto visSphere = std::make_unique<btSphereShape>(0);
        visSphere->setUnscaledRadius(calculateVisibilitySphereRadius(entity));

        auto visObject = std::make_unique<btCollisionObject>();
        visObject->setCollisionShape(visSphere.get());
        visObject->setUserPointer(entry);
        auto visibilityObjectPtr = visObject.get();
        entry->visibilitySphere = std::move(visObject);
        entry->visibilityShape = std::move(visSphere);
        if (entity.m_location.m_pos.isValid()) {
            visibilityObjectPtr->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(entity.m_location.m_pos) * VISIBILITY_SCALING_FACTOR));
            m_visibilityWorld->addCollisionObject(visibilityObjectPtr, VISIBILITY_MASK_OBSERVER,
                                                  entity.hasFlags(entity_visibility_protected) || entity.hasFlags(entity_visibility_private) ? VISIBILITY_MASK_OBSERVABLE_PRIVATE
                                                                                                                                             : VISIBILITY_MASK_OBSERVABLE);
        }
    }
    if (entity.isPerceptive()) {
        auto viewShape = std::make_unique<btSphereShape>(VIEW_SPHERE_RADIUS * VISIBILITY_SCALING_FACTOR);
        auto viewSphere = std::make_unique<btCollisionObject>();
        viewSphere->setCollisionShape(viewShape.get());
        viewSphere->setUserPointer(entry);
        auto viewSpherePtr = viewSphere.get();
        entry->viewSphere = std::move(viewSphere);
        entry->viewShape = std::move(viewShape);

        //Should always be able to observe the domain entity.
        entry->observedByThisChanges.emplace_back(&mContainingEntityEntry, BulletEntry::VisibilityQueueOperationType::Add);

        if (entity.m_location.m_pos.isValid()) {
            viewSpherePtr->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(entity.m_location.m_pos) * VISIBILITY_SCALING_FACTOR));
            m_visibilityWorld->addCollisionObject(viewSpherePtr, entity.hasFlags(entity_admin) ? VISIBILITY_MASK_OBSERVABLE | VISIBILITY_MASK_OBSERVABLE_PRIVATE : VISIBILITY_MASK_OBSERVABLE,
                                                  VISIBILITY_MASK_OBSERVER);
        }
        mContainingEntityEntry.observingThis.insert(entry);

        //We are observing ourselves, and we are being observed by ourselves.
        entry->observedByThis.insert(entry);
        entry->observingThis.insert(entry);
    }

    if (m_entity.isPerceptive()) {
        entry->observingThis.insert(&mContainingEntityEntry);
        mContainingEntityEntry.observedByThis.insert(entry);
    }

    OpVector res;
    updateObserverEntry(entry, res);
    updateObservedEntry(entry, res, false); //Don't send any ops, since that will be handled by the calling code when changing locations.
    for (auto& op : res) {
        m_entity.sendWorld(op);
    }
}

void PhysicalDomain::toggleChildPerception(LocatedEntity& entity)
{
    auto I = m_entries.find(entity.getIntId());
    if (I == m_entries.end()) {
        //This could happen if the entity didn't have any position, for example.
        return;
    }
    auto& entry = I->second;
    if (entity.isPerceptive()) {
        if (!entry->viewSphere) {
            mContainingEntityEntry.observingThis.insert(entry.get());
            auto viewShape = std::make_unique<btSphereShape>(VIEW_SPHERE_RADIUS * VISIBILITY_SCALING_FACTOR);
            auto viewSphere = std::make_unique<btCollisionObject>();
            viewSphere->setCollisionShape(viewShape.get());
            viewSphere->setUserPointer(entry.get());
            if (entity.m_location.m_pos.isValid()) {
                viewSphere->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(entity.m_location.m_pos) * VISIBILITY_SCALING_FACTOR));
                m_visibilityWorld->addCollisionObject(viewSphere.get(), entity.hasFlags(entity_admin) ? VISIBILITY_MASK_OBSERVABLE | VISIBILITY_MASK_OBSERVABLE_PRIVATE : VISIBILITY_MASK_OBSERVABLE,
                                                      VISIBILITY_MASK_OBSERVER);
            }
            entry->viewSphere = std::move(viewSphere);

            //We are observing ourselves, and we are being observed by ourselves.
            entry->observedByThis.insert(entry.get());
            entry->observingThis.insert(entry.get());

            OpVector res;
            updateObserverEntry(entry.get(), res);
            for (auto& op : res) {
                m_entity.sendWorld(op);
            }
        }
    } else {
        if (entry->viewSphere) {
            m_visibilityWorld->removeCollisionObject(entry->viewSphere.get());
            entry->viewShape.reset();
            entry->viewSphere.reset();
            mContainingEntityEntry.observingThis.erase(entry.get());
        }
    }
}

void PhysicalDomain::removeEntity(LocatedEntity& entity)
{
    debug_print("PhysicalDomain::removeEntity " << entity.describeEntity())
    auto I = m_entries.find(entity.getIntId());
    if (I == m_entries.end()) {
        //This could happen if the entity didn't have any position, for example.
        return;
    }
    auto& entry = I->second;

    auto modI = m_terrainMods.find(entity.getIntId());
    if (modI != m_terrainMods.end()) {
        m_terrain->updateMod(entity.getIntId(), nullptr);
        m_terrainMods.erase(modI);
    }

    if (entry->collisionObject) {
        m_dynamicsWorld->removeCollisionObject(entry->collisionObject.get());
    }

    if (entry->addedToMovingList) {
        removeAndShift(m_movingEntities, entry.get());
    }

    entry->propertyUpdatedConnection.disconnect();
    if (entry->viewSphere) {
        m_visibilityWorld->removeCollisionObject(entry->viewSphere.get());
    }
    if (entry->visibilitySphere) {
        m_visibilityWorld->removeCollisionObject(entry->visibilitySphere.get());
    }
    for (BulletEntry* observer : entry->observingThis) {
        observer->observedByThis.erase(entry.get());
    }
    for (BulletEntry* observedEntry : entry->observedByThis) {
        observedEntry->observingThis.erase(entry.get());
    }

    if (entry->markedForVisibilityRecalculation) {
        removeAndShift(m_visibilityRecalculateQueue, entry.get());
    }

    mContainingEntityEntry.observingThis.erase(entry.get());

    //The entity owning the domain should normally not be perceptive, so we'll check first to optimize a bit.
    if (m_entity.isPerceptive()) {
        mContainingEntityEntry.observedByThis.insert(entry.get());
    }

    auto modeDataProp = entity.getPropertyClassFixed<ModeDataProperty>();
    if (modeDataProp && modeDataProp->getMode() == ModeProperty::Mode::Planted) {
        auto& plantedOnData = modeDataProp->getPlantedOnData();
        if (plantedOnData.entityId) {
            auto plantedOnI = m_entries.find(*plantedOnData.entityId);
            if (plantedOnI != m_entries.end()) {
                plantedOnI->second->attachedEntities.erase(entry.get());
            }
        }
    }

    for (auto& observation : entry->closenessObservations) {
        if (observation->callback) {
            observation->callback();
        }
    }

    //We need to first extract the list of attached entities before we erase the instance.
    //Otherwise the entity will still be there when we apply new positions for the attached entities.
    auto attachedEntities = std::move(entry->attachedEntities);

    m_entries.erase(I);

    m_propellingEntries.erase(entity.getIntId());
    m_steppingEntries.erase(entity.getIntId());

    std::set<LocatedEntity*> transformedEntities;
    for (auto* attachedEntry : attachedEntities) {
        applyTransformInternal(attachedEntry->entity,
                               {},
                               attachedEntry->entity.m_location.m_pos,
                               {},
                               transformedEntities,
                               true);
    }

}

void PhysicalDomain::childEntityPropertyApplied(const std::string& name, const PropertyBase& prop, BulletEntry* bulletEntry)
{
    if (name == "pos") {
//        auto& pos = bulletEntry->entity.m_location.m_pos;
//        if (pos.isValid()) {
//            applyNewPositionForEntity(bulletEntry, pos);
//            if (!oldPos.isEqualTo(entity.m_location.m_pos)) {
//                entity.removeFlags(entity_pos_clean);
//                hadChange = true;
//                //Check if there previously wasn't any valid pos, and thus no valid collision instances.
//                if (entity.m_location.m_pos.isValid() && !oldPos.isValid()) {
//                    if (entry->collisionObject) {
//                        short collisionMask;
//                        short collisionGroup;
//                        getCollisionFlagsForEntity(entity, collisionGroup, collisionMask);
//                        if (rigidBody) {
//                            m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);
//                        } else {
//                            m_dynamicsWorld->addCollisionObject(entry->collisionObject.get(), collisionGroup, collisionMask);
//                        }
//                    }
//                    if (entry->viewSphere) {
//                        m_visibilityWorld->addCollisionObject(entry->viewSphere.get(),
//                                                              entity.hasFlags(entity_admin) ? VISIBILITY_MASK_OBSERVABLE | VISIBILITY_MASK_OBSERVABLE_PRIVATE : VISIBILITY_MASK_OBSERVABLE,
//                                                              VISIBILITY_MASK_OBSERVER);
//                    }
//                    if (entry->visibilitySphere) {
//                        m_visibilityWorld->addCollisionObject(entry->visibilitySphere.get(), VISIBILITY_MASK_OBSERVER,
//                                                              entity.hasFlags(entity_visibility_protected) || entity.hasFlags(entity_visibility_private) ? VISIBILITY_MASK_OBSERVABLE_PRIVATE
//                                                                                                                                                         : VISIBILITY_MASK_OBSERVABLE);
//                    }
//                }
//            }
//        }
    } else if (name == PropelProperty::property_name) {
        auto propelProp = dynamic_cast<const PropelProperty*>(&prop);
        if (propelProp) {
            applyPropel(*bulletEntry, propelProp->data());
        }
    } else if (name == "friction") {
        if (bulletEntry->collisionObject) {
            auto frictionProp = dynamic_cast<const Property<double>*>(&prop);
            bulletEntry->collisionObject->setFriction(static_cast<btScalar>(frictionProp->data()));
            if (getMassForEntity(bulletEntry->entity) != 0) {
                bulletEntry->collisionObject->activate();
            }
        }
    } else if (name == "friction_roll") {
        if (bulletEntry->collisionObject) {
            auto frictionProp = dynamic_cast<const Property<double>*>(&prop);
            bulletEntry->collisionObject->setRollingFriction(static_cast<btScalar>(frictionProp->data()));
            if (getMassForEntity(bulletEntry->entity) != 0) {
                bulletEntry->collisionObject->activate();
            }
        }
    } else if (name == "friction_spin") {
        if (bulletEntry->collisionObject) {
#if BT_BULLET_VERSION < 285
            log(WARNING, "Your version of Bullet doesn't support spinning friction.");
#else
            auto frictionProp = dynamic_cast<const Property<double>*>(&prop);
            bulletEntry->collisionObject->setSpinningFriction(static_cast<btScalar>(frictionProp->data()));
            if (getMassForEntity(bulletEntry->entity) != 0) {
                bulletEntry->collisionObject->activate();
            }
#endif
        }
    } else if (name == "mode") {
        if (bulletEntry->collisionObject) {
            auto modeProp = dynamic_cast<const ModeProperty*>(&prop);
            //Check if the mode change came from "outside", i.e. wasn't made because of the physics simulation (such as being submerged).
            if (modeProp->getMode() != bulletEntry->mode) {
                applyNewPositionForEntity(bulletEntry, bulletEntry->entity.m_location.m_pos);

                auto rigidBody = btRigidBody::upcast(bulletEntry->collisionObject.get());
                if (rigidBody) {
                    if (modeProp->getMode() == ModeProperty::Mode::Projectile) {
                        rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
                    } else {
                        rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() & ~btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
                    }

                    //If there's a rigid body, there's a valid bbox, otherwise something else is broken
                    auto& bbox = bulletEntry->entity.m_location.bBox();

                    //When altering mass we need to first remove and then re-add the body.
                    m_dynamicsWorld->removeCollisionObject(bulletEntry->collisionObject.get());

                    float mass = getMassForEntity(bulletEntry->entity);
                    //"fixed" mode means that the entity stays in place, always
                    //"planted" mode means it's planted in the ground
                    //Zero mass makes the rigid body static
                    if (modeProp->getMode() == ModeProperty::Mode::Planted || modeProp->getMode() == ModeProperty::Mode::Fixed || mass == 0) {

                        if ((rigidBody->getCollisionFlags() & btCollisionObject::CF_STATIC_OBJECT) == 0
                            && rigidBody->getCollisionShape()->getShapeType() == CONVEX_HULL_SHAPE_PROXYTYPE) {
                            //If the shape is a mesh, and it previously wasn't static, we need to replace the shape with an optimized one.
                            bulletEntry->collisionShape = createCollisionShapeForEntry(bulletEntry->entity, bbox, mass, bulletEntry->centerOfMassOffset);
                            rigidBody->setCollisionShape(bulletEntry->collisionShape.get());
                        }

                        rigidBody->setMassProps(0, btVector3(0, 0, 0));

                    } else {
                        //Detach from any attached entity

                        plantOnEntity(bulletEntry, nullptr);

                        //The entity is free
                        if (rigidBody->getCollisionFlags() & btCollisionObject::CF_STATIC_OBJECT
                            && rigidBody->getCollisionShape()->getShapeType() == SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE) {
                            //If the shape is a mesh, and it previously was static, we need to replace the shape with an optimized one.
                            bulletEntry->collisionShape = createCollisionShapeForEntry(bulletEntry->entity, bbox, mass, bulletEntry->centerOfMassOffset);
                            rigidBody->setCollisionShape(bulletEntry->collisionShape.get());
                        }

                        btVector3 inertia(0, 0, 0);
                        bulletEntry->collisionShape->calculateLocalInertia(mass, inertia);

                        rigidBody->setMassProps(mass, inertia);

                        //If there are attached entities, we must also make them free.
                        auto attachedEntitiesCopy = bulletEntry->attachedEntities;
                        for (auto attachedEntry : attachedEntitiesCopy) {
                            attachedEntry->entity.setAttrValue("mode", modeProp->data());
                        }
                        if (!bulletEntry->attachedEntities.empty()) {
                            log(WARNING, "Set of attached entities isn't empty after changing all of them to free mode.");
                        }
                    }
                    //It's crucial we call this when changing mass, otherwise we might get divide-by-zero in the simulation
                    rigidBody->updateInertiaTensor();
                    short collisionMask;
                    short collisionGroup;
                    getCollisionFlagsForEntity(bulletEntry->entity, collisionGroup, collisionMask);

                    m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);

                    bulletEntry->collisionObject->activate();
                }
                //Since we've deactivated automatic updating of all aabbs each tick we need to do it ourselves when updating the position.
                m_dynamicsWorld->updateSingleAabb(bulletEntry->collisionObject.get());

                bulletEntry->mode = modeProp->getMode();
            }


            //sendMoveSight(*bulletEntry);
        }
        if (!bulletEntry->addedToMovingList) {
            m_movingEntities.emplace_back(bulletEntry);
            bulletEntry->addedToMovingList = true;
            bulletEntry->markedAsMovingThisFrame = false;
            bulletEntry->markedAsMovingLastFrame = false;
        }
        return;
    } else if (name == SolidProperty::property_name) {
        if (bulletEntry->collisionObject) {
            auto rigidBody = btRigidBody::upcast(bulletEntry->collisionObject.get());
            if (rigidBody) {
                short collisionMask;
                short collisionGroup;
                getCollisionFlagsForEntity(bulletEntry->entity, collisionGroup, collisionMask);
                m_dynamicsWorld->removeRigidBody(rigidBody);
                m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);

                bulletEntry->collisionObject->activate();
            }
        }
    } else if (name == "mass") {
        const auto* modeProp = bulletEntry->entity.getPropertyClassFixed<ModeProperty>();

        if (modeProp && (modeProp->getMode() == ModeProperty::Mode::Planted || modeProp->getMode() == ModeProperty::Mode::Fixed)) {
            //"fixed" mode means that the entity stays in place, always
            //"planted" mode means it's planted in the ground
            //Zero mass makes the rigid body static
        } else {
            if (bulletEntry->collisionObject) {
                auto rigidBody = btRigidBody::upcast(bulletEntry->collisionObject.get());
                if (rigidBody) {
                    //When altering mass we need to first remove and then re-add the body.
                    m_dynamicsWorld->removeRigidBody(rigidBody);

                    short collisionMask;
                    short collisionGroup;
                    getCollisionFlagsForEntity(bulletEntry->entity, collisionGroup, collisionMask);

                    float mass = getMassForEntity(bulletEntry->entity);
                    btVector3 inertia;
                    bulletEntry->collisionShape->calculateLocalInertia(mass, inertia);

                    rigidBody->setMassProps(mass, inertia);
                    //It's crucial we call this when changing mass, otherwise we might get divide-by-zero in the simulation
                    rigidBody->updateInertiaTensor();

                    m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);
                }
            }
        }

    } else if (name == BBoxProperty::property_name || name == ScaleProperty::property_name) {
        const auto& bbox = bulletEntry->entity.m_location.bBox();
        if (bbox.isValid()) {

            if (bulletEntry->visibilitySphere) {
                auto& entity = bulletEntry->entity;
                auto radius = calculateVisibilitySphereRadius(entity);

                if (radius != bulletEntry->visibilityShape->getImplicitShapeDimensions().x()) {
                    bulletEntry->visibilityShape->setUnscaledRadius(radius);
                    if (!bulletEntry->markedForVisibilityRecalculation) {
                        m_visibilityRecalculateQueue.emplace_back(bulletEntry);
                        bulletEntry->markedForVisibilityRecalculation = true;
                    }
                }
            }

            if (bulletEntry->collisionObject) {
                //When changing shape dimensions we must first remove the object, do the change, and then add it back again.

                //Note that we can't just call setLocalScaling since it doesn't seem to work well with mesh shapes.
                auto rigidBody = btRigidBody::upcast(bulletEntry->collisionObject.get());
                if (rigidBody) {
                    m_dynamicsWorld->removeRigidBody(rigidBody);
                } else {
                    m_dynamicsWorld->removeCollisionObject(bulletEntry->collisionObject.get());
                }
                float mass = getMassForEntity(bulletEntry->entity);

                bulletEntry->collisionShape = createCollisionShapeForEntry(bulletEntry->entity, bbox, mass, bulletEntry->centerOfMassOffset);
                bulletEntry->collisionObject->setCollisionShape(bulletEntry->collisionShape.get());
                if (bulletEntry->motionState) {
                    bulletEntry->motionState->m_centerOfMassOffset = btTransform(btQuaternion::getIdentity(), bulletEntry->centerOfMassOffset);
                }
                bulletEntry->collisionObject->activate();

                short collisionMask;
                short collisionGroup;
                getCollisionFlagsForEntity(bulletEntry->entity, collisionGroup, collisionMask);

                if (rigidBody) {
                    m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);
                } else {
                    m_dynamicsWorld->addCollisionObject(bulletEntry->collisionObject.get(), collisionGroup, collisionMask);
                }

                applyNewPositionForEntity(bulletEntry, bulletEntry->entity.m_location.pos());

                m_dynamicsWorld->updateSingleAabb(bulletEntry->collisionObject.get());
            }


        }
    } else if (name == "planted_offset" || name == "planted_scaled_offset") {
        applyNewPositionForEntity(bulletEntry, bulletEntry->entity.m_location.m_pos);
        bulletEntry->entity.m_location.update(BaseWorld::instance().getTimeAsSeconds());
        bulletEntry->entity.removeFlags(entity_clean);
        if (bulletEntry->collisionObject) {
            m_dynamicsWorld->updateSingleAabb(bulletEntry->collisionObject.get());
        }
        sendMoveSight(*bulletEntry, true, false, false, false, false);
    } else if (name == TerrainModProperty::property_name) {
        updateTerrainMod(bulletEntry->entity, true);
    } else if (name == "speed_ground") {
        bulletEntry->speedGround = dynamic_cast<const Property<double>*>(&prop)->data();
    } else if (name == "speed_water") {
        bulletEntry->speedWater = dynamic_cast<const Property<double>*>(&prop)->data();
    } else if (name == "speed_flight") {
        bulletEntry->speedFlight = dynamic_cast<const Property<double>*>(&prop)->data();
    } else if (name == "floats") {
        //TODO: find a better way to handle entities which are set to float. Perhaps it should just be handled by density?
//        applyNewPositionForEntity(bulletEntry, bulletEntry->entity.m_location.m_pos);
//        bulletEntry->entity.m_location.update(BaseWorld::instance().getTimeAsSeconds());
//        bulletEntry->entity.removeFlags(entity_clean);
//        if (bulletEntry->collisionObject) {
//            m_dynamicsWorld->updateSingleAabb(bulletEntry->collisionObject.get());
//        }
//        sendMoveSight(*bulletEntry, true, false, false, false, false);
    } else if (name == "step_factor") {
        auto stepFactorProp = dynamic_cast<const Property<double>*>(&prop);
        auto I = m_steppingEntries.find(bulletEntry->entity.getIntId());
        if (stepFactorProp && stepFactorProp->data() > 0) {
            if (I != m_steppingEntries.end()) {
                I->second.second = static_cast<float>(stepFactorProp->data());
            } else {
                m_steppingEntries.emplace(bulletEntry->entity.getIntId(), std::make_pair(bulletEntry, stepFactorProp->data()));
            }
        } else {
            if (I != m_steppingEntries.end()) {
                m_steppingEntries.erase(I);
            }
        }
    } else if (name == PerceptionSightProperty::property_name) {
        toggleChildPerception(bulletEntry->entity);
    } else if (name == ModeDataProperty::property_name) {
        applyNewPositionForEntity(bulletEntry, bulletEntry->entity.m_location.m_pos, true);
        sendMoveSight(*bulletEntry, true, false, false, false, false);
    }
}

void PhysicalDomain::updateTerrainMod(const LocatedEntity& entity, bool forceUpdate)
{
    //TODO store as flag in BulletEntry so we don't need to do lookup
    auto modeProp = entity.getPropertyClassFixed<ModeProperty>();
    if (modeProp) {
        if (modeProp->getMode() == ModeProperty::Mode::Planted) {
            auto terrainModProperty = entity.getPropertyClassFixed<TerrainModProperty>();
            if (terrainModProperty && m_terrain) {
                //We need to get the vertical position in the terrain, without any mods.
                Mercator::Segment* segment = m_terrain->getSegmentAtPos(entity.m_location.m_pos.x(), entity.m_location.m_pos.z());
                WFMath::Point<3> modPos = entity.m_location.m_pos;
                if (segment) {
                    std::vector<WFMath::AxisBox<2>> terrainAreas;

                    float height;
                    //If there's no mods we can just use position right away
                    if (segment->getMods().empty()) {
                        if (!segment->isValid()) {
                            segment->populate();
                        }
                        segment->getHeight(modPos.x() - (segment->getXRef()), modPos.z() - (segment->getZRef()), height);
                    } else {
                        Mercator::HeightMap heightMap((unsigned int) segment->getResolution());
                        heightMap.allocate();
                        segment->populateHeightMap(heightMap);

                        heightMap.getHeight(modPos.x() - (segment->getXRef()), modPos.z() - (segment->getZRef()), height);
                    }

                    modPos.y() = height;

                    auto I = m_terrainMods.find(entity.getIntId());
                    if (I != m_terrainMods.end()) {
                        const WFMath::Point<3>& oldPos = std::get<1>(I->second);
                        const WFMath::Quaternion& oldOrient = std::get<2>(I->second);

                        if (!oldOrient.isEqualTo(entity.m_location.m_orientation) || !oldPos.isEqualTo(modPos)) {
                            //Need to update terrain mod
                            forceUpdate = true;
                            const WFMath::AxisBox<2>& oldArea = std::get<3>(I->second);
                            if (oldArea.isValid()) {
                                terrainAreas.push_back(oldArea);
                            }
                        }
                    } else {
                        forceUpdate = true;
                    }

                    if (forceUpdate) {
                        auto modifier = terrainModProperty->parseModData(modPos, entity.m_location.m_orientation);

                        m_terrain->updateMod(entity.getIntId(), modifier.get());
                        if (modifier) {
                            auto bbox = modifier->bbox();
                            terrainAreas.push_back(bbox);
                            m_terrainMods[entity.getIntId()] = std::make_tuple(std::move(modifier), modPos, entity.m_location.m_orientation, bbox);
                        } else {
                            m_terrainMods.erase(entity.getIntId());
                        }

                        refreshTerrain(terrainAreas);
                    }
                }
            }
        } else {
            //Make sure the terrain mod is removed if the entity isn't planted
            auto I = m_terrainMods.find(entity.getIntId());
            if (I != m_terrainMods.end()) {
                std::vector<WFMath::AxisBox<2>> terrainAreas;
                terrainAreas.emplace_back(std::get<0>(I->second)->bbox());
                m_terrain->updateMod(entity.getIntId(), nullptr);
                m_terrainMods.erase(I);
                refreshTerrain(terrainAreas);
            }
        }
    }
}

void PhysicalDomain::getCollisionFlagsForEntity(const LocatedEntity& entity, short& collisionGroup, short& collisionMask) const
{
    //The "group" defines the features of this object, which other bodies can mask out.
    //The "mask" defines the other kind of object this body will react with.

    //Water bodies behave in a special way, so check for that.
    auto waterBodyProp = entity.getPropertyClass<BoolProperty>("water_body");
    if (waterBodyProp && waterBodyProp->isTrue()) {
        //A body of water should behave like terrain, and interact with both physical and non-physical entities.
        collisionGroup = COLLISION_MASK_TERRAIN;
        collisionMask = COLLISION_MASK_NON_PHYSICAL | COLLISION_MASK_PHYSICAL;
    } else {
        auto modeProp = entity.getPropertyClassFixed<ModeProperty>();
        if (modeProp && modeProp->getMode() == ModeProperty::Mode::Fixed) {
            if (entity.m_location.isSolid()) {
                collisionGroup = COLLISION_MASK_STATIC;
                //Fixed entities shouldn't collide with anything themselves.
                //Other physical entities should however collide with them.
                collisionMask = COLLISION_MASK_PHYSICAL;
            } else {
                //The object is both fixed and not solid. It shouldn't collide with anything at all.
                collisionGroup = 0;
                collisionMask = 0;
            }
        } else if (modeProp && modeProp->getMode() == ModeProperty::Mode::Planted) {
            if (entity.m_location.isSolid()) {
                collisionGroup = COLLISION_MASK_STATIC;
                //Planted entities shouldn't collide with anything themselves except the terrain.
                //Other physical entities should however collide with them.
                collisionMask = COLLISION_MASK_PHYSICAL | COLLISION_MASK_TERRAIN;
            } else {
                //The object is both planted and not solid. It shouldn't collide with anything at all except terrain.
                collisionGroup = 0;
                collisionMask = COLLISION_MASK_TERRAIN;
            }
        } else {
            if (entity.m_location.isSolid()) {
                //This is a physical object
                collisionGroup = COLLISION_MASK_PHYSICAL;
                //In this case other physical moving objects, the terrain and all static objects.
                collisionMask = COLLISION_MASK_PHYSICAL | COLLISION_MASK_TERRAIN | COLLISION_MASK_STATIC;
            } else {
                //Non solid objects should collide with the terrain only.
                //Mark the object as non-physical
                collisionGroup = COLLISION_MASK_NON_PHYSICAL;
                //And only collide with the terrain.
                collisionMask = COLLISION_MASK_TERRAIN;
            }
        }
    }
}

void PhysicalDomain::entityPropertyApplied(const std::string& name, const PropertyBase& prop)
{
    if (name == "friction") {
        auto frictionProp = dynamic_cast<const Property<double>*>(&prop);
        for (auto& entry : m_terrainSegments) {
            entry.second.rigidBody->setFriction(static_cast<btScalar>(frictionProp->data()));
        }
    } else if (name == "friction_roll") {
        auto frictionRollingProp = dynamic_cast<const Property<double>*>(&prop);
        for (auto& entry : m_terrainSegments) {
            entry.second.rigidBody->setRollingFriction(static_cast<btScalar>(frictionRollingProp->data()));
        }
    } else if (name == "friction_spin") {
#if BT_BULLET_VERSION < 285
        log(WARNING, "Your version of Bullet doesn't support spinning friction.");
#else
        auto frictionSpinningProp = dynamic_cast<const Property<double>*>(&prop);
        for (auto& entry : m_terrainSegments) {
            entry.second.rigidBody->setSpinningFriction(static_cast<btScalar>(frictionSpinningProp->data()));
        }
#endif
    } else if (name == TerrainProperty::property_name) {
        auto terrainProperty = m_entity.getPropertyClassFixed<TerrainProperty>();
        if (terrainProperty) {
            m_terrain = &terrainProperty->getData(m_entity);
        }
    }
}

void PhysicalDomain::calculatePositionForEntity(ModeProperty::Mode mode, PhysicalDomain::BulletEntry* entry, WFMath::Point<3>& pos)
{
    struct PlantedOnCallback : public btCollisionWorld::ContactResultCallback
    {
        btVector3 highestPoint;
        bool hadHit = false;
        const btCollisionObject* highestObject = nullptr;

        explicit PlantedOnCallback(btVector3 highestPoint_in)
                : btCollisionWorld::ContactResultCallback(), highestPoint(highestPoint_in)
        {
        }

        btScalar addSingleResult(btManifoldPoint& cp,
                                 const btCollisionObjectWrapper* colObj0, int partId0, int index0,
                                 const btCollisionObjectWrapper* colObj1, int partId1, int index1) override
        {

            //B will be the existing planted object, A will be the object being planted.
            btVector3 point = cp.getPositionWorldOnB();

            if (point.y() > highestPoint.y()) {
                highestPoint = point;
                highestObject = colObj1->m_collisionObject;
                hadHit = true;
            }

            //Returned result is ignored.
            return 0;
        }
    };

    struct PlaceOnManyCallback : public btCollisionWorld::ConvexResultCallback
    {
        btVector3 highestPoint;
        const btCollisionObject* highestObject = nullptr;
        btCollisionObject* testedCollisionObject = nullptr;

        bool needsCollision(btBroadphaseProxy* proxy0) const override
        {
            //Check that we don't collide with ourselves.
            if (testedCollisionObject == static_cast<btCollisionObject*>(proxy0->m_clientObject)) {
                return false;
            }
            return ConvexResultCallback::needsCollision(proxy0);
        }

        btScalar addSingleResult(btCollisionWorld::LocalConvexResult& convexResult, bool normalInWorldSpace) override
        {
            if (convexResult.m_hitPointLocal.y() > highestPoint.y()) {
                highestPoint = convexResult.m_hitPointLocal;
                highestObject = convexResult.m_hitCollisionObject;
            }

            //Returned result is ignored.
            return 0;
        }
    };

    auto& entity = entry->entity;

    if (mode == ModeProperty::Mode::Planted || mode == ModeProperty::Mode::Free || mode == ModeProperty::Mode::Submerged) {
        auto collisionObject = entry->collisionObject.get();
        float h = pos.y();
        getTerrainHeight(pos.x(), pos.z(), h);

        if (mode == ModeProperty::Mode::Planted) {

            bool plantedOn = false;
            if (collisionObject) {

                if (!plantedOn) {

                    auto modeDataProp = entity.getPropertyClassFixed<ModeDataProperty>();
                    if (modeDataProp && modeDataProp->getMode() == ModeProperty::Mode::Planted) {
                        if (modeDataProp->getPlantedOnData().entityId) {
                            auto plantedOnId = *modeDataProp->getPlantedOnData().entityId;
                            if (plantedOnId == entity.getIntId()) {
                                log(WARNING, String::compose("Entity %1 was marked to be planted on itself.", entity.describeEntity()));
                            } else {
                                //If it's desired that the entity should be planted on the ground then make it so.
                                //Since the ground is everywhere.
                                if (plantedOnId == m_entity.getIntId()) {
                                    plantOnEntity(entry, &mContainingEntityEntry);

                                    pos.y() = h;
                                    plantedOn = true;
                                } else {
                                    //Otherwise we need to check that it really can be planted on what it's planted on.
                                    auto I = m_entries.find(plantedOnId);
                                    if (I != m_entries.end()) {
                                        auto& plantedOnBulletEntry = I->second;

                                        //Check if it's a water body it's planted on first.
                                        if (plantedOnBulletEntry->collisionObject->getUserIndex() == USER_INDEX_WATER_BODY) {

                                            //We're planted on a water body, we should place ourselves on top of it.
                                            float newHeight = plantedOnBulletEntry->entity.m_location.pos().y();
                                            if (plantedOnBulletEntry->entity.m_location.bBox().isValid()) {
                                                newHeight += plantedOnBulletEntry->entity.m_location.bBox().highCorner().y();
                                            }

                                            if (newHeight > h) {
                                                //Make sure it's not under the terrain.
                                                pos.y() = newHeight;
                                                plantedOn = true;
                                                plantOnEntity(entry, plantedOnBulletEntry.get());
                                            }
                                        } else {

                                            //Check if it collides with the entity it's marked as being planted on by moving it downwards until
                                            //it collides with the entity or the ground.
                                            btVector3 centerOfMassOffset;
                                            auto placementShape = createCollisionShapeForEntry(entry->entity, entry->entity.m_location.bBox(), 1, centerOfMassOffset);
                                            collisionObject->setCollisionShape(placementShape.get());

                                            btVector3 aabbMin, aabbMax;
                                            collisionObject->getCollisionShape()->getAabb(collisionObject->getWorldTransform(), aabbMin, aabbMax);
                                            float height = aabbMax.y() - aabbMin.y();

                                            float yPos = pos.y();

                                            btQuaternion orientation = entity.m_location.m_orientation.isValid() ? Convert::toBullet(entity.m_location.m_orientation) : btQuaternion::getIdentity();
                                            btTransform transform(orientation, Convert::toBullet(entry->entity.m_location.pos()));
                                            transform *= btTransform(btQuaternion::getIdentity(), entry->centerOfMassOffset).inverse();

                                            collisionObject->setWorldTransform(transform);

                                            while (yPos > h) {
                                                PlantedOnCallback callback(btVector3(pos.x(), h, pos.z()));
                                                callback.m_collisionFilterGroup = COLLISION_MASK_PHYSICAL;
                                                callback.m_collisionFilterMask = COLLISION_MASK_STATIC;

                                                //Test if the shape collides, otherwise move it downwards until it reaches the ground.
                                                collisionObject->getWorldTransform().getOrigin().setY(yPos);
                                                m_dynamicsWorld->contactPairTest(collisionObject, plantedOnBulletEntry->collisionObject.get(), callback);

                                                if (callback.hadHit) {
                                                    pos.y() = std::max(callback.highestPoint.y(), h);
                                                    plantedOn = true;
                                                    plantOnEntity(entry, plantedOnBulletEntry.get());
                                                    break;
                                                }

                                                yPos -= height;
                                            }

                                            collisionObject->setCollisionShape(entry->collisionShape.get());
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (!plantedOn) {

                        btQuaternion orientation = entity.m_location.m_orientation.isValid() ? Convert::toBullet(entity.m_location.m_orientation) : btQuaternion::getIdentity();
                        btTransform transformFrom(orientation, Convert::toBullet(pos));
                        btTransform transformTo(transformFrom);
                        transformTo.getOrigin().setY(h);
                        transformFrom *= btTransform(btQuaternion::getIdentity(), entry->centerOfMassOffset).inverse();
                        transformTo *= btTransform(btQuaternion::getIdentity(), entry->centerOfMassOffset).inverse();

                        if (!((transformFrom.getOrigin() - transformTo.getOrigin()).fuzzyZero())) {

                            btVector3 centerOfMassOffset;
                            auto placementShape = createCollisionShapeForEntry(entry->entity, entry->entity.m_location.bBox(), 1, centerOfMassOffset);
                            auto convexShape = dynamic_cast<btConvexShape*>(placementShape.get());
                            if (convexShape) {

                                PlaceOnManyCallback callback{};
                                callback.m_collisionFilterGroup = COLLISION_MASK_PHYSICAL;
                                callback.m_collisionFilterMask = COLLISION_MASK_STATIC;
                                callback.highestPoint = btVector3(pos.x(), h, pos.z());
                                callback.testedCollisionObject = collisionObject;

                                m_dynamicsWorld->convexSweepTest(convexShape, transformFrom, transformTo, callback);

                                if (callback.highestObject) {
                                    auto plantedOnEntry = static_cast<BulletEntry*>(callback.highestObject->getUserPointer());
                                    if (plantedOnEntry) {
                                        assert(plantedOnEntry->entity.getId() != entity.getId());
                                        pos.y() = std::max(callback.highestPoint.y(), h);
                                        plantedOn = true;

                                        plantOnEntity(entry, plantedOnEntry);
                                    }
                                }
                            } else {
                                log(WARNING, String::compose("Did not get a convex shape when creating placement shape, for entity %1.", entity.describeEntity()));
                            }
                        }
                    }
                }
            }


            //If we couldn't find anything to plant on, make sure it's planted on the ground.
            if (!plantedOn) {
                plantOnEntity(entry, &mContainingEntityEntry);

                pos.y() = h;
            }

            auto plantedOffsetProp = entity.getPropertyType<double>("planted_offset");
            if (plantedOffsetProp) {
                pos.y() += plantedOffsetProp->data();
            }
            auto plantedScaledOffsetProp = entity.getPropertyType<double>("planted_scaled_offset");
            if (plantedScaledOffsetProp && entity.m_location.bBox().isValid()) {
                auto size = entity.m_location.bBox().highCorner() - entity.m_location.bBox().lowCorner();

                pos.y() += (plantedScaledOffsetProp->data() * size.y());
            }
        } else if (mode == ModeProperty::Mode::Free || mode == ModeProperty::Mode::Submerged) {
            //For free entities we only want to clamp to terrain if the entity is below it
            pos.y() = std::max(pos.y(), static_cast<double>(h));
        }
    } else if (mode == ModeProperty::Mode::Free) {
        float h = pos.y();
        getTerrainHeight(pos.x(), pos.z(), h);
        pos.y() = h;
    } else if (mode == ModeProperty::Mode::Fixed || mode == ModeProperty::Mode::Projectile) {
        //Don't do anything to adjust height
    } else {
        log(WARNING, "Unknown mode for entity " + entity.describeEntity());
    }
}

void PhysicalDomain::applyNewPositionForEntity(BulletEntry* entry, const WFMath::Point<3>& pos, bool calculatePosition)
{
    if (!pos.isValid()) {
        return;
    }
    btCollisionObject* collObject = entry->collisionObject.get();
    LocatedEntity& entity = entry->entity;

    ModeProperty::Mode mode = ModeProperty::Mode::Free;
    auto modeProp = entity.getPropertyClassFixed<ModeProperty>();
    if (modeProp) {
        mode = modeProp->getMode();
    }

    WFMath::Point<3> newPos = pos;

    if (calculatePosition) {
        calculatePositionForEntity(mode, entry, newPos);
    }

    entity.m_location.m_pos = newPos;

    if (collObject) {
        btTransform& transform = collObject->getWorldTransform();

        debug_print("PhysicalDomain::new pos " << entity.describeEntity() << " " << pos)

        transform.setOrigin(Convert::toBullet(newPos));
        transform *= btTransform(btQuaternion::getIdentity(), entry->centerOfMassOffset).inverse();

        collObject->setWorldTransform(transform);
    }

    if (entry->viewSphere) {
        entry->viewSphere->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(entity.m_location.m_pos) * VISIBILITY_SCALING_FACTOR));
        m_visibilityWorld->updateSingleAabb(entry->viewSphere.get());
    }
    if (entry->visibilitySphere) {
        entry->visibilitySphere->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(entity.m_location.m_pos) * VISIBILITY_SCALING_FACTOR));
        m_visibilityWorld->updateSingleAabb(entry->visibilitySphere.get());
    }

    if (!entry->markedForVisibilityRecalculation) {
        m_visibilityRecalculateQueue.emplace_back(entry);
        entry->markedForVisibilityRecalculation = true;
    }
}

void PhysicalDomain::applyPropel(BulletEntry& entry, const WFMath::Vector<3>& propel)
{
    /**
     * A callback which checks if the instance is "grounded", i.e. that there's a contact point which is below its center.
     */
    struct IsGroundedCallback : public btCollisionWorld::ContactResultCallback
    {
        const btRigidBody& m_body;
        bool& m_isGrounded;

        IsGroundedCallback(const btRigidBody& body, bool& isGrounded)
                : btCollisionWorld::ContactResultCallback(), m_body(body), m_isGrounded(isGrounded)
        {
            m_collisionFilterGroup = body.getBroadphaseHandle()->m_collisionFilterGroup;
            m_collisionFilterMask = body.getBroadphaseHandle()->m_collisionFilterMask;
        }

        bool needsCollision(btBroadphaseProxy* proxy0) const override
        {
            bool collides = (proxy0->m_collisionFilterGroup & m_collisionFilterMask) != 0;
            collides = collides && (m_collisionFilterGroup & proxy0->m_collisionFilterMask);
            //Discount water bodies.
            return collides && ((btCollisionObject*) proxy0->m_clientObject)->getUserIndex() != USER_INDEX_WATER_BODY;
        }

        btScalar addSingleResult(btManifoldPoint& cp,
                                 const btCollisionObjectWrapper* colObj0, int partId0, int index0,
                                 const btCollisionObjectWrapper* colObj1, int partId1, int index1) override
        {
            //Local collision point, in the body's space
            btVector3 point;
            if (colObj0->m_collisionObject == &m_body) {
                point = cp.m_localPointA;
            } else {
                point = cp.m_localPointB;
            }

            if (point.y() <= 0) {
                m_isGrounded = true;
            }

            //Returned result is ignored.
            return 0;
        }
    };

    if (propel.isValid()) {
        if (entry.collisionObject) {
            auto rigidBody = btRigidBody::upcast(entry.collisionObject.get());
            if (rigidBody) {
                LocatedEntity& entity = entry.entity;

                debug_print("PhysicalDomain::applyPropel " << entity.describeEntity() << " " << propel << " " << propel.mag())

                btVector3 btPropel = Convert::toBullet(propel);

                //TODO: add support for flying and swimming
                if (!btPropel.isZero()) {

                    //Check if we're trying to jump
                    if (btPropel.m_floats[1] > 0) {
                        auto jumpSpeedProp = entity.getPropertyType<double>("speed_jump");
                        if (jumpSpeedProp && jumpSpeedProp->data() > 0) {

                            bool isGrounded = false;
                            IsGroundedCallback groundedCallback(*rigidBody, isGrounded);
                            m_dynamicsWorld->contactTest(rigidBody, groundedCallback);
                            if (isGrounded) {
                                //If the entity is grounded, allow it to jump by setting the vertical velocity.
                                btVector3 newVelocity = rigidBody->getLinearVelocity();
                                newVelocity.m_floats[1] = static_cast<btScalar>(btPropel.m_floats[1] * jumpSpeedProp->data());
                                rigidBody->setLinearVelocity(newVelocity);
                                //We'll mark the entity as actively jumping here, and rely on the post-tick callback to reset it when it's not jumping anymore.
                                entry.isJumping = true;
                            }
                        }
                    }
                    btPropel.m_floats[1] = 0; //Don't allow vertical velocity to be set for the continuous velocity.

                    auto K = m_propellingEntries.find(entity.getIntId());
                    if (K == m_propellingEntries.end()) {
                        const Property<double>* stepFactorProp = entity.getPropertyType<double>("step_factor");
                        if (stepFactorProp && entity.m_location.bBox().isValid()) {
                            auto height = entity.m_location.bBox().upperBound(1) - entity.m_location.bBox().lowerBound(1);
                            m_propellingEntries.insert(std::make_pair(entity.getIntId(), PropelEntry{rigidBody, &entry, btPropel, (float) (height * stepFactorProp->data())}));
                        } else {
                            m_propellingEntries.insert(std::make_pair(entity.getIntId(), PropelEntry{rigidBody, &entry, btPropel, 0}));
                        }
                    } else {
                        K->second.velocity = btPropel;
                    }
                } else {
                    btVector3 bodyVelocity = rigidBody->getLinearVelocity();
                    bodyVelocity.setX(0);
                    bodyVelocity.setZ(0);

                    if (rigidBody->getCenterOfMassPosition().y() <= 0) {
                        bodyVelocity.setY(0);
                    }

                    rigidBody->setLinearVelocity(bodyVelocity);
                    double friction = 1.0; //Default to 1 if no "friction" prop is present.
                    auto frictionProp = entity.getPropertyType<double>("friction");
                    if (frictionProp) {
                        friction = frictionProp->data();
                    }
                    rigidBody->setFriction(static_cast<btScalar>(friction));

                    m_propellingEntries.erase(entity.getIntId());
                }
            }
        }
    }
}

void PhysicalDomain::applyTransform(LocatedEntity& entity, const TransformData& transformData,
                                    std::set<LocatedEntity*>& transformedEntities)
{
    //First handle any changes to the "mode_data" property, to see if entities are planted.
    auto I = m_entries.find(entity.getIntId());
    auto& entry = I->second;
    BulletEntry* entryPlantedOn = nullptr;

    if (transformData.plantedOnEntity) {
        if (transformData.plantedOnEntity == &entity) {
            log(WARNING, String::compose("Tried to plant entity %1 on itself.", entity.describeEntity()));
        } else {
            auto plantedOnI = m_entries.find(transformData.plantedOnEntity->getIntId());
            if (plantedOnI != m_entries.end()) {
                entryPlantedOn = plantedOnI->second.get();
            }
        }
    }
    plantOnEntity(entry.get(), entryPlantedOn);

    applyTransformInternal(entity, transformData.orientation, transformData.pos, transformData.impulseVelocity, transformedEntities, true);
}

void PhysicalDomain::applyTransformInternal(LocatedEntity& entity,
                                            const WFMath::Quaternion& orientation,
                                            const WFMath::Point<3>& pos,
                                            const WFMath::Vector<3>& impulseVelocity,
                                            std::set<LocatedEntity*>& transformedEntities,
                                            bool calculatePosition)
{
    WFMath::Point<3> oldPos = entity.m_location.m_pos;

    auto I = m_entries.find(entity.getIntId());
    if (I == m_entries.end()) {
        //This could happen if the entity didn't have any position, for example.
        return;
    }
    bool hadChange = false;
    auto& entry = I->second;
    btRigidBody* rigidBody = nullptr;
    if (entry->collisionObject) {
        rigidBody = btRigidBody::upcast(entry->collisionObject.get());
    }
    WFMath::Quaternion rotationChange = WFMath::Quaternion::IDENTITY();
    if (orientation.isValid() && !orientation.isEqualTo(entity.m_location.m_orientation, 0.001)) {
        debug_print("PhysicalDomain::new orientation " << entity.describeEntity() << " " << orientation)

        if (entry->collisionShape) {
            btTransform& transform = entry->collisionObject->getWorldTransform();

            transform.setRotation(Convert::toBullet(orientation));
            transform.setOrigin(Convert::toBullet(entry->entity.m_location.pos()));
            transform *= btTransform(btQuaternion::getIdentity(), entry->centerOfMassOffset).inverse();

            entry->collisionObject->setWorldTransform(transform);
        }
        if (entity.m_location.m_orientation.isValid()) {
            rotationChange = orientation * entity.m_location.m_orientation.inverse();
        } else {
            rotationChange = orientation;
        }
        entity.m_location.m_orientation = orientation;
        entity.removeFlags(entity_orient_clean);
        hadChange = true;
    }
    if (pos.isValid()) {
        applyNewPositionForEntity(entry.get(), pos, calculatePosition);
        if (!oldPos.isEqualTo(entity.m_location.m_pos)) {
            entity.removeFlags(entity_pos_clean);
            hadChange = true;
            //Check if there previously wasn't any valid pos, and thus no valid collision instances.
            if (entity.m_location.m_pos.isValid() && !oldPos.isValid()) {
                if (entry->collisionObject) {
                    short collisionMask;
                    short collisionGroup;
                    getCollisionFlagsForEntity(entity, collisionGroup, collisionMask);
                    if (rigidBody) {
                        m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);
                    } else {
                        m_dynamicsWorld->addCollisionObject(entry->collisionObject.get(), collisionGroup, collisionMask);
                    }
                }
                if (entry->viewSphere) {
                    m_visibilityWorld->addCollisionObject(entry->viewSphere.get(),
                                                          entity.hasFlags(entity_admin) ? VISIBILITY_MASK_OBSERVABLE | VISIBILITY_MASK_OBSERVABLE_PRIVATE : VISIBILITY_MASK_OBSERVABLE,
                                                          VISIBILITY_MASK_OBSERVER);
                }
                if (entry->visibilitySphere) {
                    m_visibilityWorld->addCollisionObject(entry->visibilitySphere.get(), VISIBILITY_MASK_OBSERVER,
                                                          entity.hasFlags(entity_visibility_protected) || entity.hasFlags(entity_visibility_private) ? VISIBILITY_MASK_OBSERVABLE_PRIVATE
                                                                                                                                                     : VISIBILITY_MASK_OBSERVABLE);
                }
            }
        }
    }
    if (impulseVelocity.isValid() && rigidBody) {
        rigidBody->applyCentralImpulse(Convert::toBullet(impulseVelocity));
    }

    if (hadChange) {

        if (entry->collisionObject && entry->collisionObject->getUserIndex() == USER_INDEX_WATER_BODY) {
            //We moved a water body, check with all entities that might be contained.
            //This is quite expensive since we're iterating through all entities (on an unordered map).
            //However, since we normally never expect a water body to move on a live server we do it this way, since we then avoid
            //having to keep track of which entities are in water and not.
            for (auto& entityEntry : m_entries) {
                auto childEntry = entityEntry.second.get();
                if (childEntry->waterNearby == entry.get()) {
                    if (!childEntry->addedToMovingList) {
                        m_movingEntities.emplace_back(childEntry);
                        childEntry->addedToMovingList = true;
                        childEntry->markedAsMovingThisFrame = false;
                        childEntry->markedAsMovingLastFrame = false;
                    }
                }
            }
        }

        //Only check for resting entities if the entity has been moved; not if the velocity has changed.
        if (pos.isValid() || orientation.isValid()) {
            transformedEntities.insert(&entry->entity);
            transformRestingEntities(entry.get(), entry->entity.m_location.m_pos - oldPos, rotationChange, transformedEntities);
        }
        if (entry->collisionShape) {
            //Since we've deactivated automatic updating of all aabbs each tick we need to do it ourselves when updating the position.
            m_dynamicsWorld->updateSingleAabb(entry->collisionObject.get());

            if (rigidBody && rigidBody->getInvMass() != 0) {
                rigidBody->activate();
            }
        }
        processMovedEntity(*I->second, 0);
    }
}

void PhysicalDomain::refreshTerrain(const std::vector<WFMath::AxisBox<2>>& areas)
{
    //Schedule dirty terrain areas for update in processDirtyTerrainAreas() which is called for each tick.
    m_dirtyTerrainAreas.insert(m_dirtyTerrainAreas.end(), areas.begin(), areas.end());
}

void PhysicalDomain::processDirtyTerrainAreas()
{
    if (!m_terrain) {
        m_dirtyTerrainAreas.clear();
        return;
    }

    if (m_dirtyTerrainAreas.empty()) {
        return;
    }
    rmt_ScopedCPUSample(PhysicalDomain_processDirtyTerrainAreas, 0)

    std::set<Mercator::Segment*> dirtySegments;
    for (auto& area : m_dirtyTerrainAreas) {
        m_terrain->processSegments(area, [&](Mercator::Segment& s, int, int) { dirtySegments.insert(&s); });
    }
    m_dirtyTerrainAreas.clear();

    boost::optional<float> friction;
    auto frictionProp = m_entity.getPropertyType<double>("friction");
    if (frictionProp) {
        friction = (float) frictionProp->data();
    }
    boost::optional<float> frictionRolling;
    auto frictionRollingProp = m_entity.getPropertyType<double>("friction_roll");
    if (frictionRollingProp) {
        frictionRolling = (float) frictionRollingProp->data();
    }
    boost::optional<float> frictionSpinning;
    auto frictionSpinningProp = m_entity.getPropertyType<double>("friction_spin");
    if (frictionSpinningProp) {
        frictionSpinning = (float) frictionSpinningProp->data();
    }

    auto worldHeight = m_entity.m_location.bBox().highCorner().y() - m_entity.m_location.bBox().lowCorner().y();

    debug_print("dirty segments: " << dirtySegments.size())
    for (auto& segment : dirtySegments) {
        debug_print("rebuilding segment at x: " << segment->getXRef() << " z: " << segment->getZRef());

        auto& terrainEntry = buildTerrainPage(*segment);
        if (friction) {
            terrainEntry.rigidBody->setFriction(*friction);
        }
        if (frictionRolling) {
            terrainEntry.rigidBody->setRollingFriction(*frictionRolling);
        }
        if (frictionSpinning) {
#if BT_BULLET_VERSION < 285
            log(WARNING, "Your version of Bullet doesn't support spinning friction.");
#else
            terrainEntry.rigidBody->setSpinningFriction(*frictionSpinning);
#endif
        }

        struct : public btCollisionWorld::ContactResultCallback
        {
            std::vector<PhysicalDomain::BulletEntry*> m_entries;

            btScalar addSingleResult(btManifoldPoint& cp, const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0, const btCollisionObjectWrapper* colObj1Wrap,
                                     int partId1, int index1) override
            {
                auto* bulletEntry = static_cast<BulletEntry*>(colObj1Wrap->m_collisionObject->getUserPointer());
                if (bulletEntry && (m_entries.empty() || *m_entries.rbegin() != bulletEntry)) {
                    m_entries.emplace_back(bulletEntry);
                }
                return btScalar(1.0);
            }
        } callback;

        callback.m_collisionFilterGroup = COLLISION_MASK_TERRAIN;
        callback.m_collisionFilterMask = COLLISION_MASK_PHYSICAL | COLLISION_MASK_NON_PHYSICAL | COLLISION_MASK_STATIC;

        auto area = segment->getRect();
        WFMath::Vector<2> size = area.highCorner() - area.lowCorner();

        btBoxShape boxShape(btVector3(size.x() * 0.5f, worldHeight, size.y() * 0.5f));
        btCollisionObject collObject;
        collObject.setCollisionShape(&boxShape);
        auto center = area.getCenter();
        collObject.setWorldTransform(btTransform(btQuaternion::getIdentity(), btVector3(center.x(), 0, center.y())));
        m_dynamicsWorld->contactTest(&collObject, callback);

        debug_print("Matched " << callback.m_entries.size() << " entries")
        for (BulletEntry* entry : callback.m_entries) {
            debug_print("Adjusting " << entry->entity.describeEntity())
            Anonymous anon;
            anon->setId(entry->entity.getId());
            std::vector<double> posList;
            addToEntity(entry->entity.m_location.m_pos, posList);
            anon->setPos(posList);
            Move move;
            move->setTo(entry->entity.getId());
            move->setFrom(entry->entity.getId());
            move->setArgs1(anon);
            entry->entity.sendWorld(move);
        }
    }
}

void PhysicalDomain::sendMoveSight(BulletEntry& entry, bool posChange, bool velocityChange, bool orientationChange, bool angularChange, bool modeChange)
{
    if (!entry.observingThis.empty()) {
        LocatedEntity& entity = entry.entity;
        Location& lastSentLocation = entry.lastSentLocation;
        bool shouldSendOp = false;
        Anonymous move_arg;
        if (velocityChange) {
            ::addToEntity(entity.m_location.velocity(), move_arg->modifyVelocity());
            shouldSendOp = true;
            lastSentLocation.m_velocity = entity.m_location.velocity();
        }
        if (angularChange) {
            move_arg->setAttr("angular", entity.m_location.m_angularVelocity.toAtlas());
            shouldSendOp = true;
            lastSentLocation.m_angularVelocity = entity.m_location.m_angularVelocity;
        }
        if (orientationChange) {
            move_arg->setAttr("orientation", entity.m_location.orientation().toAtlas());
            shouldSendOp = true;
            lastSentLocation.m_orientation = entity.m_location.m_orientation;
        }
        //If the velocity changes we should also send the position, to make it easier for clients to project position.
        if (posChange || velocityChange) {
            ::addToEntity(entity.m_location.pos(), move_arg->modifyPos());
            shouldSendOp = true;
            lastSentLocation.m_pos = entity.m_location.m_pos;
        }
        if (modeChange) {
            auto prop = entity.getPropertyClassFixed<ModeProperty>();
            if (prop) {
                Atlas::Message::Element element;
                if (prop->get(element) == 0) {
                    move_arg->setAttr("mode", element);
                    shouldSendOp = true;
                }
            }
        }

        if (shouldSendOp) {
            Set setOp;
            move_arg->setId(entity.getId());
            if (debug_flag) {
                debug_print("Sending set op for movement.")
                if (entity.m_location.velocity().isValid()) {
                    debug_print("new velocity: " << entity.m_location.velocity() << " " << entity.m_location.velocity().mag())
                }
            }

            //entity.m_location.addToEntity(move_arg);

            setOp->setArgs1(move_arg);
            setOp->setFrom(entity.getId());
            setOp->setTo(entity.getId());
            double seconds = BaseWorld::instance().getTimeAsSeconds();
            setOp->setSeconds(seconds);

            for (BulletEntry* observer : entry.observingThis) {
                Sight s;
                s->setArgs1(setOp);
                s->setTo(observer->entity.getId());
                s->setFrom(entity.getId());
                s->setSeconds(seconds);

                entity.sendWorld(s);
            }
        }
    }
}

void PhysicalDomain::processMovedEntity(BulletEntry& bulletEntry, double timeSinceLastUpdate)
{
    LocatedEntity& entity = bulletEntry.entity;
    Location& lastSentLocation = bulletEntry.lastSentLocation;
    const Location& location = entity.m_location;
    //This should normally not happen, but let's check nonetheless.
    if (!location.m_pos.isValid()) {
        return;
    }

    bool orientationChange = location.m_orientation.isValid() && (!lastSentLocation.m_orientation.isValid() || !location.m_orientation.isEqualTo(lastSentLocation.m_orientation, 0.1f));
    bool posChange = false;
    if (!lastSentLocation.m_pos.isValid()) {
        posChange = true;
    } else {
        //If position differs from last sent position, and there's no velocity, then send position update
        if (lastSentLocation.m_pos != location.m_pos && (!location.m_velocity.isValid() || location.m_velocity == WFMath::Vector<3>::ZERO())) {
            posChange = true;
        } else {
            if (lastSentLocation.m_velocity.isValid()) {
                auto projectedPosition = lastSentLocation.m_pos + (lastSentLocation.m_velocity * timeSinceLastUpdate);
                if (WFMath::Distance(location.m_pos, projectedPosition) > 0.5) {
                    posChange = true;
                }
            }
        }
    }

    if (false) {
        sendMoveSight(bulletEntry, true, true, true, true, true);
    } else {

        bool velocityChange = false;

        if (entity.m_location.m_velocity.isValid()) {
            bool hadValidVelocity = lastSentLocation.m_velocity.isValid();
            //Send an update if either the previous velocity was invalid, or any of the velocity components have changed enough, or if either the new or the old velocity is zero.
            if (!hadValidVelocity) {
                debug_print("No previous valid velocity " << entity.describeEntity() << " " << lastSentLocation.m_velocity)
                velocityChange = true;
                lastSentLocation.m_velocity = entity.m_location.m_velocity;
            } else {
                bool xChange = !fuzzyEquals(location.m_velocity.x(), lastSentLocation.m_velocity.x(), 0.01);
                bool yChange = !fuzzyEquals(location.m_velocity.y(), lastSentLocation.m_velocity.y(), 0.01);
                bool zChange = !fuzzyEquals(location.m_velocity.z(), lastSentLocation.m_velocity.z(), 0.01);
                bool hadZeroVelocity = lastSentLocation.m_velocity.isEqualTo(WFMath::Vector<3>::ZERO());
                if (xChange || yChange || zChange) {
                    debug_print("Velocity changed " << entity.describeEntity() << " " << location.m_velocity)
                    velocityChange = true;
                    lastSentLocation.m_velocity = entity.m_location.velocity();
                } else if (entity.m_location.m_velocity.isEqualTo(WFMath::Vector<3>::ZERO()) && !hadZeroVelocity) {
                    debug_print("Old or new velocity zero " << entity.describeEntity() << " " << location.m_velocity)
                    velocityChange = true;
                    lastSentLocation.m_velocity = entity.m_location.velocity();
                }
            }
        }
        bool angularChange = false;

        if (entity.m_location.m_angularVelocity.isValid()) {
            bool hadZeroAngular = lastSentLocation.m_angularVelocity.isEqualTo(WFMath::Vector<3>::ZERO());
            angularChange = !fuzzyEquals(lastSentLocation.m_angularVelocity, location.m_angularVelocity, 0.01);
            if (!angularChange && entity.m_location.m_angularVelocity.isEqualTo(WFMath::Vector<3>::ZERO()) && !hadZeroAngular) {
                debug_print("Angular changed " << entity.describeEntity() << " " << location.m_angularVelocity)
                angularChange = true;
                lastSentLocation.m_angularVelocity = entity.m_location.m_angularVelocity;
            }
        }

        if (posChange || velocityChange || orientationChange || angularChange || bulletEntry.modeChanged) {
            //Increase sequence number as properties have changed.
            entity.increaseSequenceNumber();
            sendMoveSight(bulletEntry, posChange, velocityChange, orientationChange, angularChange, bulletEntry.modeChanged);
            lastSentLocation.m_pos = entity.m_location.m_pos;
            bulletEntry.modeChanged = false;
        }
        if (posChange && !bulletEntry.closenessObservations.empty()) {
            //Since callbacks can remove observations we need to first collect att invalid observations, and then remove them carefully.
            std::vector<ClosenessObserverEntry*> invalidEntries;
            for (auto& observation : bulletEntry.closenessObservations) {
                if (!isWithinReach(*observation->reacher, *observation->target, observation->reach, {})) {
                    invalidEntries.emplace_back(observation);
                }
            }


            for (auto& observation : invalidEntries) {
                //It's important that we check that the observation still is valid, since it's possible that callbacks alters the collections.:
                auto I = bulletEntry.closenessObservations.find(observation);
                if (I != bulletEntry.closenessObservations.end()) {
                    if (&bulletEntry == observation->reacher) {
                        observation->target->closenessObservations.erase(observation);
                    } else {
                        observation->reacher->closenessObservations.erase(observation);
                    }
                    bulletEntry.closenessObservations.erase(I);

                    auto J = m_closenessObservations.find(observation);
                    //Hold on to an instance while we call callbacks and erase it.
                    auto observerInstance = std::move(J->second);
                    m_closenessObservations.erase(J);

                    if (observation->callback) {
                        observation->callback();
                    }
                }
            }
        }
    }

    updateTerrainMod(entity);
}

void PhysicalDomain::tick(double tickSize, OpVector& res)
{
//    CProfileManager::Reset();
//    CProfileManager::Increment_Frame_Counter();
    rmt_ScopedCPUSample(PhysicalDomain_tick, 0);

    auto start = std::chrono::steady_clock::now();
    /**
     * An entry of a projectile hitting another entry.
     */
    struct BulletCollisionEntry
    {
        /**
         * The entry that was hit.
         */
        BulletEntry* bulletEntry;
        /**
         * The position in the world where the hit occurred.
         */
        btVector3 pos;
    };

    //  static std::map<BulletEntry*, BulletCollisionEntry> projectileCollisions;
    static std::vector<std::pair<BulletEntry*, BulletCollisionEntry>> projectileCollisions;

    gContactProcessedCallback = [](btManifoldPoint& cp, void* body0, void* body1) -> bool {
        auto object0 = static_cast<btCollisionObject*>(body0);
        auto bulletEntry0 = static_cast<BulletEntry*>(object0->getUserPointer());
        auto object1 = static_cast<btCollisionObject*>(body1);
        auto bulletEntry1 = static_cast<BulletEntry*>(object1->getUserPointer());

        if (bulletEntry0->mode == ModeProperty::Mode::Projectile) {
            projectileCollisions.emplace_back(bulletEntry0, BulletCollisionEntry{bulletEntry1, cp.getPositionWorldOnB()});
        }
        if (bulletEntry1->mode == ModeProperty::Mode::Projectile) {
            projectileCollisions.emplace_back(bulletEntry1, BulletCollisionEntry{bulletEntry0, cp.getPositionWorldOnA()});
        }
        return true;
    };

    auto simulationSpeedProp = m_entity.getPropertyClassFixed<SimulationSpeedProperty>();
    if (simulationSpeedProp) {
        tickSize *= simulationSpeedProp->data();
    }

    projectileCollisions.clear();

    postDuration = {};
    //Step simulations with 60 hz.
    m_dynamicsWorld->stepSimulation((float) tickSize, static_cast<int>(60 * tickSize));
    auto interim = std::chrono::steady_clock::now() - start;

    //CProfileManager::dumpAll();

    //The list of projectilecollisions will contain duplicates, so we need to keep track of the last
    //processed and check that it does not repeat.
    BulletEntry* lastCollisionEntry = nullptr;
    for (const auto& entry : projectileCollisions) {
        auto projectileEntry = entry.first;
        if (lastCollisionEntry == projectileEntry) {
            continue;
        }
        lastCollisionEntry = projectileEntry;
        auto& collisionEntry = entry.second;
        const auto modeDataProperty = projectileEntry->entity.getPropertyClassFixed<ModeDataProperty>();
        Atlas::Objects::Entity::Anonymous ent;
        //If the projectile data contained information on which entity caused the projectile to fly away, copy that.
        //This is needed for game rules to determine which entity hit another.
        if (modeDataProperty && modeDataProperty->getProjectileData().entity) {
            ent->setId(modeDataProperty->getProjectileData().entity->getId());
        } else {
            ent->setId(collisionEntry.bulletEntry->entity.getId());
        }
        std::vector<double> posList;
        addToEntity(Convert::toWF<WFMath::Point<3>>(collisionEntry.pos), posList);
        ent->setPos(std::move(posList));
        ent->setLoc(m_entity.getId());
        if (modeDataProperty && modeDataProperty->getMode() == ModeProperty::Mode::Projectile) {
            auto& projectileData = modeDataProperty->getProjectileData();
            //Copy any data found in "mode_data".
            for (const auto& projectile_entry : projectileData.extra) {
                ent->setAttr(projectile_entry.first, projectile_entry.second, &Inheritance::instance().getFactories());
            }
        }
        Atlas::Objects::Operation::Hit hit;
        hit->setArgs1(std::move(ent));
        hit->setTo(projectileEntry->entity.getId());
        hit->setFrom(collisionEntry.bulletEntry->entity.getId());

        auto hitCopy = hit.copy();
        hitCopy->setTo(collisionEntry.bulletEntry->entity.getId());
        hitCopy->setFrom(projectileEntry->entity.getId());

        //We need to make sure that Hit ops gets their correct "from".
        collisionEntry.bulletEntry->entity.sendWorld(std::move(hit));
        projectileEntry->entity.sendWorld(std::move(hitCopy));
    }

    auto visStart = std::chrono::steady_clock::now();
    updateVisibilityOfDirtyEntities(res);
    auto visDuration = std::chrono::steady_clock::now() - visStart;

    processWaterBodies();

    //We process the vector of moving entities as efficient as possible by don't doing
    //any reallocations during the processing. If any entry should be removed (because it's
    //not moving any more) we relocate the last entry to the position we're at.
    //Once we're done with processing we'll shrink the vector if any element was removed.
    size_t movingSize = m_movingEntities.size();
    for (size_t i = 0; i < movingSize;) {
        auto movedEntry = m_movingEntities[i];
        if (!movedEntry->markedAsMovingThisFrame) {
            //Stopped moving
            if (movedEntry->entity.m_location.m_angularVelocity.isValid()) {
                movedEntry->entity.m_location.m_angularVelocity.zero();
            }
            if (movedEntry->entity.m_location.m_velocity.isValid()) {
                debug_print("Stopped moving " << movedEntry->entity.describeEntity())
                movedEntry->entity.m_location.m_velocity.zero();
            }
            processMovedEntity(*movedEntry, tickSize);
            movedEntry->markedAsMovingLastFrame = false;
            movedEntry->addedToMovingList = false;

            //If we're removing the last entry just skip
            if (i == movingSize - 1) {
                --movingSize;
            } else {
                //Move the last element to this position and don't advance i
                m_movingEntities[i] = m_movingEntities[movingSize - 1];
                --movingSize;
            }
        } else if (!movedEntry->markedAsMovingLastFrame) {
            //Started moving
            movedEntry->markedAsMovingLastFrame = true;
            movedEntry->markedAsMovingThisFrame = false;
            processMovedEntity(*movedEntry, tickSize);
            ++i;
        } else {
            //Moved previously and has continued to move
            movedEntry->markedAsMovingLastFrame = true;
            movedEntry->markedAsMovingThisFrame = false;
            processMovedEntity(*movedEntry, tickSize);
            ++i;
        }
    }
    //If we've removed any entries we need to shrink the vector
    if (movingSize != m_movingEntities.size()) {
        m_movingEntities.resize(movingSize);
    }

    processDirtyTerrainAreas();

    auto duration = std::chrono::steady_clock::now() - start;
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    if (debug_flag) {
        log(microseconds > 3000 ? WARNING : INFO,
            String::compose("Physics took %1 μs (just stepSimulation %2 μs, visibility %3 μs, tick size %4 μs, visibility queue: %5, postTick: %6 μs, moving count: %7).",
                            microseconds,
                            std::chrono::duration_cast<std::chrono::microseconds>(interim).count(),
                            std::chrono::duration_cast<std::chrono::microseconds>(visDuration).count(),
                            static_cast<long>(tickSize * 1000000),
                            m_visibilityRecalculateQueue.size(),
                            std::chrono::duration_cast<std::chrono::microseconds>(postDuration).count(),
                            movingSize)
        );
    }
    s_processTimeUs += microseconds;
}

void PhysicalDomain::processWaterBodies()
{
    auto testEntityIsSubmergedFn = [&](BulletEntry* bulletEntry, BulletEntry* waterEntry) -> bool {
        bool isInside;
        //If the water body entity has been deleted it will have been set to null.
        if (waterEntry == nullptr) {
            isInside = false;
        } else if (waterEntry == bulletEntry) {
            isInside = false;
            bulletEntry->waterNearby = nullptr;
        } else {
            auto waterBody = waterEntry->collisionObject.get();
            auto shapeType = waterBody->getCollisionShape()->getShapeType();
            auto overlappingObject = bulletEntry->collisionObject.get();
            if (shapeType == BOX_SHAPE_PROXYTYPE) {
                auto boxShape = dynamic_cast<btBoxShape*>(waterBody->getCollisionShape());
                //Translate position of overlapping shape into the water body's space
                auto testPos = overlappingObject->getWorldTransform().getOrigin() - waterBody->getWorldTransform().getOrigin();
                testPos = testPos * waterBody->getWorldTransform().getBasis().inverse();
                isInside = boxShape->isInside(testPos, 0);
            } else if (shapeType == STATIC_PLANE_PROXYTYPE) {
                isInside = overlappingObject->getWorldTransform().getOrigin().y() <= waterBody->getWorldTransform().getOrigin().y();
            } else {
                //We only support planes and boxes
                return false;
            }
        }
        if (isInside) {
            if (bulletEntry->mode != ModeProperty::Mode::Submerged) {
                auto rigidBody = btRigidBody::upcast(bulletEntry->collisionObject.get());
                if (rigidBody) {
                    rigidBody->setGravity(btVector3(0, 0, 0));
                    rigidBody->setDamping(0.8, 0);
                }
                bulletEntry->mode = ModeProperty::Mode::Submerged;
                auto prop = bulletEntry->entity.requirePropertyClassFixed<ModeProperty>("submerged");
                prop->set("submerged");
                bulletEntry->modeChanged = true;
                if (!bulletEntry->addedToMovingList) {
                    m_movingEntities.emplace_back(bulletEntry);
                    bulletEntry->addedToMovingList = true;
                    bulletEntry->markedAsMovingThisFrame = false;
                    bulletEntry->markedAsMovingLastFrame = false;
                }
            }
            return true;
        } else {
            if (bulletEntry->mode == ModeProperty::Mode::Submerged) {
                auto rigidBody = btRigidBody::upcast(bulletEntry->collisionObject.get());
                if (rigidBody) {
                    rigidBody->setGravity(m_dynamicsWorld->getGravity());
                    rigidBody->setDamping(0, 0);
                }
                bulletEntry->mode = ModeProperty::Mode::Free;
                auto prop = bulletEntry->entity.requirePropertyClassFixed<ModeProperty>("free");
                prop->set("free");
                bulletEntry->modeChanged = true;
                if (!bulletEntry->addedToMovingList) {
                    m_movingEntities.emplace_back(bulletEntry);
                    bulletEntry->addedToMovingList = true;
                    bulletEntry->markedAsMovingThisFrame = false;
                    bulletEntry->markedAsMovingLastFrame = false;
                }
            }
            return false;
        }
    };

    for (auto entry : m_movingEntities) {
        if (entry->waterNearby) {
            testEntityIsSubmergedFn(entry, entry->waterNearby);
        }
    }

}

bool PhysicalDomain::getTerrainHeight(float x, float y, float& height) const
{
    if (m_terrain) {
        Mercator::Segment* s = m_terrain->getSegmentAtPos(x, y);
        if (s != nullptr && !s->isValid()) {
            s->populate();
        }
        return m_terrain->getHeight(x, y, height);
    } else {
        height = m_entity.m_location.bBox().lowCorner().y();
        return false;
    }
}

void PhysicalDomain::transformRestingEntities(PhysicalDomain::BulletEntry* entry,
                                              const WFMath::Vector<3>& posTransform,
                                              const WFMath::Quaternion& orientationChange,
                                              std::set<LocatedEntity*>& transformedEntities)
{
    auto* collObject = entry->collisionObject.get();
    if (collObject) {

        //Check if there are any objects resting on us, and move them along too.
        std::set<BulletEntry*> objectsRestingOnOurObject = entry->attachedEntities;
        int numManifolds = m_dynamicsWorld->getDispatcher()->getNumManifolds();
        for (int i = 0; i < numManifolds; i++) {
            btPersistentManifold* contactManifold = m_dynamicsWorld->getDispatcher()->getManifoldByIndexInternal(i);
            const btCollisionObject* obA = contactManifold->getBody0();
            const btCollisionObject* obB = contactManifold->getBody1();

            const btCollisionObject* otherObject;
            bool aIsOurObject = false;

            if (obA == collObject) {
                otherObject = obB;
                aIsOurObject = true;
            } else if (obB == collObject) {
                otherObject = obA;
            } else {
                continue;
            }

            if (otherObject->getInternalType() != btCollisionObject::CO_GHOST_OBJECT && !otherObject->isStaticObject()) {

                auto* restingEntry = static_cast<BulletEntry*>(otherObject->getUserPointer());

                if (restingEntry) {
                    int numContacts = contactManifold->getNumContacts();
                    for (int j = 0; j < numContacts; j++) {
                        btManifoldPoint& pt = contactManifold->getContactPoint(j);
                        if (pt.getDistance() < 0.f) {
                            const btVector3& ptA = pt.getPositionWorldOnA();
                            const btVector3& ptB = pt.getPositionWorldOnB();
                            //Check if the other object rests on our object.
                            //Note that due to how collision is handled, the vertical check is "inversed".
                            //I.e. if object A rests on object B, the collision point on A will actually
                            // be _below_ the point on B, since they overlap.
                            if (aIsOurObject) {
                                if (ptA.y() > ptB.y()) {
                                    objectsRestingOnOurObject.emplace(restingEntry);
                                    break;
                                }
                            } else {
                                if (ptB.y() > ptA.y()) {
                                    objectsRestingOnOurObject.emplace(restingEntry);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        auto childTransform = posTransform;
        if (orientationChange.isValid()) {
            childTransform.rotate(orientationChange);
        }
        //Move all of the objects that were resting on our object.
        for (auto& restingEntry : objectsRestingOnOurObject) {
            //Check that we haven't already handled this entry, to avoid infinite loop with complex shapes resting on each other.
            if (transformedEntities.find(&restingEntry->entity) != transformedEntities.end()) {
                continue;
            }
            auto relativePos = restingEntry->entity.m_location.pos() - (entry->entity.m_location.pos() - posTransform);

            if (orientationChange.isValid()) {
                relativePos.rotate(orientationChange);
            }

            applyTransformInternal(restingEntry->entity, restingEntry->entity.m_location.m_orientation * orientationChange,
                                   entry->entity.m_location.m_pos + relativePos, WFMath::Vector<3>(), transformedEntities, false);

        }
    }
}

void PhysicalDomain::plantOnEntity(PhysicalDomain::BulletEntry* plantedEntry, PhysicalDomain::BulletEntry* entryPlantedOn)
{
    auto existingModeDataProp = plantedEntry->entity.getPropertyClassFixed<ModeDataProperty>();

    if (existingModeDataProp && existingModeDataProp->getMode() == ModeProperty::Mode::Planted) {
        auto& plantedOnData = existingModeDataProp->getPlantedOnData();
        //Check if we're already planted, and perhaps should be detached.
        if (plantedOnData.entityId) {
            if (entryPlantedOn && *plantedOnData.entityId == entryPlantedOn->entity.getIntId()) {
                //Already planted on entity, nothing to do
                return;
            }
            if (!entryPlantedOn && !plantedOnData) {
                //Not planted already, and not being planted, nothing to do.
                return;
            }

            auto I = m_entries.find(*plantedOnData.entityId);
            if (I != m_entries.end()) {
                I->second->attachedEntities.erase(plantedEntry);
            }
        }

    } else {
        if (!entryPlantedOn) {
            //No prop exists, and we shouldn't be planted, just return.
            return;
        }
    }

    //We need to change the property for this
    auto newModeDataProp = plantedEntry->entity.requirePropertyClassFixed<ModeDataProperty>();

    if (entryPlantedOn) {
        newModeDataProp->setPlantedData({entryPlantedOn->entity.getIntId()});
        entryPlantedOn->attachedEntities.insert(plantedEntry);
    } else {
        newModeDataProp->clearData();
    }

    newModeDataProp->addFlags(prop_flag_unsent);
}

bool PhysicalDomain::isEntityReachable(const LocatedEntity& reachingEntity, float reach, const LocatedEntity& queriedEntity, const WFMath::Point<3>& positionOnQueriedEntity) const
{
    if (&reachingEntity == &m_entity) {
        //If the entity itself is reaching for a contained entities it's allowed.
        return true;
    }

    //For physical domains we require "reach" to be set in order for anything to reach.
    if (reach == 0) {
        return false;
    }

    auto& queriedLocation = queriedEntity.m_location;
    auto& reachingLocation = reachingEntity.m_location;
    //Check that locations are valid, unless reaching for something contained in the domain itself (since the domain might not have a valid position).
    if (!reachingLocation.m_pos.isValid() || (&queriedEntity != &m_entity && !queriedLocation.m_pos.isValid())) {
        return false;
    }

    auto reachingEntryI = m_entries.find(reachingEntity.getIntId());
    if (reachingEntryI != m_entries.end()) {
        auto& reachingEntityEntry = reachingEntryI->second;

        //If a contained entity tries to touch the domain entity we must check the optional position.
        if (&queriedEntity == &m_entity) {
            if (!positionOnQueriedEntity.isValid()) {
                return false;
            }
            //Try to get the collision objects positions, if there are any. Otherwise use the entities positions. This is because some entities don't have collision objects.
            btVector3 reachingEntityPos = reachingEntityEntry->collisionObject ? reachingEntityEntry->collisionObject->getWorldTransform().getOrigin()
                                                                               : Convert::toBullet(reachingLocation.pos());

            auto distance = reachingEntityPos.distance(Convert::toBullet(positionOnQueriedEntity));
            distance -= reachingLocation.radius();
            return distance <= reach;

        } else {
            //Both the reaching entity and the queried entity must be contained in the domain.
            auto queriedBulletEntryI = m_entries.find(queriedEntity.getIntId());
            if (queriedBulletEntryI != m_entries.end()) {
                auto& queriedBulletEntry = queriedBulletEntryI->second;
                return isWithinReach(*reachingEntityEntry, *queriedBulletEntry, reach, positionOnQueriedEntity);
            }
        }
    }
    //If either the reaching of queried entity doesn't belong to the domain we won't allow it.
    //The most likely case if the reaching entity not belonging (i.e. reaching into a domain).
    return false;
}

bool PhysicalDomain::isWithinReach(BulletEntry& reacherEntry, BulletEntry& targetEntry, float reach, const WFMath::Point<3>& positionOnQueriedEntity) const
{
    auto& targetLocation = targetEntry.entity.m_location;
    auto& reachingLocation = reacherEntry.entity.m_location;
    btVector3 queriedEntityPos;
    //Try to get the collision objects positions, if there are any. Otherwise use the entities positions. This is because some entities don't have collision objects.
    btVector3 reachingEntityPos = reacherEntry.collisionObject ? reacherEntry.collisionObject->getWorldTransform().getOrigin()
                                                               : Convert::toBullet(reacherEntry.entity.m_location.pos());

    if (positionOnQueriedEntity.isValid()) {
        //Adjust position on queried entity into parent coord system (to get a global pos)
        queriedEntityPos = Convert::toBullet(
                positionOnQueriedEntity.toParentCoords(targetLocation.m_pos,
                                                       targetLocation.m_orientation.isValid() ? targetLocation.m_orientation : WFMath::Quaternion::IDENTITY()));
    } else {
        //Try to get the collision objects positions, if there are any. Otherwise use the entities positions. This is because some entities don't have collision objects.
        queriedEntityPos = targetEntry.collisionObject ? targetEntry.collisionObject->getWorldTransform().getOrigin()
                                                       : Convert::toBullet(targetLocation.pos());
    }

    //Start with the simple case by checking if the centers are close
    //We measure from the edge of one entity to the edge of another.
    if (reachingEntityPos.distance(queriedEntityPos) - reachingLocation.radius() - targetLocation.radius() <= reach) {
        return true;
    }

    if (targetEntry.collisionObject) {
        //Check how we collide with the target.
        btCollisionWorld::ClosestRayResultCallback rayResultCallback(reachingEntityPos, queriedEntityPos);
        auto extendedQueriedPos = queriedEntityPos + ((queriedEntityPos - reachingEntityPos) * 0.1); //Extend a bit
        btCollisionWorld::rayTestSingle(btTransform(btQuaternion::getIdentity(), reachingEntityPos),
                                        btTransform(btQuaternion::getIdentity(), extendedQueriedPos),
                                        targetEntry.collisionObject.get(),
                                        targetEntry.collisionShape.get(),
                                        targetEntry.collisionObject->getWorldTransform(),
                                        rayResultCallback);

        if (!rayResultCallback.hasHit()) {
            return false;
        }

        //Start with the full distance.
        auto distance = reachingEntityPos.distance(rayResultCallback.m_hitPointWorld);

        //We measure from the edge of one entity to the edge of another.
        distance -= reachingLocation.radius();

        return distance <= reach;
    }
    return false;
}

std::vector<Domain::CollisionEntry> PhysicalDomain::queryCollision(const WFMath::Ball<3>& sphere) const
{
    struct : public btCollisionWorld::ContactResultCallback
    {
        std::map<BulletEntry*, btManifoldPoint> m_entries;

        btScalar addSingleResult(btManifoldPoint& cp, const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0, const btCollisionObjectWrapper* colObj1Wrap,
                                 int partId1, int index1) override
        {
            auto* bulletEntry = static_cast<BulletEntry*>(colObj1Wrap->m_collisionObject->getUserPointer());
            if (bulletEntry) {
                m_entries.emplace(bulletEntry, cp);
            }
            return btScalar(1.0);
        }

    } callback;

    btTransform pos(btQuaternion::getIdentity(), Convert::toBullet(sphere.center()));

    btSphereShape shape(sphere.radius());

    btCollisionObject collisionObject;
    collisionObject.setCollisionShape(&shape);
    collisionObject.setWorldTransform(pos);

    m_dynamicsWorld->contactTest(&collisionObject, callback);

    std::vector<Domain::CollisionEntry> result;
    result.reserve(callback.m_entries.size());
    for (auto& entry: callback.m_entries) {
        result.emplace_back(Domain::CollisionEntry{&entry.first->entity,
                                                   Convert::toWF<WFMath::Point<3>>(entry.second.getPositionWorldOnA()),
//                                                   std::abs(entry.second.getDistance())});
                                                   entry.second.getPositionWorldOnB().distance(pos.getOrigin())});
    }
    return result;
}

boost::optional<std::function<void()>> PhysicalDomain::observeCloseness(LocatedEntity& reacher, LocatedEntity& target, double reach, std::function<void()> callback)
{
    auto reacherEntryI = m_entries.find(reacher.getIntId());
    auto targetEntryI = m_entries.find(target.getIntId());
    if (reacherEntryI != m_entries.end() && targetEntryI != m_entries.end()) {
        auto* reacherEntry = reacherEntryI->second.get();
        auto* targetEntry = targetEntryI->second.get();
        auto obs = new ClosenessObserverEntry{reacherEntry, targetEntry, reach, callback};
        reacherEntry->closenessObservations.insert(obs);
        targetEntry->closenessObservations.insert(obs);
        m_closenessObservations.emplace(obs, std::unique_ptr<ClosenessObserverEntry>(obs));
        return boost::optional<std::function<void()>>([this, reacherEntry, targetEntry, obs]() {
            reacherEntry->closenessObservations.erase(obs);
            targetEntry->closenessObservations.erase(obs);
            m_closenessObservations.erase(obs);
        });
    }
    return boost::none;
}

void PhysicalDomain::removed()
{
    //Copy to allow modifications to the field during callbacks.
    auto observations = std::move(m_closenessObservations);
    for (auto& entry : observations) {
        entry.second->callback();
    }
}

float PhysicalDomain::calculateVisibilitySphereRadius(const LocatedEntity& entity) const
{
    auto visProp = entity.getPropertyClassFixed<VisibilityDistanceProperty>();

    double radius;
    //If there's a vis_dist prop set, use that directly to set the radius.
    //If not, check the radius of the entity as apply the VISIBILITY_RATIO, unless there's no physical object.
    //In that case apply a default settings based on the entity having a radius of 0.25 meters
    if (visProp) {
        radius = visProp->data();
    } else if (entity.m_location.bBox().isValid() && entity.m_location.radius() > 0) {
        radius = (entity.m_location.radius() * VISIBILITY_RATIO);
    } else {
        //Handle it as if it was 0.25 meter in radius
        radius = (0.25f * VISIBILITY_RATIO);
    }
    for (size_t i = 0; i < VISIBILITY_DISTANCE_THRESHOLDS.size(); ++i) {
        if (radius < VISIBILITY_DISTANCE_THRESHOLDS[i]) {
            if (i == 0) {
                return VISIBILITY_DISTANCE_THRESHOLDS[0] * VISIBILITY_SCALING_FACTOR;
            } else {
                return VISIBILITY_DISTANCE_THRESHOLDS[i - 1] * VISIBILITY_SCALING_FACTOR;
            }
        }
    }
    //If we get here the visibility is beyond the largest threshold, so return that.
    return VISIBILITY_DISTANCE_THRESHOLDS.back() * VISIBILITY_SCALING_FACTOR;

}
