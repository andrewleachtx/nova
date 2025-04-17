#include "include.h"

#include <dv-processing/core/core.hpp>
#include <dv-processing/core/utils.hpp>
#include <dv-processing/io/mono_camera_recording.hpp>
#include <dv-processing/core/frame.hpp>

using namespace std;

// Macros
#ifdef _WIN32
    #define popen_macro _popen
    #define pclose_macro _pclose
#else
    #define popen_macro popen
    #define pclose_macro pclose
#endif

// Constants
const string cmd_format{"ffmpeg -y -f rawvideo -pix_fmt rgb24 -s %ux%u -i - -c:v libx264 -pix_fmt yuv420p -vf \"pad=ceil(iw/2)*2:ceil(ih/2)*2, vflip\" -r 30 -preset veryfast %s.mp4"}; // TODO make background process?

// We can pass in a user pointer to callback functions - shouldn't require an updater; vars have inf lifespan
GLFWwindow *g_window;
Camera g_camera;
bool g_cursorVisible(false);
bool g_isMainviewportHovered(false);
bool g_keyToggles[256] = {false};
float g_fps, g_lastRenderTime(0.0f);
string g_resourceDir, g_dataFilepath, g_dataDir;
bool loadFile;

MainScene g_mainSceneFBO;
FrameScene g_frameSceneFBO;

bool recording;
string video_name;
GLuint vid_width, vid_height;
FILE *ffmpeg;
vector<unsigned char> pixels;

Mesh g_meshSphere;
Program g_progBasic, g_progInst, g_progFrame;

glm::vec3 g_lightPos, g_lightCol;
BPMaterial g_lightMat;

// Maybe use unique_ptr
shared_ptr<EventData> g_eventData;

float g_particleScale(0.75f);

static void updateEvtDataAndCamera() {
    // Load .aedat events into EventData object //
    g_eventData = make_shared<EventData>();
    g_eventData->initParticlesFromFile(g_dataFilepath);
    g_eventData->initInstancing(g_progInst);

    // Camera //
    g_camera = Camera();
    g_camera.setInitPos(700.0f, 125.0f, 1500.0f);
    g_camera.setEvtCenter(g_eventData->getCenter());

    loadFile = false;
}

