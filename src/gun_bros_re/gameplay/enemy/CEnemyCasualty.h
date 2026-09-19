/** @file CEnemyCasualty.h
 * @brief Enemy totals consumed by the casualties menu.
 * Original: level.cpp CStatisticEnemy::Combine :122531 and OnEnemyKilled :119306.
 * This is the host's display aggregate, not the complete original statistics
 * record (weapon/group/critical buckets remain in CChallengeManager).
 */
#pragma once
#include "gun_bros_re/data/objects/CGameAssetRef.h"
#include <string>
struct CEnemyCasualty {
    GameObjectRef resource;
    unsigned count = 0;
    std::string name;
};
