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

CBitmapFont *MovieRenderer::GetFont(unsigned index) {
    auto found = m_fonts.find(index);
    if (found != m_fonts.end()) { return found->second.get(); }
    auto font = std::make_unique<CBitmapFont>();
    if (!font->Init(*m_core, index)) { ++m_failures; return nullptr; }
    CBitmapFont *result = font.get();
    m_fonts[index] = std::move(font);
    return result;
}

MovieRenderer::Animation *MovieRenderer::GetAnimation(unsigned archetype, unsigned animation) {
    const unsigned key = archetype * 256 + animation;
    auto found = m_animations.find(key);
    if (found != m_animations.end()) { return &found->second; }
    const CSpriteGluArchetype *data = m_sprites.GetArchetype(static_cast<std::uint8_t>(archetype));
    if (data == nullptr || data->GetAnimationCount() == 0) {
        std::printf("[movie] invalid sprite archetype=%u animation=%u\n", archetype, animation);
        ++m_failures;
        return nullptr;
    }
    if (animation >= data->GetAnimationCount()) {
        // CSpritePlayer::SetAnimation :58861 clamps to animationCount-1.
        // MDS_ICON_STANDARD's coin is actually 4:43 while the BIG has33
        // animations. Preserve the raw key and follow the native consumer.
        const unsigned original = animation;
        animation = data->GetAnimationCount() - 1;
        std::printf("[movie] native animation clamp archetype=%u requested=%u resolved=%u\n", archetype, original, animation);
    }
    Animation value;
    float left = 100000, top = 100000, right = -100000, bottom = -100000;
    CSpriteIterator iterator(m_sprites, *data);
    const auto &steps = data->GetAnimation(animation).steps;
    for (unsigned index = 0; index < steps.size(); ++index) {
        std::vector<SpriteQuad> quads;
        if (!iterator.Expand(static_cast<std::uint8_t>(animation), index, quads)) { ++m_failures; return nullptr; }
        for (const SpriteQuad &quad : quads) {
            left = std::min(left, static_cast<float>(quad.offsetX));
            top = std::min(top, static_cast<float>(quad.offsetY));
            right = std::max(right, static_cast<float>(quad.offsetX + quad.Width()));
            bottom = std::max(bottom, static_cast<float>(quad.offsetY + quad.Height()));
        }
        value.steps.push_back(std::move(quads));
        value.durations.push_back(steps[index].durationMs);
        value.duration += steps[index].durationMs;
    }
    if (left <= right) { value.bounds = {0, 0, left, top, right - left, bottom - top}; }
    return &m_animations.emplace(key, std::move(value)).first->second;
}

MovieKeyFrame MovieRenderer::AtTime(const MovieObject &object, unsigned time) const {
    MovieKeyFrame result;
    result.visible = false;
    if (object.frames.empty() || time < object.frames.front().time) { return result; }
    unsigned current = 0;
    while (current + 1 < object.frames.size() && object.frames[current + 1].time <= time) { ++current; }
    result = object.frames[current];
    if (current + 1 >= object.frames.size()) { return result; }
    const MovieKeyFrame &next = object.frames[current + 1];
    const float progress = static_cast<float>(time - result.time) / (next.time - result.time);
    result.x = static_cast<std::int16_t>(result.x + (next.x - result.x) * progress);
    result.y = static_cast<std::int16_t>(result.y + (next.y - result.y) * progress);
    result.alpha += (next.alpha - result.alpha) * progress;
    result.scaleX += (next.scaleX - result.scaleX) * progress;
    result.scaleY += (next.scaleY - result.scaleY) * progress;
    result.rotation += (next.rotation - result.rotation) * progress;
    result.tileX = static_cast<std::int32_t>(result.tileX + (static_cast<double>(next.tileX) - result.tileX) * progress);
    result.tileY = static_cast<std::int32_t>(result.tileY + (static_cast<double>(next.tileY) - result.tileY) * progress);
    result.width = static_cast<std::int16_t>(result.width + (next.width - result.width) * progress);
    result.height = static_cast<std::int16_t>(result.height + (next.height - result.height) * progress);
    return result;
}

