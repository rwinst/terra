#include "Particle.h"
#include <random>
#include <algorithm>

namespace {
    std::mt19937& rng() {
        static std::mt19937 r(std::random_device{}());
        return r;
    }
    float randRange(float lo, float hi) {
        std::uniform_real_distribution<float> d(lo, hi);
        return d(rng());
    }
}

void ParticleSystem::spawnBurst(float x, float y, Color color, int count) {
    for (int i = 0; i < count; i++) {
        Particle p;
        p.x = x;
        p.y = y;
        p.vx = randRange(-90.0f, 90.0f);
        p.vy = randRange(-160.0f, -20.0f);
        p.maxLife = randRange(0.25f, 0.6f);
        p.life = p.maxLife;
        p.color = color;
        p.size = randRange(2.0f, 5.0f);
        list.push_back(p);
    }
}

void ParticleSystem::update(float dt) {
    for (auto& p : list) {
        p.vy += 600.0f * dt; // light gravity, separate from player physics on purpose
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.life -= dt;
    }
    list.erase(std::remove_if(list.begin(), list.end(),
        [](const Particle& p) { return p.life <= 0.0f; }), list.end());
}
