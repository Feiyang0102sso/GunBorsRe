/** Original: src/gunbros/enemy.cpp Draw :67499, DrawUI :68451, GetRotationOffset :71429.
 * Windows graphics/resource storage is adapted; original data comes from BIG.
 */
/**
 * @file CEnemyDrawing.cpp
 * @brief One enemy's models, assembled by its script and ready to draw.
 */

#include "gun_bros_re/gameplay/enemy/CEnemy.h"

#include "engine/core/CMatrix4d.h"
#include "engine/graphics/CMeshCamera.h"

#include <cstdio>
#include <cmath>

void CEnemy::GetRotationOffset(float gameScale, float &x, float &y) const {
    const CEnemy::CombatState &state = combat;
    x = 0;
    y = 0;
    const CMesh *mesh = GetPart(0).controller.GetAnimation().GetMesh();
    if (mesh == nullptr) { return; }
    // CMesh::GetRotationOffset shifts the circular hurtbox with the root mesh.
    const ZMeshBounds &bounds = mesh->GetBounds();
    constexpr float radians = 3.14159265f / 180;
    const float cosine = std::cos(state.facing * radians);
    const float sine = std::sin(state.facing * radians);
    const float scale = bounds.inverseExtent * gameScale;
    x = (bounds.centerX * cosine + bounds.centerY * sine) * scale;
    y = ((bounds.centerY * cosine - bounds.centerX * sine) * 0.8660254f - bounds.centerZ * 0.5f) * scale;
}

void CEnemy::GetCollisionCircle(float gameScale, int part,
    float &x, float &y, float &radius) const {
    float offsetX, offsetY;
    GetRotationOffset(gameScale, offsetX, offsetY);
    radius = GetPart(part).radius * combat.scaleFactor;
    x += offsetX * combat.scaleFactor;
    y += offsetY * combat.scaleFactor;
}

namespace {

// The lean CEnemy::Draw asks OrientForGame for. Reference: :98959.
constexpr float kGameTiltDegrees = 30.0f;
constexpr float kDegreesToRadians = 3.14159265f / 180.0f;

}  // namespace

std::int32_t CEnemy::GetPartConfig(std::uint32_t partIndex) const {
    const std::int32_t configIndex =
        GetPart(partIndex).controller.GetMeshConfigIndex();
    if (configIndex < 0 ||
        static_cast<std::size_t>(configIndex) >= configs.size() ||
        !configs[configIndex]->valid) {
        return -1;
    }
    return configIndex;
}

/**
 * Draw every live part against one base matrix.
 *
 * The attachment comes from PART 0 -- its mesh, at its animation time --
 * whichever part is being drawn. CEnemy::Draw (:67499) reads both off part 0's
 * controller and passes the same pair to every GetNodeAt it makes.
 *
 * @param base Row-major, and it must already carry the scale: the vertices go
 *        in raw, so `base` is what turns model units into world ones.
 */
