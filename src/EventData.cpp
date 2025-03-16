#include "EventData.h"
#include "utils.h"

#include <algorithm>
#include <cstdio>
#include <dv-processing/core/utils.hpp>

EventData::EventData() : initTimestamp(0), lastTimestamp(0), timeWindow_L(-1.0f), timeWindow_R(-1.0f),
    min_XYZ(std::numeric_limits<float>::max()), max_XYZ(std::numeric_limits<float>::lowest()) {}
EventData::~EventData() {}

void EventData::initParticlesFromFile(const std::string &filename, size_t mod_freq) {
    dv::io::MonoCameraRecording reader(filename);

    // TODO: Should just write a reset method using this and call it for sanity check
    size_t max_batchSz = 0;
    particleBatches.clear();
    particleSizes.clear();
    initTimestamp = 0;
    lastTimestamp = 0;
    glm::vec3 raw_minXYZ = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 raw_maxXYZ = glm::vec3(std::numeric_limits<float>::lowest());

    // https://dv-processing.inivation.com/rel_1_7/reading_data.html#read-events-from-a-file
    while (reader.isRunning()) {
        if (const auto events = reader.getNextEventBatch(); events.has_value()) {
            std::vector<glm::vec3> evtBatch;
            for (size_t i = 0; i < events.value().size(); i++) {
                const auto &event = events.value()[i];
                
                if (particleBatches.empty() && evtBatch.empty()) {
                    initTimestamp = event.timestamp();
                }

                lastTimestamp = std::max(lastTimestamp, event.timestamp());

                if (i % mod_freq == 0) {
                    float relativeTimestamp = static_cast<float>(event.timestamp() - initTimestamp);
                    glm::vec3 event_xyz = glm::vec3(static_cast<float>(event.x()), static_cast<float>(event.y()), relativeTimestamp);
                    evtBatch.push_back(event_xyz);

                    // glm::min/max are beautiful functions. Guarantees each component is min/max'd always
                    raw_minXYZ = glm::min(raw_minXYZ, event_xyz);
                    raw_maxXYZ = glm::max(raw_maxXYZ, event_xyz);
                }
            }

            max_batchSz = std::max(max_batchSz, evtBatch.size());
            particleBatches.push_back(evtBatch);
            particleSizes.push_back(evtBatch.size());
        }
    }

    /*
        The center is of course the midpoint. We should store the longest component from the center
        to the planes formed by the box.center
    */
    glm::vec3 center = 0.5f * (raw_minXYZ + raw_maxXYZ);
    float diff_scale = 500.0f / static_cast<float>(lastTimestamp - initTimestamp);

    // Each particle is grouped by event s.t. particle[i] stores a vector of glm::vec3 with a normalized abs. timestamp
    for (size_t i = 0; i < particleBatches.size(); i++) {
        particleBatches[i].resize(max_batchSz, glm::vec3(0.0f));
        for (size_t j = 0; j < particleSizes[i]; j++) {
            particleBatches[i][j].z = particleBatches[i][j].z * diff_scale;
        }
    }

    // Normalize the timestamp of the min/max XYZ
    min_XYZ = raw_minXYZ;
    max_XYZ = raw_maxXYZ;
    min_XYZ.z *= diff_scale;
    max_XYZ.z *= diff_scale;

    printf("Loaded %zu event batches\n", particleBatches.size());
    printf("Biggest batch size: %zu\n", max_batchSz);
    printvec3(min_XYZ);
    printvec3(max_XYZ);
}

// // FIXME: Lazy usage of immediate mode, should add VAO
// void EventData::drawBoundingBox(Program &prog) {
//     glm::vec3 corners[8] = {
//         { min_XYZ.x, min_XYZ.y, min_XYZ.z },
//         { max_XYZ.x, min_XYZ.y, min_XYZ.z },
//         { max_XYZ.x, max_XYZ.y, min_XYZ.z },
//         { min_XYZ.x, max_XYZ.y, min_XYZ.z },
//         { min_XYZ.x, min_XYZ.y, max_XYZ.z },
//         { max_XYZ.x, min_XYZ.y, max_XYZ.z },
//         { max_XYZ.x, max_XYZ.y, max_XYZ.z },
//         { min_XYZ.x, max_XYZ.y, max_XYZ.z }
//     };

//     static int edges[12][2] = {
//         {0,1}, {1,2}, {2,3}, {3,0},
//         {4,5}, {5,6}, {6,7}, {7,4},
//         {0,4}, {1,5}, {2,6}, {3,7}
//     };
// }

void EventData::drawBoundingBox(MatrixStack &MV, MatrixStack &P, Program &prog, float particleScale, const Mesh &meshCube) {
    // Compute scaled corners of the bounding box, accounting for particleScale
    glm::vec3 scaled_minXYZ = min_XYZ * particleScale;
    glm::vec3 scaled_maxXYZ = max_XYZ * particleScale;

    // Compute the center and scale of the bounding box
    glm::vec3 box_center = 0.5f * (scaled_minXYZ + scaled_maxXYZ);
    glm::vec3 box_scale  = scaled_maxXYZ - scaled_minXYZ;

    // Render the bounding box cube
    prog.bind();
    MV.pushMatrix();
        MV.translate(box_center);
        MV.scale(box_scale);

        // Set bounding box color (e.g., semi-transparent white)
        glm::vec3 bbox_color(1.0f, 1.0f, 1.0f);
        sendToPhongShader(prog, P, MV, glm::vec3(0.0f), bbox_color, BPMaterial());

        meshCube.draw(prog);
    MV.popMatrix();
    prog.unbind();
}

void EventData::draw(MatrixStack &MV, MatrixStack &P, Program &prog,
    float particleScale, int focused_evt,
    const glm::vec3 &lightPos, const glm::vec3 &lightColor,
    const BPMaterial &lightMat, const Mesh &meshSphere, 
    const Mesh &meshCube) {

    prog.bind();
    MV.pushMatrix();
        // Translate to center
        // glm::vec3 center = 0.5f * (min_XYZ + max_XYZ);
        // MV.translate(center);

        for (size_t i = 0; i < particleBatches.size(); i++) {
            for (size_t j = 0; j < particleSizes[i]; j++) {
                MV.pushMatrix();
                MV.translate(particleBatches[i][j]);
                MV.scale(particleScale);

                // glm::vec3 color = getTimeColor(particleBatches[i][j].z);
                glm::vec3 color = glm::vec3(1.0f, 0.0f, 0.0f);

                // if (focused_evt != -1 && focused_evt != static_cast<int>(i)) {
                //     color *= 0.15f;
                // }
                sendToPhongShader(prog, P, MV, lightPos, color, lightMat);
                meshSphere.draw(prog);
                MV.popMatrix();
            }
        }
        
        // drawBoundingBox(MV, P, prog, particleScale, meshCube);
        MV.popMatrix();
        prog.unbind();
}