MovieRenderer::Metrics MovieRenderer::GetMetrics(const CMovie &movie, unsigned objectIndex, unsigned time, float width, float height, unsigned depth) {
    if (depth > movie.objects.size() || objectIndex >= movie.objects.size()) { return {}; }
    const MovieObject &object = movie.objects[objectIndex];
    if (object.type != 6 || object.frames.empty()) {
        return GetFrameMetrics(movie, objectIndex, AtTime(object, time), time, width, height, depth);
    }
    // GetMetricsAtTime :182335 and CalculateLocation :182636 resolve both
    // key locations before interpolation. A changed parent uses its key time.
    unsigned index = 0;
    while (index + 1 < object.frames.size() && object.frames[index + 1].time <= time) { ++index; }
    const MovieKeyFrame &first = object.frames[index];
    if (index + 1 == object.frames.size() || time <= first.time) {
        return GetFrameMetrics(movie, objectIndex, first, time, width, height, depth);
    }
    const MovieKeyFrame &last = object.frames[index + 1];
    unsigned firstTime = time, lastTime = time;
    if (first.parent != last.parent) { firstTime = first.time; lastTime = last.time; }
    const Metrics before = GetFrameMetrics(movie, objectIndex, first, firstTime, width, height, depth);
    const Metrics after = GetFrameMetrics(movie, objectIndex, last, lastTime, width, height, depth);
    const float progress = float(time - first.time) / (last.time - first.time);
    Metrics result;
    result.x = std::floor(before.x + (after.x - before.x) * progress);
    result.y = std::floor(before.y + (after.y - before.y) * progress);
    result.width = std::floor(before.width + (after.width - before.width) * progress);
    result.height = std::floor(before.height + (after.height - before.height) * progress);
    return result;
}

MovieRenderer::Metrics MovieRenderer::GetFrameMetrics(const CMovie &movie, unsigned objectIndex, const MovieKeyFrame &frame,
    unsigned time, float width, float height, unsigned depth) {
    Metrics metrics;
    if (depth > movie.objects.size() || objectIndex >= movie.objects.size()) { return metrics; }
    const MovieObject &object = movie.objects[objectIndex];
    if (object.type == 0) {
        Animation *animation = GetAnimation(frame.content[0], frame.content[2]);
        if (animation != nullptr) { metrics = animation->bounds; }
    } else {
        metrics.width = frame.width;
        metrics.height = frame.height;
        if (frame.content[2] == 253) { metrics.width *= width / movie.width; }
        if (frame.content[3] == 253) { metrics.height *= height / movie.height; }
    }
    // Region bounds stay unscaled (:182603); Draw transforms around the center.
    if (object.type != 6) {
        metrics.left *= frame.scaleX;
        metrics.top *= frame.scaleY;
        metrics.width *= frame.scaleX;
        metrics.height *= frame.scaleY;
    }
    metrics.x = frame.x;
    metrics.y = frame.y;
    if (frame.parent != 255) {
        Metrics parent;
        if (frame.parent == 254 || frame.parent == 253) { parent = {-width * 0.5f, -height * 0.5f, 0, 0, width, height}; }
        else { parent = GetMetrics(movie, frame.parent, time, width, height, depth + 1); }
        metrics.x += parent.x + parent.left + AnchorX(frame.parentAnchor, parent.width) - metrics.left - AnchorX(frame.selfAnchor, metrics.width);
        metrics.y += parent.y + parent.top + AnchorY(frame.parentAnchor, parent.height) - metrics.top - AnchorY(frame.selfAnchor, metrics.height);
    }
    if (object.type != 0) {
        // CMovieEmptyRegion binds the far edge independently from its origin.
        // This is what stretches navigation bars and panels to the iPad width.
        if (frame.content[2] != 255 && frame.content[2] != 253) {
            Metrics edge;
            if (frame.content[2] == 254) { edge = {-width * 0.5f, -height * 0.5f, 0, 0, width, height}; }
            else { edge = GetMetrics(movie, frame.content[2], time, width, height, depth + 1); }
            metrics.width = std::max(0.0f, edge.x + edge.left + AnchorX(frame.content[1], edge.width) - metrics.x);
        }
        if (frame.content[3] != 255 && frame.content[3] != 253) {
            Metrics edge;
            if (frame.content[3] == 254) { edge = {-width * 0.5f, -height * 0.5f, 0, 0, width, height}; }
            else { edge = GetMetrics(movie, frame.content[3], time, width, height, depth + 1); }
            metrics.height = std::max(0.0f, edge.y + edge.top + AnchorY(frame.content[1], edge.height) - metrics.y);
        }
    }
    return metrics;
}

