#pragma once
#ifndef MATERIAL_H
#define MATERIAL_H

#include <glm/glm.hpp>
using glm::vec3;

class BPMaterial {
    public:
        vec3 ka, kd, ks;
        float s;

        BPMaterial() : ka(1.0f), kd(1.0f), ks(1.0f), s(1.0f) {}
        BPMaterial(const vec3& ka, const vec3& kd, const vec3& ks, float s) : ka(ka), kd(kd), ks(ks), s(s) {}
};

#endif // MATERIAL_H
