#include "EventData.h"
#include "utils.h"

#include <algorithm>
#include <cstdio>
#include <dv-processing/core/utils.hpp>

EventData::EventData() : initTimestamp(0), lastTimestamp(0), timeWindow_L(-1.0f), timeWindow_R(-1.0f),
    min_XYZ(std::numeric_limits<float>::max()), max_XYZ(std::numeric_limits<float>::lowest()),
    center(glm::vec3(0.0f)) {}
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
    center = 0.5f * (min_XYZ + max_XYZ);

    printf("Loaded %zu event batches\n", particleBatches.size());
    printf("Biggest batch size: %zu\n", max_batchSz);
}

// TODO: Move precalculable things to an init
void EventData::drawBoundingBoxWireframe(MatrixStack &MV, MatrixStack &P, Program &prog, float particleScale) {
    const glm::vec3 &scaled_minXYZ = min_XYZ; 
    const glm::vec3 &scaled_maxXYZ = max_XYZ;
    
    glLineWidth(2.0f);
    
    glm::vec3 corners[8] = {
        { scaled_minXYZ.x, scaled_minXYZ.y, scaled_minXYZ.z },
        { scaled_maxXYZ.x, scaled_minXYZ.y, scaled_minXYZ.z },
        { scaled_maxXYZ.x, scaled_maxXYZ.y, scaled_minXYZ.z },
        { scaled_minXYZ.x, scaled_maxXYZ.y, scaled_minXYZ.z },
        { scaled_minXYZ.x, scaled_minXYZ.y, scaled_maxXYZ.z },
        { scaled_maxXYZ.x, scaled_minXYZ.y, scaled_maxXYZ.z },
        { scaled_maxXYZ.x, scaled_maxXYZ.y, scaled_maxXYZ.z },
        { scaled_minXYZ.x, scaled_maxXYZ.y, scaled_maxXYZ.z }
    };

    int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };

    // Instead of immediate mode use VBOs
    static GLuint lineVBO, lineVAO;
    static bool initialized = false;
    
    if (!initialized) {
        glGenBuffers(1, &lineVBO);
        glGenVertexArrays(1, &lineVAO);
        initialized = true;
    }
    
    // Buffer for x0, y0, z0, x1, y1, ... of each line segment
    static std::vector<float> line_posbuf(12 * 2 * 3);
    for (int i = 0; i < 12; i++) {
        // Start
        line_posbuf.push_back(corners[edges[i][0]].x);
        line_posbuf.push_back(corners[edges[i][0]].y);
        line_posbuf.push_back(corners[edges[i][0]].z);
        
        // End
        line_posbuf.push_back(corners[edges[i][1]].x);
        line_posbuf.push_back(corners[edges[i][1]].y);
        line_posbuf.push_back(corners[edges[i][1]].z);
    }
    
    // Bind VAO and update VBO
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferData(GL_ARRAY_BUFFER, line_posbuf.size() * sizeof(float), line_posbuf.data(), GL_DYNAMIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
    prog.bind();
    MV.pushMatrix();
    
    // TODO: Can change, for now white
    glm::vec3 color_line(1.0f, 1.0f, 1.0f);
    BPMaterial mat_line;
    mat_line.ka = color_line;
    mat_line.kd = color_line;
    mat_line.ks = color_line;
    mat_line.s = 10.0f;
    
    sendToPhongShader(prog, P, MV, glm::vec3(0.0f), color_line, mat_line);
    glDrawArrays(GL_LINES, 0, line_posbuf.size() / 3);
    
    MV.popMatrix();
    prog.unbind();
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glLineWidth(1.0f);
}

void EventData::draw(MatrixStack &MV, MatrixStack &P, Program &prog,
    float particleScale, int focused_evt,
    const glm::vec3 &lightPos, const glm::vec3 &lightColor,
    const BPMaterial &lightMat, const Mesh &meshSphere, 
    const Mesh &meshCube) {

    prog.bind();
    MV.pushMatrix();
        for (size_t i = 0; i < particleBatches.size(); i++) {
            for (size_t j = 0; j < particleSizes[i]; j++) {
                MV.pushMatrix();
                MV.translate(particleBatches[i][j]);
                MV.scale(particleScale);

                glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f);
                // apply tint if t not in [timeWindow_L, timeWindow_R]
                if (particleBatches[i][j].z < timeWindow_L || particleBatches[i][j].z > timeWindow_R) {
                    color = glm::vec3(0.5f, 0.5f, 0.5f);
                }

                sendToPhongShader(prog, P, MV, lightPos, color, lightMat);
                meshSphere.draw(prog);
                MV.popMatrix();
            }
        }
        
        drawBoundingBoxWireframe(MV, P, prog, particleScale);
    MV.popMatrix();
    prog.unbind();
}