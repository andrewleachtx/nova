#include "EventData.h"
#include "utils.h"

#include <algorithm>
#include <cstdio>
#include <dv-processing/core/utils.hpp>

// TODO ask about default -1
EventData::EventData() : initTimestamp(0), lastTimestamp(0), timeWindow_L(-1.0f), timeWindow_R(-1.0f),
    min_XYZ(std::numeric_limits<float>::max()), max_XYZ(std::numeric_limits<float>::lowest()),
    center(glm::vec3(0.0f)), mod_freq(1), frameLength(0) {}
EventData::~EventData() {}

void EventData::initParticlesFromFile(const std::string &filename, size_t freq) {
    dv::io::MonoCameraRecording reader(filename);
    this->mod_freq = freq;

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
                // if (i % freq != 0 && false) { continue; } TODO speed up
                const auto &event = events.value()[i];
                
                if (particleBatches.empty() && evtBatch.empty()) {
                    initTimestamp = event.timestamp();
                }

                lastTimestamp = std::max(lastTimestamp, event.timestamp());

                float relativeTimestamp = static_cast<float>(event.timestamp() - initTimestamp);
                glm::vec3 event_xyz = glm::vec3(static_cast<float>(event.x()), static_cast<float>(event.y()), relativeTimestamp);
                evtBatch.push_back(event_xyz);

                // glm::min/max are beautiful functions. Guarantees each component is min/max'd always
                raw_minXYZ = glm::min(raw_minXYZ, event_xyz);
                raw_maxXYZ = glm::max(raw_maxXYZ, event_xyz);
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

    this->spaceWindow = glm::vec4(min_XYZ.y, max_XYZ.x, max_XYZ.y, min_XYZ.x);

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
                if (j % this->mod_freq == 0) {

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
        }
        
        drawBoundingBoxWireframe(MV, P, prog, particleScale);
    MV.popMatrix();
    prog.unbind();
}

// TODO use glm
float min(float a, float b) {
    if (a <= b) {
        return a;
    }
    return b;
}
float max(float a, float b) {
    if (a >= b) {
        return a;
    }
    return b;
}

void EventData::drawFrame(Program &prog, std::vector<vec3> &eigenvectors) {

    prog.bind();

    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO); 
    glBindVertexArray(VAO); 

    glGenBuffers(1, &VBO); 
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // TODO get rid of this somehow (either just perform once somewhere or get the resolution size from the camera)
    float min_x = particleBatches[0][0].x;
    float max_x = particleBatches[0][0].x;
    float min_y = particleBatches[0][0].y;
    float max_y = particleBatches[0][0].y;
    float min_z = particleBatches[0][0].z;
    float max_z = particleBatches[0][0].z;
    for (size_t i = 0; i < particleBatches.size(); i++) {
        for (size_t j = 0; j < particleSizes[i]; j++) {
            min_x = min(min_x, particleBatches[i][j].x);
            max_x = max(max_x, particleBatches[i][j].x);
            min_y = min(min_y, particleBatches[i][j].y);
            max_y = max(max_y, particleBatches[i][j].y);
            min_z = min(min_z, particleBatches[i][j].z);
            max_z = max(max_z, particleBatches[i][j].z);
        }
    }


    glm::mat4 projection = glm::ortho(min_x, max_x, min_y, max_y);
    glUniformMatrix4fv(prog.getUniform("projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glm::vec2 rolling_sum(0.0f, 0.0f);
    std::vector<float> total;
    for (size_t i = 0; i < particleBatches.size(); ++i) {
        for (size_t j = 0; j < particleSizes[i]; ++j) {
            float x = particleBatches[i][j].x;
            float y = particleBatches[i][j].y;
            float t = particleBatches[i][j].z;

            if (t <= getTimeWindow_R() && t >= getTimeWindow_L()) {

                // x == top, y == right, z == bottom, w == left
                if (x >= getSpaceWindow().w && x <= getSpaceWindow().y && y >= getSpaceWindow().x && y <= getSpaceWindow().z) {
                    total.push_back(x);
                    total.push_back(y);

                    rolling_sum.x += x;
                    rolling_sum.y += y;
                }
            }
        }
    }

    // Calculate covariance // TODO improve normalization
    float mean_x = rolling_sum.x / max_x / total.size() / 2;
    float mean_y = rolling_sum.y / max_y / total.size() / 2;

    float cov_x_y = 0;
    float cov_x_x = 0;
    float cov_y_y = 0;
    for (size_t i = 0; i < total.size(); i += 2) {
        float x = total[i];
        float y = total[i + 1];

        cov_x_y += (x / max_x - mean_x) * (y / max_y - mean_y);
        cov_x_x += (x / max_x - mean_x) * (x / max_x - mean_x);
        cov_y_y += (y / max_y - mean_y) * (y / max_y - mean_y);
    }

    cov_x_y /= (total.size() / 2 - 1);
    cov_x_x /= (total.size() / 2 - 1);
    cov_y_y /= (total.size() / 2 - 1);

    // Matrix
    float a = 1;
    float b = -(cov_x_x + cov_y_y);
    float c = cov_x_x * cov_y_y - cov_x_y * cov_x_y;

    float eigen1 = (-b + std::sqrt(b*b - 4 * a * c)) / (2 * a);
    float eigen2 = (-b - std::sqrt(b*b - 4 * a * c)) / (2 * a);

    // Eigen vector 1
    eigenvectors.push_back(glm::vec3(eigen1 - cov_y_y, cov_x_y, 1));

    // Eigen vector 2
    eigenvectors.push_back(glm::vec3(eigen2 - cov_y_y, cov_x_y, 1));

    // Load data points
    glBufferData(GL_ARRAY_BUFFER, total.size() * sizeof(float), total.data(), GL_DYNAMIC_DRAW);

    // Has to be in this order for some reason IDK
    int pos = prog.getAttribute("pos");
	glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);
	glEnableVertexAttribArray(pos);

    glPointSize(1.0f); 
    glDrawArrays(GL_POINTS, 0, total.size()); // Probably break up

	// Disable and unbind
	glDisableVertexAttribArray(pos);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glBindVertexArray(0);
	
    glDeleteBuffers(1, &VBO); // TODO make permanent
    glDeleteBuffers(1, &VAO);

	GLSL::checkError(GET_FILE_LINE);


    prog.unbind();

}
