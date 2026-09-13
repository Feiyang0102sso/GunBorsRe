#include "engine/core/Paths.h"
/** @file MovieRenderer.cpp
 * @brief Original 3x3 anchors, 16.16 transforms, layered sprites and tiled panels.
 */
#define NOMINMAX
#include "engine/glu/movie/MovieRenderer.h"
#include "engine/core/CMatrix4d.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
float AnchorX(unsigned anchor, float width) {
    if (anchor > 8) { return 0; }
    return (anchor % 3) * width * 0.5f;
}
float AnchorY(unsigned anchor, float height) {
    if (anchor > 8) { return 0; }
    return (anchor / 3) * height * 0.5f;
}
}

bool MovieRenderer::Init(CResPackTOC &pack, CResPackTOC &core) {
    m_pack = &pack;
    m_core = &core;
    if (!m_sprites.Init(pack) ||
        !m_program.Load(Paths::Shaders().c_str(), "ogles_vs_mvp_tex0", "ogles_ps_tex0") ||
        !m_colorProgram.Load(Paths::Shaders().c_str(), "ogles_vs_mvp_constcolor", "ogles_ps_constcolor") ||
        !m_batch.Create(m_program) || !m_markers.Create(m_colorProgram)) { return false; }
    Matrix4dOrthoTopLeft(1024, 768, 1, m_projection);
    return GetFont(0) != nullptr;
}

CMovie *MovieRenderer::GetMovie(unsigned ordinal) {
    auto found = m_movies.find(ordinal);
    if (found != m_movies.end()) { return &found->second; }
    const unsigned base = m_pack->GetResValue("GLU_MOVIE_MOVIE");
    std::vector<std::uint8_t> bytes;
    if (!m_pack->GetResource(base + ordinal, bytes)) { ++m_failures; return nullptr; }
    CArrayInputStream input(bytes);
    CMovie movie;
    if (!movie.Init(input)) { ++m_failures; return nullptr; }
    return &m_movies.emplace(ordinal, std::move(movie)).first->second;
}

int MovieRenderer::FindMovie(const char *name) const {
    const unsigned handle = m_pack->GetResValue(name);
    if (handle == 0) { return -1; }
    return static_cast<int>(handle - m_pack->GetResValue("GLU_MOVIE_MOVIE"));
}

unsigned MovieRenderer::Ordinal(const char *name) {
    auto found = m_ordinals.find(name);
    if (found != m_ordinals.end()) { return found->second; }
    const int ordinal = FindMovie(name);
    if (ordinal < 0) {
        std::printf("[movie] unknown alias %s\n", name);
        ++m_failures;
        return 0;
    }
    m_ordinals[name] = static_cast<unsigned>(ordinal);
    return static_cast<unsigned>(ordinal);
}

unsigned MovieRenderer::SpriteDuration(unsigned archetype, unsigned animationIndex) {
    Animation *animation = GetAnimation(archetype, animationIndex);
    if (animation == nullptr) { return 0; }
    return animation->duration;
}

bool MovieRenderer::SpriteBounds(unsigned archetype, unsigned animationIndex, MovieRegion &bounds) {
    Animation *animation = GetAnimation(archetype, animationIndex);
    if (animation == nullptr || animation->steps.empty()) { return false; }
    bounds.x = animation->bounds.left;
    bounds.y = animation->bounds.top;
    bounds.width = animation->bounds.width;
    bounds.height = animation->bounds.height;
    return true;
}

std::string MovieRenderer::NamedString(const char *name, unsigned offset) {
    if (name == nullptr || name[0] == 0) { return {}; }
    std::string key = name;
    if (offset != 0) { key += ":" + std::to_string(offset); }
    auto found = m_strings.find(key);
    if (found != m_strings.end()) { return found->second; }
    std::vector<std::uint8_t> bytes;
    const unsigned handle = m_core->GetResValue(name);
    if (handle == 0 || !m_core->GetResource(handle + offset, bytes)) { return {}; }
    std::string result;
    for (std::uint8_t byte : bytes) {
        if (byte == 0) { break; }
        result.push_back(static_cast<char>(byte));
    }
    m_strings[key] = result;
    return result;
}

bool MovieRenderer::BindSpritePlayer(unsigned archetype, unsigned animationIndex, CSpritePlayer &player) {
    Animation *animation = GetAnimation(archetype, animationIndex);
    if (animation == nullptr || animation->steps.empty()) { return false; }
    player.SetAnimation(&animation->durations);
    player.SetLooping(false);
    return true;
}

bool MovieRenderer::DrawSpritePlayer(unsigned archetype, unsigned animationIndex, const CSpritePlayer &player,
    float x, float y, float alpha) {
    Animation *animation = GetAnimation(archetype, animationIndex);
    if (animation == nullptr || player.GetStep() >= animation->steps.size()) { return false; }
    unsigned time = 0;
    for (unsigned step = 0; step < player.GetStep(); ++step) { time += animation->durations[step]; }
    return DrawSprite(archetype, animationIndex, time, x, y, 1, alpha);
}