void MovieRenderer::Flush() { m_batch.Upload(); m_batch.Draw(m_program, m_projection); m_batch.Begin(); }

void MovieRenderer::Rectangle(float x, float y, float width, float height, float r, float g, float b, float alpha) {
    m_markers.Begin();
    m_markers.AddRect(x, y, width, height);
    m_markers.Draw(m_colorProgram, m_projection, r, g, b, alpha);
}

void MovieRenderer::Image(const CTexture &texture, float x, float y, float width, float height, bool flipVertical) {
    const float scale = std::min(width / texture.GetWidth(), height / texture.GetHeight());
    const float drawnWidth = texture.GetWidth() * scale;
    const float drawnHeight = texture.GetHeight() * scale;
    const SourceRect source{0, 0, static_cast<std::uint16_t>(texture.GetWidth()), static_cast<std::uint16_t>(texture.GetHeight())};
    m_batch.Begin();
    m_batch.AddQuad(texture, x + (width - drawnWidth) / 2, y + (height - drawnHeight) / 2,
        drawnWidth, drawnHeight, source, false, flipVertical, BlendMode::Alpha);
    Flush();
}

bool MovieRenderer::Text(const std::string &text, float x, float y, unsigned font, float scale, float maxWidth, float alpha) {
    CBitmapFont *value = GetFont(font);
    if (value == nullptr) { return false; }
    if (maxWidth > 0 && value->Width(text, scale) > maxWidth) { scale *= maxWidth / value->Width(text, scale); }
    m_batch.Begin();
    value->Draw(m_batch, text, x, y, scale, alpha);
    Flush();
    return true;
}

float MovieRenderer::TextHeight(unsigned font, float scale) {
    CBitmapFont *bitmap = GetFont(font);
    if (bitmap == nullptr) { return 0; }
    return bitmap->Height(scale);
}

float MovieRenderer::TextWidth(const std::string &text, unsigned font, float scale) {
    CBitmapFont *value = GetFont(font);
    if (value == nullptr) { return 0; }
    return value->Width(text, scale);
}

bool MovieRenderer::SpriteFrameTimes(unsigned archetype, unsigned animation, std::vector<unsigned> &times) {
    const auto *data = GetAnimation(archetype, animation);
    if (data == nullptr) { return false; }
    times.clear();
    unsigned time = 0;
    for (unsigned duration : data->durations) { times.push_back(time); time += duration; }
    return !times.empty();
}

bool MovieRenderer::DrawSprite(unsigned archetype, unsigned animationIndex, unsigned time, float x, float y, float scale, float alpha, float rotation) {
    Animation *animation = GetAnimation(archetype, animationIndex);
    if (animation == nullptr || animation->steps.empty()) { return false; }
    unsigned step = 0;
    if (animation->duration != 0) {
        unsigned remaining = time % animation->duration;
        while (step + 1 < animation->steps.size() && remaining >= animation->durations[step]) { remaining -= animation->durations[step++]; }
    }
    m_batch.Begin();
    for (const SpriteQuad &quad : animation->steps[step]) {
        m_batch.AddTransformedQuad(*quad.page, x + quad.offsetX * scale, y + quad.offsetY * scale,
            quad.Width() * scale, quad.Height() * scale, quad.source, quad.flipHorizontal, quad.flipVertical,
            quad.blend, x, y, 1, 1, rotation, alpha, quad.rotateTexture);
    }
    Flush();
    return true;
}

