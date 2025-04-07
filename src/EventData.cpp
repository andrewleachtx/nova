#include "EventData.h"
#include "utils.h"

#include <algorithm>
#include <cstdio>
#include <dv-processing/core/utils.hpp>

using std::vector;

// TODO ask about default -1 (if you mean timewindow, we know time >=0 so -1.0 indicates it hasn't been
// initialized for later if statements if needed). also, my GitLens sees "--global", see below
// https://docs.github.com/en/get-started/git-basics/setting-your-username-in-git
EventData::EventData() : earliestTimestamp(0), latestTimestamp(0), timeWindow_L(0.0f), timeWindow_R(0.0f),
    minXYZ(std::numeric_limits<float>::max()), maxXYZ(std::numeric_limits<float>::lowest()),
    center(glm::vec3(0.0f)), mod_freq(1), frameLength(0), morlet(false), pca(false) {}
EventData::~EventData() {}

void EventData::reset() {
    // TODO: Do we want to free the memory? Because if we go from like 100'000 particles -> 10 we should. Otherwise, better to keep
    evtParticles.clear();
    earliestTimestamp = 0;
    latestTimestamp = 0;
    minXYZ = glm::vec3(std::numeric_limits<float>::max());
    maxXYZ = glm::vec3(std::numeric_limits<float>::lowest());
    center = glm::vec3(0.0f);
    timeWindow_L = -1.0f;
    timeWindow_R = -1.0f;
    frameLength = 0;
    spaceWindow = glm::vec4(0.0f);
}

void EventData::initParticlesFromFile(const std::string &filename, size_t freq) {
    dv::io::MonoCameraRecording reader(filename);
    this->mod_freq = freq;

    // If someone calls init again, we should always reset
    reset();

    // https://dv-processing.inivation.com/rel_1_7/reading_data.html#read-events-from-a-file
    while (reader.isRunning()) {
        if (const auto events = reader.getNextEventBatch(); events.has_value()) {
            for (auto &evt : events.value()) {
                long long evtTimestamp = evt.timestamp();
                if (evtParticles.empty()) {
                    earliestTimestamp = evtTimestamp;
                }
                
                // Assumedly the last event batch and event has the latest timestamp, but not sure - so use max(...)
                latestTimestamp = std::max(latestTimestamp, evtTimestamp);

                // We can sort of "normalize" the timestamp to start at 0 this way.
                float relativeTimestamp = static_cast<float>(evtTimestamp - earliestTimestamp);
                glm::vec4 evt_xytp = glm::vec4(
                    static_cast<float>(evt.x()),
                    static_cast<float>(evt.y()),
                    relativeTimestamp,
                    static_cast<float>(evt.polarity()) // (float)true == 1.0f, (float)false == 0.0f
                );

                evtParticles.push_back(evt_xytp);

                // glm::min/max does componentwise; .x = min(.x, candidate_x), .y = min(.y, candidate_y), ... 
                minXYZ = glm::min(minXYZ, glm::vec3(evt_xytp));
                maxXYZ = glm::max(maxXYZ, glm::vec3(evt_xytp));
            }
        }
    }

    // TODO: This is arbitrary, we can should define as a constant somewhere
    // Apply scale
    static const float diff_scale = 500.0f / static_cast<float>(latestTimestamp - earliestTimestamp);
    for (auto &evt : evtParticles) {
        evt.z *= diff_scale;
    }

    // Normalize the timestamp of the min/max XYZ for bounding box
    this->minXYZ.z *= diff_scale;
    this->maxXYZ.z *= diff_scale;
    this->center = 0.5f * (minXYZ + maxXYZ);

    this->spaceWindow = glm::vec4(minXYZ.y, maxXYZ.x, maxXYZ.y, minXYZ.x);

    printf("Loaded %zu particles from %s\n", evtParticles.size(), filename.c_str());
}

