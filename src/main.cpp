#include "include.h"

#include <dv-processing/core/core.hpp>
#include <dv-processing/core/frame.hpp>
#include <dv-processing/io/mono_camera_recording.hpp>
#include <dv-processing/core/frame.hpp>

// TODO: Probably just using namespace std is fine at this point
using std::cout, std::cerr, std::endl;
using std::vector, std::string, std::make_shared, std::shared_ptr, std::pair, std::array, std::tuple;
using std::stoi, std::stoul, std::min, std::max, std::numeric_limits, std::abs;

// We can pass in a user pointer to callback functions - shouldn't require an updater; vars have inf lifespan
GLFWwindow *g_window;
FreeCam g_freecam;
bool g_cursorVisible(false);
bool g_keyToggles[256] = {false};
float g_fps, g_lastRenderTime(0.0f);
string g_resourceDir = "./";

Mesh g_meshSphere, g_meshSquare, g_meshCube, g_meshWeirdSquare;
Program g_progScene;

glm::vec3 g_lightPos(0.0f, 100.0f, 0.0f), g_lightCol;
BPMaterial g_lightMat;

static void init() {
    srand(0);

    glfwSetTime(0.0);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    // ImGui //
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& imgui_io = ImGui::GetIO(); (void)imgui_io;

        imgui_io.Fonts->AddFontFromFileTTF(string(g_resourceDir + "/CascadiaCode.ttf").c_str(), 20.0f);
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

        g_lightCol = glm::vec3((187 / 255.0f), (178 / 255.0f), (233 / 255.0f));
        g_lightMat = BPMaterial(g_lightCol, g_lightCol, g_lightCol, 100.0f);

    GLSL::checkError();
    printf("[DEBUG] Made it through init()");
}

static void drawSun(MatrixStack& MV, MatrixStack& P) {
    Program& prog = g_progScene;

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

    P.popMatrix();
    MV.popMatrix();

    drawGUI(g_freecam, g_fps);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    GLSL::checkError(GET_FILE_LINE);
}

int main(int argc, char** argv) {
    cout << "Go package resolution https://davidmcelroy.org/?p=9964!!" << endl;

    if (argc < 3) {
        cout << "Usage: ./NOVA <resource_dir> <data_dir>" << endl;
        return 0;
    }

    g_resourceDir = argv[1] + string("/");

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        cerr << "Failed to initialize GLFW" << endl;
        return -1;
    }

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

    // GLFW Cleanup //
    glfwDestroyWindow(g_window);
    glfwTerminate();    

    return 0;
}