bool MovieRenderer::DrawSpriteFitted(unsigned archetype, unsigned animationIndex, unsigned time, float x, float y, float width, float height, float alpha) {
    Animation *animation = GetAnimation(archetype, animationIndex);
    if (animation == nullptr || animation->bounds.width <= 0 || animation->bounds.height <= 0) { return false; }
    const float scale = std::min(width / animation->bounds.width, height / animation->bounds.height);
    return DrawSprite(archetype, animationIndex, time,
        x + (width - animation->bounds.width * scale) * 0.5f - animation->bounds.left * scale,
        y + (height - animation->bounds.height * scale) * 0.5f - animation->bounds.top * scale, scale, alpha);
}

bool MovieRenderer::ButtonBackground(float x, float y, float width, float height, bool selected, bool hovered) {
    // MDS_BUTTON_STORE / MISSION_INFO: blue and green backgrounds are
    // sprites 62/64 (small), 65/67 (medium), and 70/72 (large).
    unsigned background = 62;
    unsigned glow = 75;
    const float aspect = width / std::max(1.0f, height);
    if (aspect >= 3.2f) { background = 65; glow = 76; }
    if (aspect >= 4.7f) { background = 70; glow = 77; }
    if (selected) { background += 2; }
    if (!StretchButtonSprite(background, x, y, width, height)) { return false; }
    if (hovered) { return StretchButtonSprite(glow, x, y, width, height); }
    return true;
}

bool MovieRenderer::StretchButtonSprite(unsigned animationIndex, float x, float y, float width, float height) {
    Animation *animation = GetAnimation(0, animationIndex);
    if (animation == nullptr || animation->steps.empty() || animation->bounds.width <= 0 || animation->bounds.height <= 0) { return false; }
    // Host buttons have different widths from the authored MDS instances.
    // Fill the actual hit rectangle so fitted text stays inside its background.
    const float scaleX = width / animation->bounds.width;
    const float scaleY = height / animation->bounds.height;
    m_batch.Begin();
    for (const SpriteQuad &quad : animation->steps[0]) {
        m_batch.AddTransformedQuad(*quad.page, x + (quad.offsetX - animation->bounds.left) * scaleX,
            y + (quad.offsetY - animation->bounds.top) * scaleY, quad.Width() * scaleX,
            quad.Height() * scaleY, quad.source, quad.flipHorizontal, quad.flipVertical,
            quad.blend, 0, 0, 1, 1, 0, 1, quad.rotateTexture);
    }
    Flush();
    return true;
}

bool MovieRenderer::DrawNamed(const char *name, unsigned time, float x, float y) {
    const int ordinal = FindMovie(name);
    if (ordinal < 0) { return false; }
    return Draw(ordinal, time, x, y);
}

bool MovieRenderer::DrawFitted(unsigned ordinal, unsigned time, float x, float y, float width, float height, unsigned regionIndex) {
    for (const MovieRegion &region : Regions(ordinal, time)) {
        if (region.index != regionIndex || region.width <= 0 || region.height <= 0) { continue; }
        float previous[16], transform[16], projection[16];
        std::memcpy(previous, m_projection, sizeof(previous));
        Matrix4dIdentity(transform);
        transform[0] = width / region.width;
        transform[5] = height / region.height;
        transform[3] = x - region.x * transform[0];
        transform[7] = y - region.y * transform[5];
        Matrix4dMultiply(previous, transform, projection);
        std::memcpy(m_projection, projection, sizeof(projection));
        const bool result = Draw(ordinal, time);
        std::memcpy(m_projection, previous, sizeof(previous));
        return result;
    }
    return false;
}

