/** @file CEnemyWorld.h
 * @brief Host callbacks connecting the original spawner to its level.
 * Original consumers: enemySpawner.cpp IEnemySpawnerScriptInterface :146664,
 * level.cpp CLevel::SpawnEnemy. This callback contract is a Windows adapter,
 * not a recovered original class; C prefix is the requested project convention.
 */
#pragma once
#include "gun_bros_re/data/CGameAssetRef.h"
#include "gun_bros_re/gameplay/map/CLayerObject.h"
/** The level world decides which spawn nodes are free and owns the actors. */
class CEnemyWorld {
public:
    virtual ~CEnemyWorld() = default;
    virtual bool SpawnEnemy(const GameObjectRef &enemy, int layer, int node, int objectId) = 0;
    virtual int CountEnemies(const GameObjectRef *enemy = nullptr, int objectId = -1) const = 0;
    // Pool occupancy includes dead actors until Release. Worlds which retire
    // actors immediately can use their ordinary count.
    virtual int CountEnemySlots(const GameObjectRef *enemy = nullptr) const { return CountEnemies(enemy); }
    virtual void StartObjectLayer(int layer) {}
    virtual bool SpawnMapObject(const CLayerObject::Object &object, int objectId) { return false; }
    virtual void SendEnemyMessage(int objectId, int message) {}
    virtual void SendPropMessage(int objectId, int message) {}
    virtual void SetEnemyPortal(int enemyId, int propId) {}
    virtual bool IsActivePortal(int propId) const { return false; }
    virtual void PlayLevelSound(const GameObjectRef &sound) {}
    virtual void OnWaveCleared(unsigned perfectRewardPercent) {}
    virtual bool SpawnPickup(const GameObjectRef &pickup, int layer, int node, int objectId, bool nearby) { return false; }
    virtual bool SpawnPickupAt(const GameObjectRef &pickup, float x, float y, int objectId) { return false; }
    virtual bool SpawnMPMatchPickup(const GameObjectRef &pickup, int layer) { return false; }
    virtual bool GetObjectPosition(int objectId, float &x, float &y) const { return false; }
    virtual std::uint64_t ResolveIndicatorTarget(int objectId) const { return 0; }
    virtual bool GetIndicatorTarget(std::uint64_t key, float &x, float &y) const { return false; }
    virtual unsigned GetPowerupCount(unsigned localIndex) const { return 0; }
};
