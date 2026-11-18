#include "viz/FeedbackTunnel.h"

#include "render/Fullscreen.h"
#include "viz/WaveLoop.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;

// Resamples the previous frame around the (possibly drifting) vanishing point:
// zoom outward, rotate (more strongly near the centre when swirling), ripple,
// shift hue and fade.
constexpr const char* kWarpFragmentSrc = R"(#version 330 core
in vec2 vUv;

uniform sampler2D uPrevious;
uniform vec2 uAspect;     // (width / height, 1) so rotation stays circular
uniform vec2 uCenter;     // vanishing point, 0..1
uniform float uZoom;      // > 1 pulls the old image outward
uniform float uAngle;
uniform float uSwirl;     // extra rotation that grows toward the centre
uniform float uRipple;    // strength of the radial waves
uniform float uTime;
uniform float uFade;
uniform float uHueShift;

out vec4 fragColor;

vec3 rotateHue(vec3 c, float angle) {
    const vec3 k = vec3(0.57735);   // grey axis
    float ca = cos(angle);
    return c * ca + cross(k, c) * sin(angle) + k * dot(k, c) * (1.0 - ca);
}

void main() {
    vec2 p = (vUv - uCenter) * uAspect;
    float r = length(p);
    float a = uAngle + uSwirl / (r + 0.15);
    float c = cos(a), s = sin(a);
    p = mat2(c, s, -s, c) * p / uZoom;
    p *= 1.0 + uRipple * sin(r * 28.0 - uTime * 3.0);
    vec2 uv = p / uAspect + uCenter;

    vec3 color = texture(uPrevious, uv).rgb;
    // Whatever would come from outside the frame is black, not a smeared edge.
    vec2 inside = step(vec2(0.0), uv) * step(uv, vec2(1.0));
    color *= inside.x * inside.y;

    color = max(rotateHue(color, uHueShift), 0.0) * uFade;
    fragColor = vec4(color, 1.0);
}
)";

// Copies the trails out: soft-clips bright areas (so overlapping trails
// saturate gracefully instead of blowing out to white) and darkens the corners.
constexpr const char* kPresentFragmentSrc = R"(#version 330 core
in vec2 vUv;

uniform sampler2D uImage;

out vec4 fragColor;

void main() {
    vec3 color = texture(uImage, vUv).rgb;
    color = 1.0 - exp(-color * 1.2);
    vec2 d = vUv - 0.5;
    color *= 1.0 - 0.6 * dot(d, d);
    fragColor = vec4(color, 1.0);
}
)";

constexpr std::size_t kCirclePoints = 128;
constexpr std::size_t kMaxShockwaves = 10;
constexpr std::size_t kMaxSparks = 400;

float approach(float current, float target, float k) { return current + (target - current) * k; }

}  // namespace

FeedbackTunnel::FeedbackTunnel() : rng_(std::random_device{}()) {
    // Open with everything on so the full effect is visible straight away.
    target_ = Look{1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    look_ = target_;
    sceneTimer_ = 16.0f;
}

bool FeedbackTunnel::init(int width, int height) {
    if (!warp_.build(kFullscreenVertexSrc, kWarpFragmentSrc)) return false;
    if (!present_.build(kFullscreenVertexSrc, kPresentFragmentSrc)) return false;
    for (GlowLine* line : {&monoRing_, &leftRing_, &rightRing_, &shockRing_})
        if (!line->init()) return false;
    if (!sparkSprites_.init()) return false;

    points_.reserve(kRingPoints);
    colors_.reserve(kRingPoints);
    resize(width, height);
    return history_[0].texture() != 0;
}

void FeedbackTunnel::resize(int width, int height) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
    for (auto& target : history_)
        target.create(width_, height_, GL_RGBA16F);
    shockwaves_.clear();
    sparks_.clear();
}

// ---------------------------------------------------------------------------
// Scenes
// ---------------------------------------------------------------------------