void CEnemy::Draw(const ZShaderProgram &program, const float *base) {
    const CMeshAnimationController &parent =
        GetPart(0).controller.GetAnimation();

    for (std::uint32_t i = 0; i < GetPartCount(); ++i) {
        const std::int32_t configIndex = GetPartConfig(i);
        if (configIndex < 0) {
            continue;
        }

        // Uploaded immediately before the draw, not once per frame: two parts
        // can be showing the same config, and then they share one buffer.
        CEnemy::ModelConfig &config = *configs[configIndex];
        const CMeshAnimationController &animation =
            GetPart(i).controller.GetAnimation();
        if (animation.Evaluate(pose)) {
            config.buffer.SetVertices(pose);
        } else {
            config.buffer.SetFrame(*config.mesh, 0);
        }

        const CEnemy::Part &part = GetPart(i);
        if (!part.visible) {
            continue;
        }
        ZMeshPart placement;
        placement.extraAngleDegrees = part.extraAngleDegrees;
        placement.extraAxisX = part.extraAxisX;
        placement.extraAxisY = part.extraAxisY;
        placement.extraAxisZ = part.extraAxisZ;
        if (part.boneIndex != kEnemyNoBoneIndex) {
            parent.GetNodeAt(static_cast<std::size_t>(part.boneIndex),
                             placement.attachment);
        }

        float mvp[kMatrix4dElements];
        const float *partBase = base;
        float unrotated[16];
        if (combat.enabled && !part.followsFacing) {
            // Undo actor-facing rotation without changing the attachment or
            // independent part rotation. Turret bases stay aligned to ground.
            float pivot[16], rotation[16], translated[16], local[16], next[16];
            const int bodyConfig = GetPartConfig(0);
            ZMeshBounds bounds{};
            if (bodyConfig >= 0) { bounds = configs[bodyConfig]->mesh->GetBounds(); }
            Matrix4dTranslation(bounds.centerX, bounds.centerY, bounds.centerZ, pivot);
            Matrix4dRotationZ(-combat.facing * kDegreesToRadians, rotation);
            Matrix4dTranslation(-bounds.centerX, -bounds.centerY, -bounds.centerZ, translated);
            Matrix4dMultiply(pivot, rotation, next);
            Matrix4dMultiply(next, translated, local);
            Matrix4dMultiply(base, local, unrotated);
            partBase = unrotated;
        }
        MeshCameraBuildPartMatrix(placement, partBase, mvp);
        // CEnemy::Draw :67667 uses white RGB and half the part flash amount.
        // Gun heat retains the buffer's default red overlay.
        const float hitColor[] = {1, 1, 1};
        config.buffer.Draw(program, mvp, *config.texture, part.hitFlash * 0.5f, hitColor);
    }
}

/**
 * How much to scale an enemy's RAW vertices by to put it in the world.
 *
 * `mesh.inverseExtent * runtimeScale * gameScale * cameraScale`, copied from
 * CEnemy::Draw (:67499). The trap is that the model is NOT normalised first --
 * the inverse extent in the product is what normalises it, and the vertices
 * arrive at their authored size. So a turret whose mesh spans 136 units, with
 * a game scale of 150 and a camera at 0.8, is drawn at 136 x (150/136) x 0.8 =
 * 120 world units. Reading the product as if it applied to an already-unit
 * model gives a number a hundred times too small, which is how it looks when
 * this is got wrong.
 *
 * @return 0 when part 0 has no mesh to measure.
 */
float CEnemy::GetWorldScale(float gameScale, float cameraScale) const {
    const std::int32_t configIndex = GetPartConfig(0);
    if (configIndex < 0) {
        return 0.0f;
    }

    // The runtime scale factor CEnemy::SetScaleFactor holds is 1 until
    // something changes it, and nothing in this port does yet.
    // Current callers apply combat.scaleFactor separately; native 61 sets it.
    const float inverseExtent =
        configs[configIndex]->mesh->GetBounds().inverseExtent;
    return inverseExtent * gameScale * cameraScale;
}

/**
 * The matrix CEnemy::Draw stands a model up with, via OrientForGame (:98959).
 *
 * Term for term: `translate(x, y) * scale * translate(-pivot) * rotateX(30) *
 * translate(pivot) * rotateZ(facing) * translate(-pivot)`, where the pivot is
 * part 0's mesh bounds centre -- CEnemy::Draw passes exactly that.
 *
 * The 30-degree lean is why a Gun Bros character reads as three-dimensional
 * from a top-down camera at all; the unbalanced last translate is the
 * original's, and it is what makes a leaning model's feet stay put.
 *
 * @param out Row-major, 16 floats. Must not alias `base`.
 */
