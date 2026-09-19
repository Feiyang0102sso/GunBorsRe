#pragma once
/** Original renderQueue.cpp: group/Y comparison and shared sprite/model passes.
 * GL batches and the Item payload adapt the original queue to Windows.
 */
#include "gun_bros_re/gameplay/map/CMapResources.h"
#include "engine/core/ZMatrix4d.h"
class CLevel;
class CBullet;
class CRenderQueue {
public:
    static void Draw(CMap &loaded, ZQuadBatch &batch, const ZShaderProgram &program,
                const float *mapMvp, bool showProps = true, CLevel *scene = nullptr,
                CBrother *brotherModel = nullptr, float brotherY = 0, int viewportWidth = 1);
private:
    struct Item {
        int group = 3;
        int y = 0;
        const CProp *prop = nullptr;
        CBrother *player = nullptr;
        CEnemy *enemy = nullptr;
        CBullet *bullet = nullptr;
        CParticleSystem::RenderItem particle;
        float matrix[kMatrix4dElements] = {};
    };
    static bool Compare(const Item &left, const Item &right);
};