static void initEvtDataAndCamera() {
    // Load .aedat events into EventData object //
    g_eventData = make_shared<EventData>();
    g_eventData->initParticlesEmpty();
    g_eventData->initInstancing(g_progInst);

    // Camera //
    g_camera = Camera();
    g_camera.setInitPos(700.0f, 125.0f, 1500.0f);
    g_camera.setEvtCenter(g_eventData->getCenter());

    loadFile = false;
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
        ImGuiStyle &style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        ImGui::StyleColorsDark(); 
        ImGui_ImplGlfw_InitForOpenGL(g_window, true);
        ImGui_ImplOpenGL3_Init("#version 430");

        initImGuiStyle(style);

    // Shader Programs //
        g_progBasic = genPhongProg(g_resourceDir);
        g_progInst = genInstProg(g_resourceDir);
        g_progFrame = genBasicProg(g_resourceDir); 

    // Initialize data + camera and set its center //
        initEvtDataAndCamera();

    // Load Shape(s) & Scene //
        g_meshSphere.loadMesh(g_resourceDir + "sphere.obj");
        g_meshSphere.init();

        g_lightPos = glm::vec3(0.0f, 1000.0f, 0.0f);
        g_lightCol = glm::vec3((187 / 255.0f), (178 / 255.0f), (233 / 255.0f));
        g_lightMat = BPMaterial(g_lightCol, g_lightCol, g_lightCol, 100.0f);

        int width, height;
        glfwGetFramebufferSize(g_window, &width, &height);
        g_mainSceneFBO.initialize(width, height);
        g_frameSceneFBO.initialize(width, height); // TODO consider GL_RGB32F

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
    // g_camera.applyOrthoMatrix(P);
    g_camera.applyViewMatrix(MV);
    
    // Draw Main Scene //
        // g_eventData->draw(MV, P, g_progBasic,
        //     g_particleScale, g_focusedEvent,
        //     g_lightPos, g_lightCol,
        //     g_lightMat, g_meshSphere
        // );
        g_eventData->drawInstanced(MV, P, g_progInst,
            g_progBasic, g_particleScale,
            g_lightPos, g_lightCol,
            g_lightMat, g_meshSphere
        );

    P.popMatrix();
    MV.popMatrix();
    g_mainSceneFBO.unbind();

    // Draw Frame // 
    // FIXME: make method for this i.e. g_eventData->drawDCEFrame() ? render() easily gets bloated, this is fine though if we
    // don't have many more
    float nextUpdateTime = t - 1 / g_frameSceneFBO.getUpdateFPS();
    if (g_frameSceneFBO.getAutoUpdate() != FrameScene::MANUAL_UPDATE && nextUpdateTime >= g_frameSceneFBO.getLastRenderTime()) {

        if (g_frameSceneFBO.getAutoUpdate() == FrameScene::EVENT_AUTO_UPDATE) {
            uint framePeriod = g_frameSceneFBO.getFramePeriod_E();
            uint frameStart = g_eventData->getEventWindow_L();
            uint frameEnd = g_eventData->getEventWindow_R();
            uint maxEvent = g_eventData->getMaxEvent() - 1;

            if (frameStart == frameEnd) { // TODO consider breaking when right bound reaches end
                g_frameSceneFBO.getAutoUpdate() = FrameScene::MANUAL_UPDATE;
            }
            else {
                g_eventData->getEventWindow_L() = glm::min(maxEvent, frameStart + framePeriod);
                g_eventData->getEventWindow_R() = glm::min(maxEvent, frameEnd + framePeriod);
                g_eventData->getTimeWindow_L() = g_eventData->getTimestamp(g_eventData->getEventWindow_L());
                g_eventData->getTimeWindow_R() = g_eventData->getTimestamp(g_eventData->getEventWindow_R());
            }
        }
        else if (g_frameSceneFBO.getAutoUpdate() == FrameScene::TIME_AUTO_UPDATE) {
            float framePeriod = g_frameSceneFBO.getFramePeriod_T();
            float frameStart = g_eventData->getTimeWindow_L();
            float frameEnd = g_eventData->getTimeWindow_R();
            float maxTime = g_eventData->getMaxTimestamp();

            if (frameStart == frameEnd) { // TODO consider breaking when right bound reaches end
                g_frameSceneFBO.getAutoUpdate() = FrameScene::MANUAL_UPDATE;
            }
            else {
                g_eventData->getTimeWindow_L() = glm::min(maxTime, frameStart + framePeriod);
                g_eventData->getTimeWindow_R() = glm::min(maxTime, frameEnd + framePeriod);
                g_eventData->getEventWindow_L() = g_eventData->getFirstEvent(g_eventData->getTimeWindow_L());
                g_eventData->getEventWindow_R() = g_eventData->getLastEvent(g_eventData->getTimeWindow_R());
            }
        }
        g_frameSceneFBO.setDirtyBit(true); 
        g_frameSceneFBO.setLastRenderTime(t);
    }

    if (g_frameSceneFBO.getDirtyBit()) {
        g_frameSceneFBO.bind();
        glViewport(0, 0, width, height); 
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glDisable(GL_DEPTH_TEST); // TODO necessary? Andrew: Not unless you enable it somewhere else in the loop.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // TODO need depth bit? Andrew: Good practice to clear buffers, doesn't hurt

        glm::vec2 viewport_resolution(g_frameSceneFBO.getFBOwidth(), g_frameSceneFBO.getFBOheight());
        g_eventData->drawFrame(g_progFrame, viewport_resolution, 
            g_frameSceneFBO.isMorlet(), g_frameSceneFBO.getFreq(), g_frameSceneFBO.getPCA()); 
                
        g_frameSceneFBO.unbind();
        g_frameSceneFBO.setDirtyBit(false);
    }

    // Build ImGui Docking & Main Viewport //
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        drawGUI(g_camera, g_fps, g_particleScale, g_isMainviewportHovered, g_mainSceneFBO, 
            g_frameSceneFBO, g_eventData, g_dataFilepath, video_name, recording, g_dataDir, loadFile);
    
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

// FIXME: Add params and move to utils ?
static void video_output() {
    if (recording) {

        if (ffmpeg == nullptr) { // Check if beginning recording
            vid_width = g_mainSceneFBO.getFBOwidth();
            vid_height = g_mainSceneFBO.getFBOheight();

            // Dynamically construct command
            unsigned int cmd_size = static_cast<unsigned int>(cmd_format.size() + std::to_string(vid_width).size() + 
                std::to_string(vid_height).size() + video_name.size() - 6); // -6 accounts for each %_
            char *cmd = new char[cmd_size]; 
            sprintf(cmd, cmd_format.c_str(), vid_width, vid_height, video_name.c_str());

            ffmpeg = popen_macro(cmd, "wb");
            if (!ffmpeg) { cerr << "ffmpeg error" << endl; } // TODO add error handling?

            delete[] cmd;
            pixels.resize(vid_width * vid_height * 3);
        }

        glReadPixels(0, 0, vid_width, vid_height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data()); 
        fwrite(pixels.data(), vid_width * vid_height * 3, 1, ffmpeg);
    }
    else if (ffmpeg != nullptr) { // Check if recording stopped
        pclose_macro(ffmpeg);
        ffmpeg = nullptr;
    }

}

int main(int argc, char** argv) {
    // resources/ data/ 
    if (argc < 3) {
        cout << "Usage: ./NOVA <resource_dir> <data_dir>" << endl;
        return 0;
    }

    g_resourceDir = argv[1] + string("/");
    g_dataDir = argv[2];

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        cerr << "Failed to initialize GLFW" << endl;
        return -1;
    }

    // TODO: Can keep for a "fullscreen" mode setting later
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

    /*
        FIXME: g_dataFilepath not sanitized when init() -> initEvtDataAndCamera() gets called, think
        about fixing state
    */
    init();

    string curFilepath = g_dataFilepath;
    while (!glfwWindowShouldClose(g_window)) {
        render();
        
        if (loadFile) {
            curFilepath = g_dataFilepath;
            updateEvtDataAndCamera();
        }

        glfwSwapBuffers(g_window);
        glfwPollEvents();

        video_output();
    }

    // Cleanup //
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(g_window);
    glfwTerminate();

    return 0;
}