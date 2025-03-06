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

Mesh g_meshSphere, g_meshSquare, g_meshCube, g_meshWeirdSquare;
Program g_progScene;

glm::vec3 g_lightPos, g_lightCol;
BPMaterial g_lightMat;

// Maybe use unique_ptr
shared_ptr<EventData> g_eventData;

int g_focusedEvent = -1;
float g_particleScale(0.35f);

// https://github.com/ocornut/imgui/wiki/Docking
// Creates required dockspace before rendering ImGui windows on top
// TODO: Can we add power save ? https://github.com/ocornut/imgui/wiki/Implementing-Power-Save,-aka-Idling-outside-of-ImGui
static void drawGUIDockspace() {
    static bool is_fullscreen = true;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    ImGuiWindowFlags w_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    if (is_fullscreen) {
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        // ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

        w_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    }

    ImGui::Begin("DockSpace", nullptr, w_flags);
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("DockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
    ImGui::End();
}

// TODO: Move this and drawGUIDockspace into utils.h/.cpp
static void drawGUI2(const Camera& camera, float fps, float &particle_scale, int &focused_evt) {
    drawGUIDockspace();

    ImGui::Begin("Main Viewport");
        const glm::vec3 &cam_pos = camera.pos;
        
        // Have to do a bounding check to make sure the mouse is really within the actual main viewport img for cursor callbacks
        ImVec2 image_sz = ImGui::GetContentRegionAvail();
        ImVec2 fbo_imageSz = ImVec2(g_mainSceneFBO.getFBOwidth(), g_mainSceneFBO.getFBOheight());
        float img_aspect = image_sz.x / image_sz.y;
        float fbo_aspect = fbo_imageSz.x / fbo_imageSz.y;

        ImVec2 final_sz;
        if (fbo_aspect > img_aspect) {
            // Effectively the height is the limiting factor here, so we should max height and adjust width
            final_sz = ImVec2(image_sz.x * fbo_aspect, image_sz.x);
        }
        else {
            // Width limiting factor, adjust height
            final_sz = ImVec2(image_sz.x, image_sz.x / fbo_aspect);
        }

        ImGui::Image((ImTextureID)g_mainSceneFBO.getColorTexture(), final_sz, ImVec2(0, 1), ImVec2(1, 0));
        g_isMainviewportHovered = ImGui::IsItemHovered();
    ImGui::End();

    ImGui::Begin("Load");
        ImGui::Text("File:");

        if (ImGui::Button("Open File")) {
            // TODO: This should interact with the EventData object somehow, simply recalling init may not be clean
        }

        // TODO: Cache recent files and state?
    ImGui::End();

    ImGui::Begin("Info");
        ImGui::Text("Camera (World): (%.3f, %.3f, %.3f)", cam_pos.x, cam_pos.y, cam_pos.z);
        ImGui::Separator();
        ImGui::SliderFloat("Particle Scale", &particle_scale, 0.1f, 2.5f);
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", fps);
        // ImGui::PlotLines("FPS History", fps_historyBuf.data(), fps_historyBuf.size(), fps_bufIdx, nullptr, 0.0f, maxFPS + 10.0f, ImVec2(0, 80));
    ImGui::End();

    // TODO: Something like this, should add params
    ImGui::Begin("Time Slice Controls");
        ImGui::Text("Adjust left / right pointers:");
        // ImGui::SliderFLoat("left", &left, lower bound, upper bound)
        // ImGui::SliderFLoat("left", &left, lower bound, upper bound)
    ImGui::End();
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
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        io.Fonts->AddFontFromFileTTF(string(g_resourceDir + "/CascadiaCode.ttf").c_str(), 20.0f);
    
        // bbb2e9 hex R:187, G:178, B:233
        ImVec4 color_purple = ImVec4(0.733f, 0.698f, 0.914f, 1.0f);
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(g_window, true);
        ImGui_ImplOpenGL3_Init("#version 430");

    // Camera //
        g_camera = Camera();
        g_camera.setInitPos(0.0f, 0.0f, 0.0f);

    // Shader Programs //
        g_progScene = genPhongProg(g_resourceDir);

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

    // Load .aedat events into EventData object //
        g_eventData = make_shared<EventData>();
        g_eventData->initParticlesFromFile(g_dataFilepath);

    GLSL::checkError();
    cout << "[DEBUG] Made it out of init()" << endl;
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
    
    // Draw //
        // drawSun(MV, P);
        g_eventData->draw(MV, P, g_progScene,
                          g_particleScale, g_focusedEvent,
                          g_lightPos, g_lightCol,
                          g_lightMat, g_meshSphere,
                          g_meshCube);

    P.popMatrix();
    MV.popMatrix();
    g_mainSceneFBO.unbind();

    // Build ImGui Docking & Main Viewport //
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        drawGUI2(g_camera, g_fps, g_particleScale, g_focusedEvent);
    
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

    // GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    // const GLFWvidmode* mode = glfwG\etVideoMode(monitor);
    // g_window = glfwCreateWindow(mode->width, mode->height, "NOVA", monitor, nullptr);
    // g_window = glfwCreateWindow(1920, 1080, "NOVA", nullptr, nullptr);
    g_window = glfwCreateWindow(1240, 600, "NOVA", nullptr, nullptr);
    if (!g_window) {
        cerr << "Failed to create window" << endl;
        glfwTerminate();
        return -1;
    }

    // Placement above init() assumes parameters are initialized, and that wc has inf lifetime 
    WindowContext wc = { &g_camera, &g_cursorVisible, g_keyToggles, &g_isMainviewportHovered, &g_mainSceneFBO };
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
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(g_window);
    glfwTerminate();

    return 0;
}