// TODO: Move precalculable things to an init
void EventData::drawBoundingBoxWireframe(MatrixStack &MV, MatrixStack &P, Program &prog, float particleScale) {
    const glm::vec3 &scaled_minXYZ = minXYZ; 
    const glm::vec3 &scaled_maxXYZ = maxXYZ;
    
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
        // Starting XYZ
        line_posbuf.push_back(corners[edges[i][0]].x);
        line_posbuf.push_back(corners[edges[i][0]].y);
        line_posbuf.push_back(corners[edges[i][0]].z);
        
        // Ending XYZ
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
    glDrawArrays(GL_LINES, 0, (GLsizei)line_posbuf.size() / 3);
    
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
        for (size_t i = 0; i < evtParticles.size(); i++) {
            if (i % this->mod_freq == 0) {
                MV.pushMatrix();
                    MV.translate(evtParticles[i]);
                    MV.scale(particleScale);

                    glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f);
                    // apply tint if t not in [timeWindow_L, timeWindow_R]
                    if (evtParticles[i].z < timeWindow_L || evtParticles[i].z > timeWindow_R) {
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

// FIXME: This is a quick function that I think is only called in this source file, should be inline static
// FIXME: I see f = 125; - be consistent if you are going to use the .0f or not. Otherwise compiler defaults to double.
inline static float contribution(float t, float center_t) {
    float f = 125.0f;
    float h = 4.0f / (1.0f * f);
    // printf("%f \n", h);
    t = t - center_t / 2.0f;
    auto complex_result = std::exp(2.0f * std::complex<float>(0.0f, 1.0f) * std::acos(-1.0f) * f *  t) * 
        (float) std::exp((-4.0f * std::log(2.0f) * std::pow(t, 2.0f)) / std::pow(h, 2.0f));
    complex_result *= 0.1f; // TODO contribution
    return std::real(complex_result) + std::imag(complex_result);
}

void EventData::drawFrame(Program &prog, std::vector<vec3> &eigenvectors) {
    prog.bind();

    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO); 
    glBindVertexArray(VAO); 

    glGenBuffers(1, &VBO); 
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

    float min_x = this->minXYZ.x;
    float max_x = this->maxXYZ.x;
    float min_y = this->minXYZ.y;
    float max_y = this->maxXYZ.y;
    glm::mat4 projection = glm::ortho(min_x, max_x, min_y, max_y);
    glUniformMatrix4fv(prog.getUniform("projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glm::vec2 rolling_sum(0.0f, 0.0f);
    std::vector<float> total;
    // FIXME: I changed this to work with the 1D array.
    for (size_t i = 0; i < evtParticles.size(); i++) {
        // FIXME: This is also a neat way to unpack things of the same type, use multi declaration where you want.
        float x(evtParticles[i].x), y(evtParticles[i].y), t(evtParticles[i].z);
        float polarity(evtParticles[i].w);
        // float x = evtParticles[i].x;
        // float y = evtParticles[i].y;
        // float t = evtParticles[i].z;
        // float polarity = evtParticles[i].w;

        // FIXME: You don't need getters within class
        if (t <= getTimeWindow_R() && t >= getTimeWindow_L()) {
            // x == top, y == right, z == bottom, w == left
            if (x >= getSpaceWindow().w && x <= getSpaceWindow().y && y >= getSpaceWindow().x && y <= getSpaceWindow().z) {
                total.push_back(x);
                total.push_back(y);

                float weight = morlet ? contribution(t, getTimeWindow_R() - getTimeWindow_L() * polarity) : 1.0f; 
                total.push_back(weight);

                rolling_sum.x += x;
                rolling_sum.y += y;
            }
        }
        else { // Assumes strictly increasing
            break;
        }
    }

    // Calculate covariance // TODO improve normalization
    // FIXME: Division is very costly (slow) on a per-frame scale, make sure to use it only when you must
    // otherwise if the divisor is constant, store inverse (1 / divisor) in the class and do a * inverse
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

    // Eigen vectors
    eigenvectors.push_back(glm::vec3(eigen1 - cov_y_y, cov_x_y, 1));
    eigenvectors.push_back(glm::vec3(eigen2 - cov_y_y, cov_x_y, 1));

    // Load data points
    glBufferData(GL_ARRAY_BUFFER, total.size() * sizeof(float), total.data(), GL_DYNAMIC_DRAW);

    // Has to be in this order for some reason IDK
    int pos = prog.getAttribute("pos");
	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);
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
