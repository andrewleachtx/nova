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

        void reset();
        void initInstancing(Program &instancingProg);
        void initParticlesFromFile(const std::string &filename); // TODO speedup and dynamic

        void drawBoundingBoxWireframe(MatrixStack &MV, MatrixStack &P, Program &prog);
        void draw(MatrixStack &MV, MatrixStack &P, Program &prog,
            float particleScale, const glm::vec3 &lightPos, const glm::vec3 &lightColor,
            const BPMaterial &lightMat, const Mesh &meshSphere);
        void drawInstanced(MatrixStack &MV, MatrixStack &P, Program &prog,
            float particleScale, const glm::vec3 &lightPos, const glm::vec3 &lightColor,
            const BPMaterial &lightMat, const Mesh &meshSphere);
        void drawFrame(Program &prog, std::vector<glm::vec3> &eigenvectors);

        const glm::vec3 &getCenter() const { return center; }
        const glm::vec3 getMin_XYZ() const { return minXYZ; } // TOOD maybe manipulate window instead
        const glm::vec3 getMax_XYZ() const { return maxXYZ; }
        const float &getMaxTimestamp() const { return maxXYZ.z; }
        const float &getMinTimestamp() const { return minXYZ.z; }
        float &getTimeWindow_L() { return timeWindow_L; }
        float &getTimeWindow_R() { return timeWindow_R; }
        float &getFrameLength() { return frameLength; }
        glm::vec4 &getSpaceWindow() { return spaceWindow; }
        bool &getMorlet() { return morlet; }
        bool &getPCA() { return pca; }

    private:
        // TODO: Might be better to just store a std::bitset for polarity, and something dynamic like a color
        // indicator for a (although we would need a vec3 for a full RGB)
        std::vector<glm::vec4> evtParticles; // x, y, t, polarity (false=0.0, true=1.0)
        size_t mod_freq; // only draw the mod_freq'th particle of the ones we read in

        long long earliestTimestamp;
        long long latestTimestamp;

        float timeWindow_L;
        float timeWindow_R;
        float frameLength;

        glm::vec4 spaceWindow; // x = top, y = right, z = bottom, w = left 
        
        // FIXME: Would be helpful to say doMorlet or doPCA to indicate booleanness
        bool morlet;
        bool pca;

        // We can define a bounding box and thus center to rotate around
        glm::vec3 minXYZ;
        glm::vec3 maxXYZ;
        glm::vec3 center;

        // Instancing
        GLuint instVBO;
};

#endif // EVENT_DATA_H