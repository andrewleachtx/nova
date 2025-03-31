#include "EventData.h"
#include "utils.h"

#include <algorithm>
#include <cstdio>
#include <dv-processing/core/utils.hpp>

// TODO ask about default -1
EventData::EventData() : initTimestamp(0), lastTimestamp(0), timeWindow_L(0.0f), timeWindow_R(0.0f),
    min_XYZ(std::numeric_limits<float>::max()), max_XYZ(std::numeric_limits<float>::lowest()),
    center(glm::vec3(0.0f)), mod_freq(1), shutterWindow_L(0.0f), shutterWindow_R(0.0f) {}
EventData::~EventData() {}

void EventData::initParticlesFromFile(const std::string &filename, size_t point_freq) {
    dv::io::MonoCameraRecording reader(filename);
    camera_resolution = glm::vec2(reader.getEventResolution().value().width, reader.getEventResolution().value().height);
    mod_freq = point_freq;

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
            std::vector<glm::vec4> evtBatch;
            for (size_t i = 0; i < events.value().size(); i++) {
                if (i % 64 != 0) { continue; } // TODO speed up
                const auto &event = events.value()[i];
                
                if (particleBatches.empty() && evtBatch.empty()) {
                    initTimestamp = event.timestamp();
                }

                lastTimestamp = std::max(lastTimestamp, event.timestamp());

                float relativeTimestamp = static_cast<float>(event.timestamp() - initTimestamp);
                glm::vec4 event_xyzw = glm::vec4(static_cast<float>(event.x()), static_cast<float>(event.y()), 
                    relativeTimestamp, static_cast<float>(event.polarity()));
                evtBatch.push_back(event_xyzw);

                // glm::min/max are beautiful functions. Guarantees each component is min/max'd always
                raw_minXYZ = glm::min(raw_minXYZ, glm::vec3(event_xyzw)); // TODO simplify
                raw_maxXYZ = glm::max(raw_maxXYZ, glm::vec3(event_xyzw));
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
    diff_scale = 500.0f / static_cast<float>(lastTimestamp - initTimestamp);

    // Each particle is grouped by event s.t. particle[i] stores a vector of glm::vec3 with a normalized abs. timestamp
    for (size_t i = 0; i < particleBatches.size(); i++) {
        particleBatches[i].resize(max_batchSz, glm::vec4(0.0f));
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

    spaceWindow = glm::vec4(min_XYZ.y, max_XYZ.x, max_XYZ.y, min_XYZ.x);

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
        for (size_t i = 0; i < particleBatches.size(); i++) {
            for (size_t j = 0; j < particleSizes[i]; j++) {
                if (j % mod_freq == 0) {
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

// start 57, final max
static bool within_inc(float val, float left, float right) {
    return val >= left && val <= right;
}

void EventData::drawFrame(Program &prog, glm::vec2 viewport_resolution, bool morlet, float freq, bool pca) {
    float timeBound_L, timeBound_R,
        batch_min_t, batch_max_t,
        x, y, t, polarity; 

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

    // Expandable contribution function choice
    timeBound_L =  timeWindow_L + shutterWindow_L;
    timeBound_R = timeWindow_L + shutterWindow_R;

    // Select contribution function
    BaseFunc *contributionFunc = nullptr;
    int choice = morlet ? 1 : 0; // Can be expanded for new contribution functions
    switch (choice) {
        case 0:
            contributionFunc = new BaseFunc();
            break;

        case 1: 
            float f = freq / 1000000 / diff_scale;
            float h = (timeBound_R - timeBound_L) / 2; // Very rough full width at half maximum
            float center_t = timeBound_L + h;
            contributionFunc = new morletFunc(f, h, center_t);
            break;

    }

    glm::vec2 rolling_sum(0.0f, 0.0f);
    std::vector<float> total;
    for (size_t i = 0; i < particleBatches.size(); ++i) {
        // Check if batch included
        batch_min_t = particleBatches[i][0].z;
        batch_max_t = particleBatches[i][particleSizes[i] - 1].z;
        if (batch_min_t > timeBound_R) { break; }
        if (batch_max_t < timeBound_L) { continue; }
        
        for (size_t j = 0; j < particleSizes[i]; ++j) { // do binary search?
            x = particleBatches[i][j].x;
            y = particleBatches[i][j].y;
            t = particleBatches[i][j].z;
            polarity = particleBatches[i][j].w;
            contributionFunc->setX(x);
            contributionFunc->setY(y);
            contributionFunc->setT(t);
            contributionFunc->setPolarity(polarity);

            if (within_inc(t, timeBound_L, timeBound_R)) {
                if (within_inc(x, spaceWindow.w, spaceWindow.y) && within_inc(y, spaceWindow.x, spaceWindow.z)) {
                    total.push_back(x);
                    total.push_back(y);
                    total.push_back(contributionFunc->getWeight());

                    rolling_sum.x += x;
                    rolling_sum.y += y;
                }
            }
            else { // Assumes strictly increasing
                break;
            }
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

    glm::mat4 projection = glm::ortho(min_XYZ.x, max_XYZ.x, min_XYZ.y, max_XYZ.y);
    glUniformMatrix4fv(prog.getUniform("projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glDrawArrays(GL_POINTS, 0, total.size());

    prog.unbind();

    glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
    delete contributionFunc;

    if (pca) {
        // Calculate covariance
        float mean_x = rolling_sum.x / (total.size() / 3);
        float mean_y = rolling_sum.y / (total.size() / 3);

        float cov_x_y = 0;
        float cov_x_x = 0;
        float cov_y_y = 0;
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
