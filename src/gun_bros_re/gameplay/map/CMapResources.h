#pragma once
#include "gun_bros_re/effects/CParticleSystem.h"
#include "gun_bros_re/effects/CParticleEffectPlayer.h"
/** Desktop resource caches and placed-object storage shared by game and viewer.
 * Original CMap, CProp and CParticleEffect still own their parsed data/behavior.
 * Declare storage before consumers so referenced templates outlive their actors.
 */
#include "engine/core/ZPaths.h"
#include "engine/glu/sprite/CSpriteGlu.h"
#include "engine/glu/sprite/CSpriteIterator.h"
#include "engine/glu/sprite/CSpritePlayer.h"
#include "gun_bros_re/gameplay/map/CMap.h"
#include "gun_bros_re/effects/CParticleEffect.h"
#include "gun_bros_re/gameplay/map/CPropResources.h"
#include "gun_bros_re/gameplay/map/TileSet.h"
#include "gun_bros_re/gameplay/enemy/CEnemy.h"
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "gun_bros_re/gameplay/weapon/CBullet.h"
#include "gun_bros_re/data/CGameObjectPack.h"
#include <array>
#include <map>
#include <memory>
#include <vector>

struct CMap::Resources {
    /** The per-pack tables a prop needs, built the first time that pack is used. */
    struct Pack {
        CGameObjectPack objectPack;
        CSpriteGlu spriteGlu;
        bool objectPackReady;
        bool spriteGluReady;

        Pack() : objectPackReady(false), spriteGluReady(false) {}
    };

    /** Sprite animations belonging to one emitter in a particle template. */
    struct ParticleEmitterVisual {
        std::array<CProp::Animation, 32> animations;
    };

    /** Parsed particle data plus the already-expanded atlas quads it draws. */
    struct ParticleEffectVisual {
        CParticleEffect effect;
        std::vector<ParticleEmitterVisual> emitters;
    };

    /** One effect attached to a prop position until its particles finish. */
    struct ActiveParticleEffect {
        std::uint64_t visualKey;
        float x;
        float y;
        int zOrderGroup;
        std::uint32_t randomState;
        CParticleEffectPlayer player;
    };

    /** A player standing on one of the map's spawn points. */
    struct Player {
        float x;
        float y;
        float facingDegrees;
        bool moving;

        // By pointer for the same reason a PlacedEnemy's model is: it owns GL
        // buffers and its controllers point back into it, so it cannot be moved
        // once built.
        std::unique_ptr<CBrother> model;

        Player()
            : x(0.0f), y(0.0f), facingDegrees(0.0f), moving(false) {}
    };

    TileSet tileSet;
    std::vector<std::unique_ptr<ZTexture>> textures;

    // Effective player collision: the level-selected map layer plus every
    // placed prop's local collision translated into world space.
    CCollisionData collisionScene;
    CCollisionData::Scene weaponCollision;

    // The enemies the object layer places. Held by pointer because an
    // EnemyModel owns GL buffers and points at its own meshes.
    // Templates precede actors so they outlive every borrowed script/move set.
    // Both by pointer, and both for the same reason: CEnemy::Bind keeps the
    // ADDRESS of the move set and the script, so the template has to stay put
    // for as long as the model does. Holding either by value here would leave
    // the model pointing at freed memory the moment this vector grew.
    // The template's game scale, which is half of how big it is drawn.
    // CEnemy now owns the old model resources directly; scale stays in Template.
    std::vector<std::unique_ptr<CEnemy::Template>> enemyTemplates;
    std::vector<std::unique_ptr<CEnemy>> enemies;

    // The player template, owned here because every PlacedPlayer's controllers
    // hold the ADDRESS of its move set -- the same trap PlacedEnemy documents.
    std::unique_ptr<CBrother::Template> playerTemplate;
    std::vector<Player> players;

    // Props, and everything they hang off. The caches own the atlas pages, so
    // they have to outlive the props that point into them -- replacing a
    // LoadedMap wholesale is what keeps that true.
    std::map<int, std::unique_ptr<Pack>> packs;
    std::map<std::uint64_t, CProp::Resources> propSprites;
    std::vector<CProp> props;

    // Effects are cached by their game-object address and instantiated only
    // when a transition asks for one.
    std::map<std::uint64_t, ParticleEffectVisual> particleEffects;
    std::vector<ActiveParticleEffect> activeParticleEffects;
    // CMap's transient effect pool, allocated with 200 slots (:91849).
    std::shared_ptr<CParticlePool> particlePool = std::make_shared<CParticlePool>(200);
    // CMap's CParticleSystem is a separate pool (:133966), used by pickups.
    std::shared_ptr<CParticleSystem> particleSystem = std::make_shared<CParticleSystem>();
};

namespace MapDetail {
CMap::Resources::Pack *GetPackResources(CResTOCManager &tocManager, CMap &loaded,
                                int packIndex);
bool ReadSectionResource(CResTOCManager &tocManager, CMap &loaded,
                         std::uint32_t packHash, ZGameSection section,
                         std::uint32_t localIndex,
                         std::vector<std::uint8_t> &payload);
std::uint64_t AssetKey(std::uint32_t packHash, std::uint32_t localIndex);
CResPackTOC *OpenPack(CResTOCManager &tocManager, const std::string &bigDirectory,
                      const std::string &packShortName);
}