bool MovieRenderer::Draw(unsigned ordinal, unsigned time, float x, float y, float width, float height, unsigned depth, float alpha,
    IMovieRegionCallback *callback) {
    const unsigned failuresBefore = m_failures;
    if (depth > 8) { ++m_failures; return false; }
    CMovie *movie = GetMovie(ordinal);
    if (movie == nullptr) { return false; }
    std::vector<unsigned> order;
    for (unsigned index = 0; index < movie->objects.size(); ++index) { order.push_back(index); }
    // Stable insertion sort keeps the original object order within a draw layer.
    for (unsigned index = 1; index < order.size(); ++index) {
        unsigned position = index;
        while (position > 0 && AtTime(movie->objects[order[position]], time).layer < AtTime(movie->objects[order[position - 1]], time).layer) {
            std::swap(order[position], order[position - 1]);
            --position;
        }
    }
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    m_batch.Begin();
    for (unsigned index : order) {
        const MovieObject &object = movie->objects[index];
        const MovieKeyFrame frame = AtTime(object, time);
        if (!frame.visible || frame.alpha <= 0) { continue; }
        if (object.type == 6 && callback != nullptr) {
            // CMovieRegion::Draw :109978 dispatches inside the Movie draw order.
            // Planet sprites must not be painted over every foreground layer.
            unsigned regionIndex = 0;
            for (unsigned previous = 0; previous < index; ++previous) {
                if (movie->objects[previous].type == 6) { ++regionIndex; }
            }
            const Metrics metrics = GetMetrics(*movie, index, time, width, height);
            const MovieRegion region{regionIndex, frame.region, x + metrics.x, y + metrics.y,
                metrics.width, metrics.height, frame.alpha * alpha};
            Flush();
            float previous[16], transform[16], transformed[16];
            std::memcpy(previous, m_projection, sizeof(previous));
            Matrix4dIdentity(transform);
            const float angle = frame.rotation * 3.14159265358979323846f / 180;
            const float cosine = std::cos(angle), sine = std::sin(angle);
            transform[0] = cosine * frame.scaleX;
            transform[1] = -sine * frame.scaleY;
            transform[4] = sine * frame.scaleX;
            transform[5] = cosine * frame.scaleY;
            const float centerX = region.x + region.width / 2;
            const float centerY = region.y + region.height / 2;
            transform[3] = centerX - centerX * transform[0] - centerY * transform[1];
            transform[7] = centerY - centerX * transform[4] - centerY * transform[5];
            Matrix4dMultiply(previous, transform, transformed);
            std::memcpy(m_projection, transformed, sizeof(transformed));
            const bool drawn = callback->DrawMovieRegion(region);
            std::memcpy(m_projection, previous, sizeof(previous));
            if (!drawn) { return false; }
            m_batch.Begin();
            continue;
        }
        if (object.type == 2) {
            if (frame.content[0] == 255) { continue; }
            Flush();
            CMovie *child = GetMovie(frame.content[0]);
            if (child == nullptr) { return false; }
            unsigned childTime = time - frame.time;
            if (frame.content[1] != 0 && child->duration > 0) { childTime %= child->duration; }
            if (!Draw(frame.content[0], childTime, x, y, width, height, depth + 1, alpha)) { return false; }
            continue;
        }
        if (object.type != 0 && object.type != 1 && object.type != 7) { continue; }
        const Metrics metrics = GetMetrics(*movie, index, time, width, height);
        if (object.type == 7) {
            Flush();
            // CMovieFill::Draw uses Utility::GradientY with two original RGB colors.
            std::uint64_t key = 0;
            for (std::uint8_t color : frame.colors) { key = key * 256 + color; }
            if (m_gradients.count(key) == 0) {
                PNGImage pixels;
                pixels.width = 1;
                pixels.height = 256;
                for (unsigned row = 0; row < 256; ++row) {
                    for (unsigned channel = 0; channel < 3; ++channel) {
                        pixels.pixels.push_back(static_cast<std::uint8_t>((frame.colors[channel] * (255 - row) + frame.colors[channel + 3] * row) / 255));
                    }
                    pixels.pixels.push_back(255);
                }
                auto texture = std::make_unique<CTexture>();
                if (!texture->Create(pixels)) { ++m_failures; return false; }
                m_gradients[key] = std::move(texture);
            }
            const SourceRect source{0, 0, 1, 256};
            m_batch.AddTransformedQuad(*m_gradients[key], x + metrics.x, y + metrics.y, metrics.width, metrics.height,
                source, false, false, BlendMode::Alpha, 0, 0, 1, 1, 0, frame.alpha * alpha);
            Flush();
            continue;
        }
        unsigned archetype = frame.content[0];
        unsigned animationIndex = frame.content[2];
        if (object.type == 1) { archetype = frame.tiledSprite[0]; animationIndex = frame.tiledSprite[2]; }
        if (archetype == 255 || animationIndex == 255) { continue; }
        Animation *animation = GetAnimation(archetype, animationIndex);
        if (animation == nullptr || animation->steps.empty()) { return false; }
        unsigned step = 0;
        if (animation->duration != 0) {
            // CMovieSprite::GetCurrentFrame :110298 preserves an animation's
            // clock across adjacent transform keys using the same sprite.
            unsigned firstKey = 0;
            while (firstKey + 1 < object.frames.size() && object.frames[firstKey + 1].time <= time) { ++firstKey; }
            while (firstKey > 0) {
                const MovieKeyFrame &previous = object.frames[firstKey - 1];
                const std::array<std::uint8_t, 4> *previousSprite = &previous.content;
                const std::array<std::uint8_t, 4> *currentSprite = &frame.content;
                if (object.type == 1) { previousSprite = &previous.tiledSprite; currentSprite = &frame.tiledSprite; }
                if ((*previousSprite)[0] != (*currentSprite)[0] || (*previousSprite)[1] != (*currentSprite)[1] ||
                    (*previousSprite)[2] != (*currentSprite)[2]) { break; }
                --firstKey;
            }
            unsigned elapsed = (time - object.frames[firstKey].time) % animation->duration;
            while (step + 1 < animation->steps.size() && elapsed >= animation->durations[step]) { elapsed -= animation->durations[step++]; }
        }
        if (object.type == 0) {
            for (const SpriteQuad &quad : animation->steps[step]) {
                m_batch.AddTransformedQuad(*quad.page, x + metrics.x + quad.offsetX, y + metrics.y + quad.offsetY,
                    static_cast<float>(quad.Width()), static_cast<float>(quad.Height()), quad.source,
                    quad.flipHorizontal, quad.flipVertical, quad.blend, x + metrics.x, y + metrics.y,
                    frame.scaleX, frame.scaleY, frame.rotation, frame.alpha * alpha, quad.rotateTexture);
            }
        } else {
            // Panel strips repeat their original pixels; edge tiles crop rather than stretch.
            const float tileWidth = animation->bounds.width;
            const float tileHeight = animation->bounds.height;
            if (tileWidth <= 0 || tileHeight <= 0) { continue; }
            // CMovieTiledSprite::Draw :111690 wraps the 16.16 phase. Positive
            // phase moves right/down, including a clipped leading tile.
            float phaseX = (static_cast<unsigned>(frame.tileX) & 65535) * tileWidth / 65536.0f;
            float phaseY = (static_cast<unsigned>(frame.tileY) & 65535) * tileHeight / 65536.0f;
            if (phaseX > 0) { phaseX -= tileWidth; }
            if (phaseY > 0) { phaseY -= tileHeight; }
            for (float tileY = phaseY; tileY < metrics.height; tileY += tileHeight) {
                for (float tileX = phaseX; tileX < metrics.width; tileX += tileWidth) {
                    for (const SpriteQuad &quad : animation->steps[step]) {
                        const float rawX = tileX + quad.offsetX - animation->bounds.left;
                        const float rawY = tileY + quad.offsetY - animation->bounds.top;
                        const float drawX = std::max(0.0f, rawX), drawY = std::max(0.0f, rawY);
                        const float right = std::min(rawX + quad.Width(), metrics.width);
                        const float bottom = std::min(rawY + quad.Height(), metrics.height);
                        const float drawWidth = right - drawX, drawHeight = bottom - drawY;
                        if (drawWidth <= 0 || drawHeight <= 0) { continue; }
                        float sourceX = drawX - rawX, sourceY = drawY - rawY;
                        SourceRect source = quad.source;
                        if (quad.rotateTexture) {
                            // A packed quarter-turn maps display Y to atlas X.
                            sourceX = drawY - rawY;
                            sourceY = drawX - rawX;
                            if (quad.flipHorizontal) { sourceX = quad.Height() - bottom + rawY; }
                            if (quad.flipVertical) { sourceY = quad.Width() - right + rawX; }
                            source.width = static_cast<unsigned short>(std::ceil(drawHeight));
                            source.height = static_cast<unsigned short>(std::ceil(drawWidth));
                        } else {
                            if (quad.flipHorizontal) { sourceX = quad.Width() - right + rawX; }
                            if (quad.flipVertical) { sourceY = quad.Height() - bottom + rawY; }
                            source.width = static_cast<unsigned short>(std::ceil(drawWidth));
                            source.height = static_cast<unsigned short>(std::ceil(drawHeight));
                        }
                        source.x += static_cast<unsigned short>(std::max(0.0f, sourceX));
                        source.y += static_cast<unsigned short>(std::max(0.0f, sourceY));
                        m_batch.AddTransformedQuad(*quad.page, x + metrics.x + drawX, y + metrics.y + drawY, drawWidth, drawHeight,
                            source, quad.flipHorizontal, quad.flipVertical, quad.blend, x + metrics.x, y + metrics.y,
                            1, 1, frame.rotation, frame.alpha * alpha, quad.rotateTexture);
                    }
                }
            }
        }
    }
    Flush();
    if (m_regionOverlay) {
        for (const MovieRegion &region : Regions(ordinal, time, x, y)) {
            m_markers.Begin();
            m_markers.AddOutline(region.x, region.y, region.width, region.height, 1);
            m_markers.Draw(m_colorProgram, m_projection, 0, 1, 0, 0.7f);
            Text(std::to_string(region.index), region.x, region.y, 0, 0.65f);
        }
    }
    return m_failures == failuresBefore;
}

