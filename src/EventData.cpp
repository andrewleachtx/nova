#include "EventData.h"
#include "utils.h"

#include <algorithm>
#include <cstdio>
#include <dv-processing/core/utils.hpp>

EventData::EventData() : initTimestamp(0), lastTimestamp(0), timeWindow_L(-1.0f), timeWindow_R(-1.0f) {}
EventData::~EventData() {}

void EventData::initParticlesFromFile(const std::string &filename, size_t mod_freq) {
    dv::io::MonoCameraRecording reader(filename);

    size_t max_batchSz = 0;
    particles.clear();
    particleSizes.clear();
    initTimestamp = 0;
    lastTimestamp = 0;

    // https://dv-processing.inivation.com/rel_1_7/reading_data.html#read-events-from-a-file
    while (reader.isRunning()) {
        if (const auto events = reader.getNextEventBatch(); events.has_value()) {
            std::vector<glm::vec3> evtBatch;
            for (size_t i = 0; i < events.value().size(); i++) {
                const auto &event = events.value()[i];
                
                if (particles.empty() && evtBatch.empty()) {
                    initTimestamp = event.timestamp();
                }

                lastTimestamp = std::max(lastTimestamp, event.timestamp());

                if (i % mod_freq == 0) {
                    float relativeTimestamp = static_cast<float>(event.timestamp() - initTimestamp);
                    evtBatch.push_back(glm::vec3(static_cast<float>(event.x()),
                                                   static_cast<float>(event.y()),
                                                   relativeTimestamp));
                }
            }

            max_batchSz = std::max(max_batchSz, evtBatch.size());
            particles.push_back(evtBatch);
            particleSizes.push_back(evtBatch.size());
        }
    }

    printf("Loaded %zu event batches\n", particles.size());
    printf("Biggest batch size: %zu\n", max_batchSz);

    // Each particle is grouped by event s.t. particle[i] stores a vector of glm::vec3 with a normalized abs. timestamp
    for (size_t i = 0; i < particles.size(); i++) {
        particles[i].resize(max_batchSz, glm::vec3(0.0f));
        for (size_t j = 0; j < particleSizes[i]; j++) {
            particles[i][j].z = particles[i][j].z / static_cast<float>(lastTimestamp - initTimestamp);
        }
    }
}

void EventData::draw(MatrixStack &MV, MatrixStack &P, Program &prog,
    float particleScale, int focused_evt,
    const glm::vec3 &lightPos, const glm::vec3 &lightColor,
    const BPMaterial &lightMat, const Mesh &g_meshSphere) const {

    prog.bind();
    MV.pushMatrix();
    for (size_t i = 0; i < particles.size(); i++) {
        for (size_t j = 0; j < particleSizes[i]; j++) {
            MV.pushMatrix();
            MV.translate(particles[i][j]);
            MV.scale(particleScale);

            // glm::vec3 color = getTimeColor(particles[i][j].z);
            glm::vec3 color = glm::vec3(1.0f, 0.0f, 0.0f);

            // if (focused_evt != -1 && focused_evt != static_cast<int>(i)) {
            //     color *= 0.15f;
            // }
            sendToPhongShader(const_cast<Program&>(prog), P, MV, lightPos, color, lightMat);
            g_meshSphere.draw(const_cast<Program&>(prog));
            MV.popMatrix();
        }
    }
    MV.popMatrix();
    prog.unbind();
}