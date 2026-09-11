#include "gun_bros_re/data/MissionCatalogInternal.h"

namespace MissionCatalogDetail {
unsigned CheckMissionMap(const CLevel::Template &data, CMap &map, std::ofstream &report) {
    CLevel level;
    MissionProbeWorld world;
    level.Bind(data, map, &world);
    unsigned failures = 0;
    std::int16_t args[3] = {1, 0, 0};
    for (int tag = 0; tag < 256; ++tag) {
        args[1] = static_cast<std::int16_t>(tag);
        level.FunctionResolver(15, args, 2);
    }
    const std::size_t initialCount = world.spawned.size();
    level.UpdateProximitySpawns(-100000, -100000, 200000, 200000);
    if (world.spawned.size() != initialCount) { ++failures; }
    args[0] = 0;
    for (int tag = 0; tag < 256; ++tag) {
        args[1] = static_cast<std::int16_t>(tag);
        level.FunctionResolver(15, args, 2);
    }
    level.UpdateProximitySpawns(-100000, -100000, 1, 1);
    const std::size_t remoteProps = world.spawned.size();
    level.UpdateProximitySpawns(-100000, -100000, 200000, 200000);
    const std::size_t allObjects = world.spawned.size();
    level.UpdateProximitySpawns(-100000, -100000, 200000, 200000);
    if (world.spawned.size() != allObjects || world.duplicates != 0) { ++failures; }
    // Native timing units and trigger switches are tested with an unused group.
    args[0] = 31;
    level.FunctionResolver(8, args, 1);
    if (level.OnTrigger(31)) { ++failures; }
    level.FunctionResolver(9, args, 1);
    args[1] = 256;
    level.FunctionResolver(34, args, 2);
    if (level.OnTrigger(31)) { ++failures; }
    level.Update(999);
    if (level.OnTrigger(31)) { ++failures; }
    level.Update(1);
    if (!level.OnTrigger(31)) { ++failures; }
    report << " script-contract initial=" << initialCount << " remote-props=" << remoteProps
        << " total=" << allObjects << " failures=" << failures << '\n';
    for (unsigned index = 0; index < map.GetObjectLayerCount(); ++index) {
        const CLayerObject &layer = map.GetObjectLayer(index);
        if (static_cast<int>(layer.GetLayerIndex()) != level.GetObjectLayer()) { continue; }
        unsigned objectId = 0;
        for (const PlacedObject &object : layer.GetObjects()) {
            report << " object=" << objectId++ << " type=" << unsigned(object.objectType)
                << " tag=" << unsigned(object.spawnTag) << " xy=" << object.x << ',' << object.y
                << " path=" << unsigned(object.pathLayer) << '\n';
        }
    }
    for (unsigned index = 0; index < map.GetCollisionLayerCount(); ++index) {
        const CLayerCollision &layer = map.GetCollisionLayer(index);
        if (static_cast<int>(layer.GetLayerIndex()) != level.GetTriggerLayer()) { continue; }
        for (const CollisionEdge &edge : layer.GetCollision().GetEdges()) {
            const auto &a = layer.GetCollision().GetVertices()[edge.firstVertex];
            const auto &b = layer.GetCollision().GetVertices()[edge.secondVertex];
            report << " trigger=" << unsigned(edge.group) << " from=" << a.x << ',' << a.y << " to=" << b.x << ',' << b.y << '\n';
        }
    }
    return failures;
}
}