/** Same 256-sample gradient used by CMovieFill; Utility::GradientY adapter. */
bool MovieRenderer::Gradient(float x, float y, float width, float height, unsigned topRgb, unsigned bottomRgb, float alpha) {
    if (width <= 0 || height <= 0) { return true; }
    const std::uint64_t key = (std::uint64_t(topRgb & 0xffffff) << 24) | (bottomRgb & 0xffffff);
    if (m_gradients.count(key) == 0) {
        PNGImage pixels;
        pixels.width = 1;
        pixels.height = 256;
        for (unsigned row = 0; row < 256; ++row) {
            for (unsigned channel = 0; channel < 3; ++channel) {
                const unsigned shift = (2 - channel) * 8;
                pixels.pixels.push_back(std::uint8_t((((topRgb >> shift) & 255) * (255 - row) + ((bottomRgb >> shift) & 255) * row) / 255));
            }
            pixels.pixels.push_back(255);
        }
        auto texture = std::make_unique<CTexture>();
        if (!texture->Create(pixels)) { ++m_failures; return false; }
        m_gradients[key] = std::move(texture);
    }
    m_batch.Begin();
    m_batch.AddTransformedQuad(*m_gradients[key], x, y, width, height, {0, 0, 1, 256}, false, false,
        BlendMode::Alpha, 0, 0, 1, 1, 0, alpha);
    Flush();
    return true;
}
