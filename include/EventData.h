#pragma once
#ifndef EVENT_DATA_H
#define EVENT_DATA_H

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "MatrixStack.h"
#include "Program.h"
#include "BPMaterial.h"
#include "Mesh.h"
#include <dv-processing/io/mono_camera_recording.hpp>

/*
    We can treat the data processed from dv-processing as particles in 3D space
    where the x and y coordinates are the position in the image plane and the z
    coordinate is the timestamp.

    Because timestamp is an int64_t, we should acknowledge possible truncation when
    converting to float.
*/
class EventData {
    public:
        EventData();
        ~EventData();

        void initParticlesFromFile(const std::string &filename, size_t mod_freq=100);

        void drawBoundingBoxWireframe(MatrixStack &MV, MatrixStack &P, Program &prog, float particleScale);
        void draw(MatrixStack &MV, MatrixStack &P, Program &prog,
            float particleScale, int focused_evt,
            const glm::vec3 &lightPos, const glm::vec3 &lightColor,
            const BPMaterial &lightMat, const Mesh &meshSphere, 
            const Mesh &meshCube);

        const glm::vec3 &getCenter() const { return center; }
        const float &getMaxTimestamp() const { return max_XYZ.z; }
        const float &getMinTimestamp() const { return min_XYZ.z; }
        float &getTimeWindow_L() { return timeWindow_L; }
        float &getTimeWindow_R() { return timeWindow_R; }

    private:
        std::vector< std::vector<glm::vec3> > particleBatches;
        // Because we pad, we need to store the range of usable particles
        std::vector<size_t> particleSizes;

        long long initTimestamp;
        long long lastTimestamp;

        float timeWindow_L;
        float timeWindow_R;
            
        // We can define a bounding box and thus center to rotate around
        glm::vec3 min_XYZ;
        glm::vec3 max_XYZ;
        glm::vec3 center;
};

#endif // EVENT_DATA_H