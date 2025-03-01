#include "include.h"

#include <dv-processing/core/core.hpp>
#include <dv-processing/core/utils.hpp>
#include <dv-processing/io/mono_camera_recording.hpp>
#include <dv-processing/core/frame.hpp>

// TODO: Probably just using namespace std is fine at this point
using std::cout, std::cerr, std::endl;
using std::vector, std::string, std::make_shared, std::shared_ptr, std::pair, std::array, std::tuple;
using std::stoi, std::stoul, std::min, std::max, std::numeric_limits, std::abs;
using namespace std;

// We can pass in a user pointer to callback functions - shouldn't require an updater; vars have inf lifespan
GLFWwindow *g_window;
FreeCam g_freecam;
bool g_cursorVisible(false);
bool g_keyToggles[256] = {false};
float g_fps, g_lastRenderTime(0.0f);
string g_resourceDir, g_dataFilepath;

Mesh g_meshSphere, g_meshSquare, g_meshCube, g_meshWeirdSquare;
Program g_progScene;

glm::vec3 g_lightPos, g_lightCol;
BPMaterial g_lightMat;

// Store (x, y, timestamp) particles for world space, with padding optimization
vector<vector<glm::vec3> > g_evtParticles;
vector<size_t> g_evtParticleSizes;
long long g_initTimestamp, g_lastTimestamp;
int g_focusedEvent = -1;
float g_particleScale(1.0f);

/*
    We need to load the particle data from the .aedat4 in data/

    TODO: I don't like .back()
*/
static void loadEventParticles(const std::string &data_filepath) {
    dv::io::MonoCameraRecording reader(data_filepath);

    // TODO: Could dynamically change this within
    size_t mod_freq = 100;
    size_t biggest_sz = 0;

    // https://dv-processing.inivation.com/rel_1_7/reading_data.html#read-events-from-a-file
    while (reader.isRunning()) {
        if (const auto events = reader.getNextEventBatch(); events.has_value()) {
            // New event; push back
            g_lastTimestamp = 0;
            g_evtParticles.push_back(vector<glm::vec3>());
            for (size_t i = 0; i < events.value().size(); i++) {
                const auto &event = events.value()[i];

                if (g_evtParticles[0].empty()) {
                    g_initTimestamp = event.timestamp();
                }

                g_lastTimestamp = std::max(g_lastTimestamp, event.timestamp());

                if (i % mod_freq == 0) {
                    // Should be relative
                    float relative_timestamp = static_cast<float>(event.timestamp() - g_initTimestamp);
                    g_evtParticles.back().push_back(glm::vec3((float)event.x(), (float)event.y(), relative_timestamp));
                }
            }

            biggest_sz = std::max(biggest_sz, g_evtParticles.back().size());
            g_evtParticleSizes.push_back(g_evtParticles.back().size());
        }
    }

    printf("There are %zu events\n", g_evtParticles.size());
    printf("biggest_sz = %zu\n", biggest_sz);

    // Should rescale with padding, can do normalization x - min / max - min
    for (size_t i = 0; i < g_evtParticles.size(); i++) {
        g_evtParticles[i].resize(biggest_sz, glm::vec3(0.0f));
        for (size_t j = 0; j < g_evtParticleSizes[i]; j++) {
            g_evtParticles[i][j].z = g_evtParticles[i][j].z / static_cast<float>(g_lastTimestamp - g_initTimestamp);
        }
    }
}

static void init() {
    srand(0);

    glfwSetTime(0.0);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    // ImGui //
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        io.Fonts->AddFontFromFileTTF(string(g_resourceDir + "/CascadiaCode.ttf").c_str(), 20.0f);

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(g_window, true);
        ImGui_ImplOpenGL3_Init("#version 430");

    // FreeCam //
        g_freecam = FreeCam();
        g_freecam.setInitPos(0.0f, 0.0f, 0.0f);

    // Shader Programs //
        g_progScene = genPhongProg(g_resourceDir);

    // Load Shape(s) & Scene //
        g_meshSphere.loadMesh(g_resourceDir + "sphere.obj");

        g_meshSphere.init();

        g_lightPos = glm::vec3(0.0f, 1000.0f, 0.0f);
        g_lightCol = glm::vec3((187 / 255.0f), (178 / 255.0f), (233 / 255.0f));
        g_lightMat = BPMaterial(g_lightCol, g_lightCol, g_lightCol, 100.0f);

    // Load .aedat events into global buffer //
        loadEventParticles(g_dataFilepath);

    GLSL::checkError();
    printf("[DEBUG] Made it through init()");
}

