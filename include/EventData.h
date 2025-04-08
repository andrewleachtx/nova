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

        void initParticlesFromFile(const std::string &filename, size_t point_freq=10000); // TODO speedup and dynamic

        void drawBoundingBoxWireframe(MatrixStack &MV, MatrixStack &P, Program &prog, float particleScale);
        void draw(MatrixStack &MV, MatrixStack &P, Program &prog,
            float particleScale, int focused_evt,
            const glm::vec3 &lightPos, const glm::vec3 &lightColor,
            const BPMaterial &lightMat, const Mesh &meshSphere, 
            const Mesh &meshCube);
        void drawFrame(Program &prog, glm::vec2 viewport_resolution, 
            bool morlet, float freq, bool pca);
        void normalizeTime();
        void oddizeTime();

        float getTimestamp(uint eventIndex, float oddFactor = 1.0f) const;
        uint getFirstEvent(float timestamp, float normFactor = 1.0f) const;
        uint getLastEvent(float timestamp, float normFactor = 1.0f) const;

        const float getDiffScale() const { return diffScale; }
        const glm::vec3 &getCenter() const { return center; }
        const glm::vec3 getMin_XYZ() const { return min_XYZ; } // TODO maybe manipulate window instead
        const glm::vec3 getMax_XYZ() const { return max_XYZ; }
        const float &getMaxTimestamp() const { return max_XYZ.z; }
        const float &getMinTimestamp() const { return min_XYZ.z; }
        const uint getMaxEvent() const { return totalEvents; }
        float &getTimeWindow_L() { return timeWindow_L; }
        float &getTimeWindow_R() { return timeWindow_R; }
        uint &getEventWindow_L() { return eventWindow_L; }
        uint &getEventWindow_R() { return eventWindow_R; }
        glm::vec4 &getSpaceWindow() { return spaceWindow; }
        float &getTimeShutterWindow_L() { return timeShutterWindow_L; }
        float &getTimeShutterWindow_R() { return timeShutterWindow_R; }
        uint &getEventShutterWindow_L() { return eventShutterWindow_L; }
        uint &getEventShutterWindow_R() { return eventShutterWindow_R; }
        int &getShutterType() { return shutterType; }

        static const int TIME_CONVERSION = 1000; // Could be made setable
        static const int TIME_SHUTTER = 0; // values must match ImGui::Combo order in utils.cpp
        static const int EVENT_SHUTTER = 1;

    private:
        glm::vec2 camera_resolution;
        float diffScale;

        std::vector< std::vector<glm::vec4> > particleBatches; // x, y, t, polarity
        uint totalEvents;
        size_t mod_freq;

        // Because we pad, we need to store the range of usable particles
        std::vector<size_t> particleSizes;

        long long initTimestamp;
        long long lastTimestamp;

        float timeWindow_L;
        float timeWindow_R;
        uint eventWindow_L;
        uint eventWindow_R;

        // Maybe move to FrameScene or own helper struct
        int shutterType;
        float timeShutterWindow_L;
        float timeShutterWindow_R;
        uint eventShutterWindow_L;
        uint eventShutterWindow_R;

        glm::vec4 spaceWindow; // x = top, y = right, z = bottom, w = left 

        // We can define a bounding box and thus center to rotate around
        glm::vec3 min_XYZ;
        glm::vec3 max_XYZ;
        glm::vec3 center;
};

#endif // EVENT_DATA_H