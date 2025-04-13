#include "EventData.h"
#include "utils.h"

#include <algorithm>
#include <cstdio>
#include <dv-processing/core/utils.hpp>

using std::vector, std::cout, std::endl;

// do in order of declaration below
EventData::EventData() : camera_resolution(0.0f), diffScale(0.0f), mod_freq(1),
    earliestTimestamp(0), latestTimestamp(0), shutterType(TIME_SHUTTER), 
    timeWindow_L(0.0f), timeWindow_R(0.0f), eventWindow_L(0), eventWindow_R(0),
    timeShutterWindow_L(0.0f), timeShutterWindow_R(0.0f), eventShutterWindow_L(0),
    eventShutterWindow_R(0), spaceWindow(0.0f), minXYZ(std::numeric_limits<float>::max()),
    maxXYZ(std::numeric_limits<float>::lowest()), center(0.0f) {}

EventData::~EventData() {
    if (instVBO) {
        glDeleteBuffers(1, &instVBO);
        instVBO = 0;
    }
}

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
    spaceWindow = glm::vec4(0.0f);

    if (instVBO) {
        glDeleteBuffers(1, &instVBO);
        instVBO = 0;
    }
}

// https://learnopengl.com/Advanced-OpenGL/Instancing
void EventData::initInstancing(Program &progInst) {
    // Generate / initialize a VBO here. GL_STATIC_DRAW may be better, should test
    genVBO(instVBO, evtParticles.size() * sizeof(glm::vec4), GL_DYNAMIC_DRAW);
    
    glBindBuffer(GL_ARRAY_BUFFER, instVBO);
    GLint aInstPos = progInst.getAttribute("aInstPos");

    // Update the vertex attribute pointer every 1 * sizeof(glm::vec4) bytes
    glEnableVertexAttribArray(aInstPos);
    glVertexAttribPointer(aInstPos, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
    glVertexAttribDivisor(aInstPos, 1); // Update once per instance (not per vertex)
    
    // Pass in the existing data
    glBufferSubData(GL_ARRAY_BUFFER, 0, evtParticles.size() * sizeof(glm::vec4), evtParticles.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void EventData::initParticlesFromFile(const std::string &filename) {
    dv::io::MonoCameraRecording reader(filename);
    // this->mod_freq = freq;

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
    this->diffScale = 5000.0f / static_cast<float>(latestTimestamp - earliestTimestamp);
    for (auto &evt : evtParticles) {
        evt.z *= diffScale;
    }

    // Normalize the timestamp of the min/max XYZ for bounding box
    this->minXYZ.z *= diffScale;
    this->maxXYZ.z *= diffScale;
    this->center = 0.5f * (minXYZ + maxXYZ);
    
    this->spaceWindow = glm::vec4(minXYZ.y, maxXYZ.x, maxXYZ.y, minXYZ.x);

    printf("Loaded %zu particles from %s\n", evtParticles.size(), filename.c_str());
}

// TODO: Move precalculable things to an init
void EventData::drawBoundingBoxWireframe(MatrixStack &MV, MatrixStack &P, Program &progBasic) {
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
    
    progBasic.bind();
    MV.pushMatrix();
    
    // TODO: Can change, for now white
    glm::vec3 color_line(1.0f, 1.0f, 1.0f);
    BPMaterial mat_line;
    mat_line.ka = color_line;
    mat_line.kd = color_line;
    mat_line.ks = color_line;
    mat_line.s = 10.0f;
    
    sendToPhongShader(progBasic, P, MV, glm::vec3(0.0f), color_line, mat_line);
    glDrawArrays(GL_LINES, 0, (GLsizei)line_posbuf.size() / 3);
    
    MV.popMatrix();
    progBasic.unbind();
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glLineWidth(1.0f);
}

/* THIS IS WITHOUT INSTANCING; DEPRECATED */
void EventData::draw(MatrixStack &MV, MatrixStack &P, Program &prog,
    float particleScale, const glm::vec3 &lightPos, const glm::vec3 &lightColor,
    const BPMaterial &lightMat, const Mesh &meshSphere) { 

    prog.bind();
    MV.pushMatrix();
        for (size_t i = 0; i < evtParticles.size(); i++) {
            if (i % this->mod_freq == 0) {
                MV.pushMatrix();
                    MV.translate(evtParticles[i]);
                    MV.scale(particleScale);

                    glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f);
                    // apply tint if t not in [timeWindow_L, timeWindow_R]
                    if (i < eventWindow_L || i > eventWindow_R) {
                        color = glm::vec3(0.5f, 0.5f, 0.5f);
                    }

                    sendToPhongShader(prog, P, MV, lightPos, color, lightMat);
                    meshSphere.draw(prog);
                MV.popMatrix();
            }
        }
        
        drawBoundingBoxWireframe(MV, P, prog);
    MV.popMatrix();
    prog.unbind();
}

void EventData::drawInstanced(MatrixStack &MV, MatrixStack &P, Program &progInst, Program &progBasic,
    float particleScale, const glm::vec3 &lightPos, const glm::vec3 &lightColor,
    const BPMaterial &lightMat, const Mesh &meshSphere) {
    
    if (evtParticles.empty() || mod_freq == 0) {
        return;
    }

    size_t instCt = std::max(1ULL, evtParticles.size() / mod_freq);

    glBindVertexArray(meshSphere.getVAOID());

    glBindBuffer(GL_ARRAY_BUFFER, instVBO);
    GLint aInstPos = progInst.getAttribute("aInstPos");
    if (aInstPos >= 0) {
        glEnableVertexAttribArray(aInstPos);
        glVertexAttribPointer(aInstPos, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
        glVertexAttribDivisor(aInstPos, 1);
    }
    else {
        printf("aInstPos not found in shader\n");
        return;
    }

    // Send uniforms to GPU/shader
    progInst.bind();
    glUniformMatrix4fv(progInst.getUniform("P"), 1, GL_FALSE, glm::value_ptr(P.topMatrix()));
    glUniformMatrix4fv(progInst.getUniform("MV"), 1, GL_FALSE, glm::value_ptr(MV.topMatrix()));
    glUniformMatrix4fv(progInst.getUniform("MV_it"), 1, GL_FALSE, 
                      glm::value_ptr(glm::inverse(glm::transpose(MV.topMatrix()))));
    glUniform3fv(progInst.getUniform("lightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(progInst.getUniform("lightCol"), 1, glm::value_ptr(lightColor));
    glUniform1f(progInst.getUniform("particleScale"), particleScale);
    
    // meshSphere.draw(prog, true, 0, instCt);
    glPointSize((GLfloat)particleScale);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArraysInstanced(GL_POINTS, 0, 1, (GLsizei)instCt);

    glDisableVertexAttribArray(aInstPos);
    glVertexAttribDivisor(aInstPos, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    progInst.unbind();
    GLSL::checkError();

    // Draw bounding box / wireframe
    drawBoundingBoxWireframe(MV, P, progBasic);

    GLSL::checkError();
}

// I <3 Zelun
static inline bool within_inc(uint val, uint left, uint right) {
    return left <= val && val <= right;
}

void EventData::drawFrame(Program &prog, glm::vec2 viewport_resolution, bool morlet, float freq, bool pca) {
    float timeBound_L, timeBound_R; 
    uint eventBound_L, eventBound_R;

    // Set up point size
    float aspectWidth = viewport_resolution.x / static_cast<float>(camera_resolution.x);
    float aspectHeight = viewport_resolution.y / static_cast<float>(camera_resolution.y);
    glPointSize(1.0f * glm::max(aspectHeight, aspectWidth));

    // Generate buffers
    static GLuint VBO, VAO;
    static bool initialized = false;
    if (!initialized) {
        glGenBuffers(1, &VBO); 
        glGenVertexArrays(1, &VAO); 
        initialized = true;
    }

    // Set up bounds
    timeBound_L =  timeWindow_L + timeShutterWindow_L;
    timeBound_R = timeWindow_L + timeShutterWindow_R;
    eventBound_L = eventWindow_L + eventShutterWindow_L;
    eventBound_R = eventWindow_L + eventShutterWindow_R;

    // Select contribution function
    BaseFunc *contributionFunc = nullptr;
    int choice = morlet ? 1 : 0; // Can be expanded for new contribution functions
    switch (choice) {
        case 0:
            contributionFunc = new BaseFunc();
            break;

        case 1: 
            float f = freq / 1000000 / diffScale;
            float h = (timeBound_R - timeBound_L) / 2; // Very rough full width at half maximum
            float center_t = timeBound_L + h;
            contributionFunc = new morletFunc(f, h, center_t);
            break;
    }

    glm::vec2 rolling_sum(0.0f, 0.0f);
    std::vector<float> total;
    for (size_t i = eventBound_L; i <= eventBound_R; ++i) {
        float x(evtParticles[i].x), y(evtParticles[i].y), t(evtParticles[i].z);
        float polarity = evtParticles[i].w;

        contributionFunc->setX(x);
        contributionFunc->setY(y);
        contributionFunc->setT(t);
        contributionFunc->setPolarity(polarity);
        
        if (within_inc(x, spaceWindow.w, spaceWindow.y) && within_inc(y, spaceWindow.x, spaceWindow.z)) {
            total.push_back(x);
            total.push_back(y);
            total.push_back(contributionFunc->getWeight());

            rolling_sum.x += x;
            rolling_sum.y += y;
        }
    }

    // Load data points
    glBindVertexArray(VAO); 
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, total.size() * sizeof(float), total.data(), GL_DYNAMIC_DRAW);

    prog.bind();

    int pos = prog.getAttribute("pos");
	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);
	glEnableVertexAttribArray(pos);

    glm::mat4 projection = glm::ortho(minXYZ.x, maxXYZ.x, minXYZ.y, maxXYZ.y);
    glUniformMatrix4fv(prog.getUniform("projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glDrawArrays(GL_POINTS, 0, total.size());

    prog.unbind();

    glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
    delete contributionFunc;

    if (pca) {
        // Calculate covariance

        // TODO: inverse change? (make sure to always update when new files are used)
        float mean_x = rolling_sum.x / (total.size() / 3);
        float mean_y = rolling_sum.y / (total.size() / 3);

        float cov_x_x(0.0f), cov_x_y(0.0f), cov_y_y(0.0f);
        for (size_t i = 0; i < total.size(); i += 3) {
            float x = total[i];
            float y = total[i + 1];

            cov_x_y += (x - mean_x) * (y - mean_y);
            cov_x_x += (x - mean_x) * (x - mean_x);
            cov_y_y += (y - mean_y) * (y - mean_y);
        }

        cov_x_y /= (total.size() / 3 - 1);
        cov_x_x /= (total.size() / 3 - 1);
        cov_y_y /= (total.size() / 3 - 1);

        // Matrix
        float a = 1;
        float b = -(cov_x_x + cov_y_y);
        float c = cov_x_x * cov_y_y - cov_x_y * cov_x_y;

        float eigen1 = (-b + std::sqrt(b*b - 4 * a * c)) / (2 * a);
        float eigen2 = (-b - std::sqrt(b*b - 4 * a * c)) / (2 * a);

        // Eigen vectors
        std::vector<glm::vec3> eigenvectors;
        eigenvectors.push_back(glm::normalize(glm::vec3(eigen1 - cov_y_y, cov_x_y, 1)));
        eigenvectors.push_back(glm::normalize(glm::vec3(eigen2 - cov_y_y, cov_x_y, 1)));

        glBegin(GL_LINES);
        glVertex3f(0, 0, 1);
        glVertex3f(eigenvectors[0].x, eigenvectors[0].y, eigenvectors[0].z);
        glVertex3f(0, 0, 1);
        glVertex3f(eigenvectors[1].x, eigenvectors[1].y, eigenvectors[1].z);
        glEnd();

    }
	
    GLSL::checkError(GET_FILE_LINE);
}

void EventData::normalizeTime() {
    float factor = diffScale * TIME_CONVERSION;
    minXYZ.z *= factor;
    maxXYZ.z *= factor;
    timeWindow_L *= factor;
    timeWindow_R *= factor;
    timeShutterWindow_L *= factor;
    timeShutterWindow_R *= factor;
}

void EventData::oddizeTime() {
    float factor = diffScale * TIME_CONVERSION;
    minXYZ.z /= factor;
    maxXYZ.z /= factor;
    timeWindow_L /= factor;
    timeWindow_R /= factor;
    timeShutterWindow_L /= factor;
    timeShutterWindow_R /= factor;
}

float EventData::getTimestamp(uint eventIndex, float oddFactor) const {
    return evtParticles[eventIndex].z / oddFactor;
}

// If timestamp does not exist return first event included in window
uint EventData::getFirstEvent(float timestamp, float normFactor) const {
    timestamp *= normFactor; 

    for (size_t i = 0; i < evtParticles.size(); i++) { // TODO binary search?
        float t = evtParticles[i].z;

        if (t >= timestamp) {
            return i;
        }
    }
    std::cerr << "Error: getFirstEvent" << std::endl;
    return std::numeric_limits<uint>::max();
} 

// If timestamp does not exist return last event included in window
uint EventData::getLastEvent(float timestamp, float normFactor) const {
    assert(this->evtParticles.size() != 0);
    timestamp *= normFactor; 

    for (size_t i = evtParticles.size() - 1; i >= 0; i--) { // TODO binary search?
        float t = evtParticles[i].z;

        if (t <= timestamp) {
            return i;
        }
    }
    std::cerr << "Error: getLastEvent" << std::endl;
    return std::numeric_limits<uint>::max();
}