void CEnemy::BuildGameMatrix(const float *base, float x,
    float y, float scale, float facingDegrees, float *out) const {
    float pivotX = 0.0f;
    float pivotY = 0.0f;
    float pivotZ = 0.0f;

    const std::int32_t configIndex = GetPartConfig(0);
    if (configIndex >= 0) {
        const ZMeshBounds &bounds = configs[configIndex]->mesh->GetBounds();
        pivotX = bounds.centerX;
        pivotY = bounds.centerY;
        pivotZ = bounds.centerZ;
    }

    float step[kMatrix4dElements];
    float accumulated[kMatrix4dElements];
    float next[kMatrix4dElements];

    // base * translate(x, y)
    Matrix4dTranslation(x, y, 0.0f, step);
    Matrix4dMultiply(base, step, accumulated);

    // * scale
    Matrix4dScale(scale, step);
    Matrix4dMultiply(accumulated, step, next);

    // * translate(-pivot)
    Matrix4dTranslation(-pivotX, -pivotY, -pivotZ, step);
    Matrix4dMultiply(next, step, accumulated);

    // * rotateX(30)
    Matrix4dRotationX(kGameTiltDegrees * kDegreesToRadians, step);
    Matrix4dMultiply(accumulated, step, next);

    // * translate(pivot)
    Matrix4dTranslation(pivotX, pivotY, pivotZ, step);
    Matrix4dMultiply(next, step, accumulated);

    // * rotateZ(facing)
    Matrix4dRotationZ(facingDegrees * kDegreesToRadians, step);
    Matrix4dMultiply(accumulated, step, next);

    // * translate(-pivot), which the original leaves unbalanced
    Matrix4dTranslation(-pivotX, -pivotY, -pivotZ, step);
    Matrix4dMultiply(next, step, out);
}

// Original src/gunbros/enemy.cpp, CEnemy::DrawUI :68451.
bool CEnemy::DrawUI(const ZShaderProgram &program, float x, float y,
    float width, float height, float canvasWidth, float canvasHeight) {
    const int body = GetPartConfig(0);
    if (body < 0) { return true; }
    // CEnemy::GetBoundsInternal :67314: union of integer XY boxes,
    // all scaled by PART 0 inverse extent * 100; attachment is ignored.
    const float units = configs[body]->mesh->GetBounds().inverseExtent * 100;
    int left = 0, top = 0, right = 0, bottom = 0;
    bool bounded = false;
    for (unsigned part = 0; part < GetPartCount(); ++part) {
        const int config = GetPartConfig(part);
        if (config < 0) { continue; }
        const auto &bounds = configs[config]->mesh->GetBounds();
        const int width = static_cast<int>((bounds.maxX - bounds.minX) * units);
        const int height = static_cast<int>((bounds.maxY - bounds.minY) * units);
        if (width == 0 || height == 0) { continue; }
        const int x1 = static_cast<int>(bounds.centerX) - width / 2;
        const int y1 = static_cast<int>(bounds.centerY) - height / 2;
        if (!bounded) { left = x1; top = y1; right = x1 + width; bottom = y1 + height; bounded = true; }
        else { left = std::min(left, x1); top = std::min(top, y1); right = std::max(right, x1 + width); bottom = std::max(bottom, y1 + height); }
    }
    if (!bounded) { return false; }
    const float fit = std::min(width / (right - left), height / (bottom - top)) * data->uiScalePercent / 100;
    float projection3D[16], translation[16], scaling[16], tilt[16], facing[16], first[16], next[16], model[16];
    Matrix4dOrthoTopLeft(canvasWidth, canvasHeight, 32767, projection3D);
    projection3D[11] = -1;
    // CEnemy::DrawUI :68451 anchors at center/bottom; no viewport crop.
    const float originX = static_cast<float>(static_cast<int>(x + width / 2));
    const float originY = static_cast<float>(static_cast<int>(y + height));
    Matrix4dTranslation(originX, originY, -500, translation);
    Matrix4dScale(fit, scaling);
    Matrix4dRotationX(3.14159265f * 0.5f, tilt);
    Matrix4dRotationZ(3.14159265f, facing);
    Matrix4dMultiply(projection3D, translation, first);
    Matrix4dMultiply(first, scaling, next);
    Matrix4dMultiply(next, tilt, first);
    Matrix4dMultiply(first, facing, model);
    Draw(program, model);
    return true;
}