FeedbackTunnel::Look FeedbackTunnel::pickScene() {
    std::bernoulli_distribution coin(0.5);
    auto same = [](const Look& a, const Look& b) {
        return a.rainbow == b.rainbow && a.stereo == b.stereo && a.shockwaves == b.shockwaves &&
               a.winding == b.winding && a.flower == b.flower;
    };

    Look next;
    do {
        next.rainbow = coin(rng_) ? 1.0f : 0.0f;
        next.stereo = coin(rng_) ? 1.0f : 0.0f;
        next.shockwaves = coin(rng_) ? 1.0f : 0.0f;
        next.winding = coin(rng_) ? 1.0f : 0.0f;
        next.flower = coin(rng_) ? 1.0f : 0.0f;
    } while (same(next, target_) || next.rainbow + next.stereo + next.shockwaves + next.winding + next.flower == 0.0f);
    return next;
}

void FeedbackTunnel::updateScene(float dt) {
    sceneTimer_ -= dt;
    if (sceneTimer_ <= 0.0f) {
        target_ = pickScene();
        sceneTimer_ = std::uniform_real_distribution<float>(12.0f, 20.0f)(rng_);
    }

    // Features fade in and out over roughly two seconds.
    const float k = 1.0f - std::exp(-dt / 0.8f);
    look_.rainbow = approach(look_.rainbow, target_.rainbow, k);
    look_.stereo = approach(look_.stereo, target_.stereo, k);
    look_.shockwaves = approach(look_.shockwaves, target_.shockwaves, k);
    look_.winding = approach(look_.winding, target_.winding, k);
    look_.flower = approach(look_.flower, target_.flower, k);
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void FeedbackTunnel::update(const AudioFeatures& features, float dt) {
    left_ = features.waveformLeft;
    right_ = features.waveformRight;
    for (std::size_t i = 0; i < mono_.size(); ++i) mono_[i] = 0.5f * (left_[i] + right_[i]);
    bands_ = features.bands;
    bass_ = features.bass;
    mid_ = features.mid;
    level_ = features.level;
    beatPulse_ = features.beatPulse;
    time_ += dt;
    dt_ = dt;
    spin_ += dt * (0.2f + 0.8f * mid_);
    windPhase_ += dt * (0.5f + 0.8f * mid_);

    updateScene(dt);
    if (features.beat) spawnBeatEffects();
    updateParticles(dt);
}

float FeedbackTunnel::ringRadius() const {
    return static_cast<float>(height_) * (0.12f + 0.06f * bass_ + 0.04f * beatPulse_);
}

Vec2 FeedbackTunnel::ringCenter() const {
    const float drift = look_.winding;
    return {static_cast<float>(width_) * (0.5f + drift * 0.12f * std::sin(windPhase_ * 0.61f)),
            static_cast<float>(height_) * (0.5f + drift * 0.10f * std::sin(windPhase_ * 0.43f + 1.3f))};
}

void FeedbackTunnel::spawnBeatEffects() {
    if (look_.shockwaves < 0.3f) return;

    const Vec2 c = ringCenter();
    const float h = static_cast<float>(height_);
    const float r = ringRadius();
    const float hue = time_ * 0.05f;

    if (shockwaves_.size() < kMaxShockwaves)
        shockwaves_.push_back({c.x, c.y, r, h * (0.55f + 0.5f * bass_), 1.0f, hue});

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const int count = static_cast<int>(18.0f + 30.0f * bass_);
    const float scale = h / 720.0f;
    for (int i = 0; i < count && sparks_.size() < kMaxSparks; ++i) {
        const float angle = unit(rng_) * kTwoPi;
        const float dx = std::cos(angle), dy = std::sin(angle);
        const float speed = h * (0.35f + 0.6f * unit(rng_));
        const float sideways = speed * 0.2f * (unit(rng_) * 2.0f - 1.0f);
        const float life = 0.6f + 0.7f * unit(rng_);
        sparks_.push_back({c.x + dx * r, c.y + dy * r,
                           dx * speed - dy * sideways, dy * speed + dx * sideways,
                           life, life, (3.0f + 5.0f * unit(rng_)) * scale, hue + 0.15f * unit(rng_)});
    }
}

void FeedbackTunnel::updateParticles(float dt) {
    const float diagonal = std::hypot(static_cast<float>(width_), static_cast<float>(height_));

    for (auto& wave : shockwaves_) {
        wave.radius += wave.speed * dt;
        wave.life -= dt * 0.8f;
    }
    std::erase_if(shockwaves_, [&](const Shockwave& w) { return w.life <= 0.0f || w.radius > diagonal; });

    const float drag = std::exp(-1.5f * dt);
    for (auto& s : sparks_) {
        s.x += s.vx * dt;
        s.y += s.vy * dt;
        s.vx *= drag;
        s.vy *= drag;
        s.life -= dt;
    }
    std::erase_if(sparks_, [](const Spark& s) { return s.life <= 0.0f; });
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

void FeedbackTunnel::buildRing(GlowLine& line, const float* samples, float angleOffset, float radiusScale, float hueOffset) {
    const auto values = waveLoop<kRingPoints>(samples, AudioFeatures::kWaveformSize, 6);
    const Vec2 c = ringCenter();
    const float h = static_cast<float>(height_);
    const float base = ringRadius() * radiusScale;
    const float wobble = h * 0.10f * (1.0f - 0.4f * look_.flower);
    const float baseHue = time_ * 0.05f + hueOffset;
    const Rgb single = hsv(baseHue, 0.7f, 1.0f);
    constexpr float lastBand = static_cast<float>(AudioFeatures::kBandCount - 1);

    points_.clear();
    colors_.clear();
    for (std::size_t i = 0; i < kRingPoints; ++i) {
        const float frac = static_cast<float>(i) / static_cast<float>(kRingPoints);
        const float angle = angleOffset + kTwoPi * frac;

        // Flower: treble at the seam, bass opposite, mirrored so both halves match.
        const float bandPos = std::abs(2.0f * frac - 1.0f) * lastBand;
        const auto band = static_cast<std::size_t>(bandPos);
        const float energy = std::lerp(bands_[band], bands_[std::min<std::size_t>(band + 1, AudioFeatures::kBandCount - 1)],
                                       bandPos - static_cast<float>(band));
        const float petal = look_.flower * h * 0.18f * (energy - 0.35f);

        const float r = std::max(base + petal + values[i] * wobble, h * 0.02f);
        points_.push_back({c.x + std::cos(angle) * r, c.y + std::sin(angle) * r});
        colors_.push_back(mix(single, hsv(baseHue + frac, 0.8f, 1.0f), look_.rainbow));
    }
    line.setPoints(points_, true, &colors_);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void FeedbackTunnel::drawRings(Layer layer) {
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const float scale = h / 720.0f;
    const float energy = std::min(level_ * 3.0f, 1.0f);
    const float white[3] = {1.0f, 1.0f, 1.0f};

    auto draw = [&](GlowLine& line, float weight) {
        if (weight < 0.02f) return;
        if (layer == Layer::History) {
            // Dim copy: what the trails are made of.
            line.draw(w, h, (6.0f + 8.0f * energy) * scale, 1.5f, white,
                      (0.10f + 0.12f * energy + 0.15f * beatPulse_) * weight);
        } else {
            // Kept narrow: a glow wider than the ring's tightest bends folds
            // over itself and the overlap shows up as bright spikes. The
            // trails behind already provide the soft halo.
            line.draw(w, h, (5.0f + 5.0f * energy) * scale, 2.0f, white,
                      (0.35f + 0.35f * energy + 0.3f * beatPulse_) * weight);
            line.draw(w, h, 2.0f * scale, 0.6f, white, 0.9f * weight);
        }
    };

    draw(monoRing_, 1.0f - look_.stereo);
    draw(leftRing_, look_.stereo);
    draw(rightRing_, look_.stereo);
}

void FeedbackTunnel::drawShockwaves(float alphaScale) {
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const float scale = h / 720.0f;

    for (const auto& wave : shockwaves_) {
        points_.clear();
        for (std::size_t i = 0; i < kCirclePoints; ++i) {
            const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(kCirclePoints);
            points_.push_back({wave.cx + std::cos(angle) * wave.radius, wave.cy + std::sin(angle) * wave.radius});
        }
        shockRing_.setPoints(points_, true);

        float color[3];
        hsvToRgb(wave.hue, 0.5f, 1.0f, color);
        const float alpha = wave.life * wave.life * alphaScale;
        shockRing_.draw(w, h, 12.0f * scale, 2.5f, color, 0.35f * alpha);
        shockRing_.draw(w, h, 1.6f * scale, 0.6f, color, 0.8f * alpha);
    }
}

void FeedbackTunnel::drawSparks(float alphaScale) {
    sprites_.clear();
    for (const auto& s : sparks_) {
        const Rgb c = hsv(s.hue, 0.45f, 1.0f);
        const float fade = s.life / s.maxLife;
        sprites_.push_back({s.x, s.y, s.size * (0.6f + 0.4f * fade), c.r, c.g, c.b, fade * alphaScale});
    }
    sparkSprites_.draw(sprites_, static_cast<float>(width_), static_cast<float>(height_));
}

void FeedbackTunnel::render() {
    if (!history_[0].texture()) return;

    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const RenderTarget& previous = history_[current_];
    const RenderTarget& next = history_[1 - current_];

    // Per-frame amounts are given for 60 fps and scaled by real frame time,
    // so the tunnel moves at the same speed on any monitor.
    const float frames = dt_ * 60.0f;
    const float zoom = std::pow(1.022f + 0.04f * beatPulse_ + 0.012f * bass_, frames);
    const float angle = (0.004f + 0.01f * mid_) * frames;
    const float fade = std::pow(0.925f, frames);
    const float hueShift = 0.012f * frames;
    const float swirl = look_.winding * 0.0025f * frames;
    const float ripple = look_.winding * 0.003f * (0.5f + mid_) * frames;
    const Vec2 center = ringCenter();

    if (look_.stereo < 0.98f) buildRing(monoRing_, mono_.data(), spin_, 1.0f, 0.0f);
    if (look_.stereo > 0.02f) {
        buildRing(leftRing_, left_.data(), spin_, 1.0f, 0.0f);
        buildRing(rightRing_, right_.data(), -1.3f * spin_ + std::numbers::pi_v<float>, 1.08f, 0.33f);
    }

    {
        FramebufferScope scope;  // restores the caller's target when done

        next.bind();
        glDisable(GL_BLEND);
        warp_.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, previous.texture());
        glUniform1i(warp_.uniform("uPrevious"), 0);
        glUniform2f(warp_.uniform("uAspect"), w / h, 1.0f);
        glUniform2f(warp_.uniform("uCenter"), center.x / w, center.y / h);
        glUniform1f(warp_.uniform("uZoom"), zoom);
        glUniform1f(warp_.uniform("uAngle"), angle);
        glUniform1f(warp_.uniform("uSwirl"), swirl);
        glUniform1f(warp_.uniform("uRipple"), ripple);
        glUniform1f(warp_.uniform("uTime"), time_);
        glUniform1f(warp_.uniform("uFade"), fade);
        glUniform1f(warp_.uniform("uHueShift"), hueShift);
        drawFullscreen();

        // Dim copies go into the history; they become the trails.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        drawRings(Layer::History);
        drawShockwaves(0.4f);
        drawSparks(0.6f);
        glDisable(GL_BLEND);
    }

    // Trails first, added onto the background the caller already cleared...
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    present_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, next.texture());
    glUniform1i(present_.uniform("uImage"), 0);
    drawFullscreen();

    // ...then the current shapes on top, crisp and bright.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    drawRings(Layer::Screen);
    drawShockwaves(1.0f);
    drawSparks(1.0f);
    glDisable(GL_BLEND);

    current_ = 1 - current_;
}
