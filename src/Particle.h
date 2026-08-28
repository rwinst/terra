#pragma once
#include "Color.h"
#include <vector>

struct Particle {
    float x, y;
    float vx, vy;
    float life;    // seconds remaining
    float maxLife;
    Color color;
    float size;
};

class ParticleSystem {
public:
    void spawnBurst(float x, float y, Color color, int count);
    void update(float dt);
    const std::vector<Particle>& particles() const { return list; }

private:
    std::vector<Particle> list;
};
