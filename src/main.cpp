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
Camera g_camera;
bool g_cursorVisible(false);
bool g_isMainviewportHovered(false);
bool g_keyToggles[256] = {false};
float g_fps, g_lastRenderTime(0.0f);
string g_resourceDir, g_dataFilepath;

MainScene g_mainSceneFBO;
MainScene g_frameSceneFBO; // TODO maybe change class

Mesh g_meshSphere, g_meshSquare, g_meshCube, g_meshWeirdSquare;
Program g_progScene;
Program g_progFrameScene;

glm::vec3 g_lightPos, g_lightCol;
BPMaterial g_lightMat;

// Maybe use unique_ptr
shared_ptr<EventData> g_eventData;

int g_focusedEvent = -1;
float g_particleScale(0.35f);

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
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        io.Fonts->AddFontFromFileTTF(string(g_resourceDir + "/CascadiaCode.ttf").c_str(), 20.0f);
    
        // bbb2e9 hex R:187, G:178, B:233
        ImGuiStyle &style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        ImGui::StyleColorsDark(); 
        ImGui_ImplGlfw_InitForOpenGL(g_window, true);
        ImGui_ImplOpenGL3_Init("#version 430");

        initImGuiStyle(style);

    // Load .aedat events into EventData object //
        g_eventData = make_shared<EventData>();
        g_eventData->initParticlesFromFile(g_dataFilepath);

    // Camera //
        g_camera = Camera();
        g_camera.setInitPos(700.0f, 125.0f, 1500.0f);
        g_camera.setEvtCenter(g_eventData->getCenter());

    // Shader Programs //
        g_progScene = genPhongProg(g_resourceDir);
        g_progFrameScene = genBasicProg(g_resourceDir); 

    // Load Shape(s) & Scene //
        g_meshSphere.loadMesh(g_resourceDir + "sphere.obj");
        g_meshCube.loadMesh(g_resourceDir + "cube.obj");

        g_meshSphere.init();
        g_meshCube.init();

        g_lightPos = glm::vec3(0.0f, 1000.0f, 0.0f);
        g_lightCol = glm::vec3((187 / 255.0f), (178 / 255.0f), (233 / 255.0f));
        g_lightMat = BPMaterial(g_lightCol, g_lightCol, g_lightCol, 100.0f);

        int width, height;
        glfwGetFramebufferSize(g_window, &width, &height);
        g_mainSceneFBO.initialize(width, height);
        g_frameSceneFBO.initialize(width, height, true); // TODO consider normalization

    GLSL::checkError();
}

static void render() {
    float t = static_cast<float>(glfwGetTime());

    // Get frame buffer size //
        int width, height;
        glfwGetFramebufferSize(g_window, &width, &height);
        g_camera.aspect = (float)width / (float)height;

    // Update FPS counter //
        float dt = t - g_lastRenderTime;
        g_fps = 1.0f / dt;
        g_lastRenderTime = t;

    // Enable wireframe
    if (g_keyToggles[(unsigned)'t']) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    g_mainSceneFBO.bind();
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    MatrixStack P, MV;
    P.pushMatrix();
    MV.pushMatrix();
    g_camera.applyProjectionMatrix(P);
    g_camera.applyViewMatrix(MV);
    
    // Draw Main Scene //
        g_eventData->draw(MV, P, g_progScene,
                          g_particleScale, g_focusedEvent,
                          g_lightPos, g_lightCol,
                          g_lightMat, g_meshSphere,
                          g_meshCube);

    P.popMatrix();
    MV.popMatrix();
    g_mainSceneFBO.unbind();

    g_frameSceneFBO.bind();
    glViewport(0, 0, width, height); // real TODO change width and height to match RESOLUTION 
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glDisable(GL_DEPTH_TEST); // TODO necessary?
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // TODO need depth bit?

    // Draw Frame //
        glm::vec2 viewport_resolution(g_frameSceneFBO.getFBOwidth(), g_frameSceneFBO.getFBOheight());
        std::vector<glm::vec3> eigenvectors;
        g_eventData->drawFrame(g_progFrameScene, eigenvectors, viewport_resolution); 
            
        if (g_eventData->getPCA()) { // TODO integrate into drawFrame
            glBegin(GL_LINES);
            glVertex3f(eigenvectors[0].x, eigenvectors[0].y, eigenvectors[0].z);
            // glVertex3f(eigenvectors[1].x, eigenvectors[1].y, eigenvectors[1].z);
            glEnd();
        }

    g_frameSceneFBO.unbind();

    // Build ImGui Docking & Main Viewport //
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        drawGUI(g_camera, g_fps, g_particleScale, g_isMainviewportHovered, g_mainSceneFBO, g_frameSceneFBO, g_eventData);
    
    // Render ImGui //
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* context_backup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(context_backup);
        }

    GLSL::checkError(GET_FILE_LINE);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cout << "Usage: ./NOVA <resource_dir> <data_filepath>" << endl;
        return 0;
    }

    g_resourceDir = argv[1] + string("/");
    g_dataFilepath = argv[2];

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        cerr << "Failed to initialize GLFW" << endl;
        return -1;
    }

    // TODO: Can keep for a "fullscreen" mode setting later perhaps
    // GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    // const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    // g_window = glfwCreateWindow(mode->width, mode->height, "NOVA", monitor, nullptr);
    // g_window = glfwCreateWindow(1920, 1080, "NOVA", nullptr, nullptr);
    g_window = glfwCreateWindow(1240, 600, "NOVA", nullptr, nullptr);
    if (!g_window) {
        cerr << "Failed to create window" << endl;
        glfwTerminate();
        return -1;
    }

    // Placement above init() assumes parameters are initialized, and that wc has inf lifetime 
    WindowContext wc = { &g_camera, &g_cursorVisible, g_keyToggles, &g_isMainviewportHovered, &g_mainSceneFBO, &g_frameSceneFBO }; 
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
    glfwSetScrollCallback(g_window, scroll_callback);
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
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(g_window);
    glfwTerminate();

    return 0;
}