// Simply interpolate green->red based on g_initTimestamp and g_lastTimestamp.
static inline glm::vec3 getTimeColor(float timestamp) {
    // t = x - min / max - min (that said, all the evt timestamps are normalized - initTimestamp)
    return glm::vec3(timestamp, 0.0f, 1.0f - timestamp);
}

static void drawParticles(MatrixStack &MV, MatrixStack &P) {
    Program &prog = g_progScene;

    // TODO: Use instanced rendering for speedup, we know buffer sz
    prog.bind();
    MV.pushMatrix();
        for (size_t i = 0; i < g_evtParticles.size(); i++) {
            for (size_t j = 0; j < g_evtParticleSizes[i]; j++) {
                MV.pushMatrix();
                    MV.translate(g_evtParticles[i][j]);
                    MV.scale(g_particleScale);

                    glm::vec3 lerp_color = getTimeColor(g_evtParticles[i][j].z);

                    // FIXME: Unsafe comparison, but should underflow and still be far larger.
                    if (g_focusedEvent != -1 && g_focusedEvent != i) {
                        lerp_color *= 0.15f;   
                    }

                    sendToPhongShader(prog, P, MV, g_lightPos, lerp_color, g_lightMat);
                    g_meshSphere.draw(prog);
                MV.popMatrix();
            }
        }
    MV.popMatrix();
    prog.unbind();
}

static void drawSun(MatrixStack &MV, MatrixStack &P) {
    Program &prog = g_progScene;

    prog.bind();
    MV.pushMatrix();
        MV.translate(g_lightPos);
        MV.scale(10.0f);
        
        sendToPhongShader(prog, P, MV, g_lightPos, g_lightCol, g_lightMat);
        g_meshSphere.draw(prog);
    MV.popMatrix();
    prog.unbind();
    
    GLSL::checkError();
}

static void render() {
    float t = static_cast<float>(glfwGetTime());

    // ImGui Frame //
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();

    // Get frame buffer size //
        int width, height;
        glfwGetFramebufferSize(g_window, &width, &height);
        g_freecam.aspect = (float)width / (float)height;

    // Update FPS counter //
        float dt = t - g_lastRenderTime;
        g_fps = 1.0f / dt;
        g_lastRenderTime = t;

    // Update camera //
        g_freecam.update(dt);

    // Enable wireframe
    if (g_keyToggles[(unsigned)'t']) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    MatrixStack P, MV;
    P.pushMatrix();
    MV.pushMatrix();
    g_freecam.applyProjectionMatrix(P);
    g_freecam.applyViewMatrix(MV);

    // TODO: Draw something basic
    drawSun(MV, P);
    drawParticles(MV, P);

    P.popMatrix();
    MV.popMatrix();

    drawGUI(g_freecam, g_fps, g_particleScale, g_focusedEvent, g_evtParticles.size());

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    GLSL::checkError(GET_FILE_LINE);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cout << "Usage: ./NOVA <resource_dir> <data_filepath>" << endl;
        return 0;
    }

    g_resourceDir = argv[1] + string("/");
    g_dataFilepath = argv[2];

    if (!glfwInit()) {
        cerr << "Failed to initialize GLFW" << endl;
        return -1;
    }

    glfwSetErrorCallback(error_callback);

    // GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    // const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    // g_window = glfwCreateWindow(mode->width, mode->height, "NOVA", monitor, nullptr);
    g_window = glfwCreateWindow(1280, 720, "NOVA", nullptr, nullptr);
    if (!g_window) {
        cerr << "Failed to create window" << endl;
        glfwTerminate();
        return -1;
    }

    // Placement above init() assumes parameters are initialized, and that wc has inf lifetime 
    WindowContext wc = { &g_freecam, &g_cursorVisible, g_keyToggles };
    glfwSetWindowUserPointer(g_window, &wc);
    glfwMakeContextCurrent(g_window);

    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        cerr << "Failed to initialize GLEW" << endl;
        return -1;
    }

    glGetError();
    cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
    cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
    GLSL::checkVersion();

    glfwSwapInterval(1);
    glfwSetKeyCallback(g_window, key_callback);
    glfwSetMouseButtonCallback(g_window, mouse_button_callback);
    glfwSetCursorPosCallback(g_window, cursor_position_callback);
    glfwSetCharCallback(g_window, char_callback);
    glfwSetFramebufferSizeCallback(g_window, resize_callback);

    init();

    while (!glfwWindowShouldClose(g_window)) {
        render();

        glfwSwapBuffers(g_window);
        glfwPollEvents();
    }

    // Cleanup //
    glfwDestroyWindow(g_window);
    glfwTerminate();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    return 0;
}