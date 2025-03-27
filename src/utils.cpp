#include "utils.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include "include.h"

#include <memory>
#include <iostream>
#include <vector>
#include <string>
#include <random>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

using std::cout, std::endl, std::cerr;
using std::shared_ptr, std::make_shared;
using std::vector, std::string;
using glm::vec3;

Program genPhongProg(const string &resource_dir) {
    Program prog = Program();
    prog.setShaderNames(resource_dir + "phong_vsh.glsl", resource_dir + "phong_fsh.glsl");
    prog.setVerbose(true);
    prog.init();

    prog.addUniform("P");
    prog.addUniform("MV");
    prog.addUniform("MV_it");

    prog.addAttribute("aPos");
    prog.addAttribute("aNor");
    prog.addAttribute("aTex");

    prog.addUniform("lightPos");
    prog.addUniform("lightCol");
    prog.addUniform("ka");
    prog.addUniform("kd");
    prog.addUniform("ks");
    prog.addUniform("s");

    // prog.setVerbose(false);

    return prog;
}

Program genBasicProg(const string &resource_dir) {
    Program prog = Program();
    prog.setShaderNames(resource_dir + "basic_vsh.glsl", resource_dir + "basic_fsh.glsl");
    prog.setVerbose(true);
    prog.init();

    prog.addAttribute("pos");
    prog.addUniform("projection");

    // prog.setVerbose(false);

    return prog;
}

void sendToPhongShader(const Program& prog, const MatrixStack& P, const MatrixStack& MV, const vec3& lightPos, const vec3& lightCol, const BPMaterial& mat) {
    glUniformMatrix4fv(prog.getUniform("P"), 1, GL_FALSE, glm::value_ptr(P.topMatrix()));
    glUniformMatrix4fv(prog.getUniform("MV"), 1, GL_FALSE, glm::value_ptr(MV.topMatrix()));
    glUniformMatrix4fv(prog.getUniform("MV_it"), 1, GL_FALSE, glm::value_ptr(glm::inverse(glm::transpose(MV.topMatrix()))));

    glUniform3fv(prog.getUniform("lightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(prog.getUniform("lightCol"), 1, glm::value_ptr(lightCol));

    glUniform3fv(prog.getUniform("ka"), 1, glm::value_ptr(mat.ka));
    glUniform3fv(prog.getUniform("kd"), 1, glm::value_ptr(mat.kd));
    glUniform3fv(prog.getUniform("ks"), 1, glm::value_ptr(mat.ks));
    glUniform1f(prog.getUniform("s"), mat.s);
}

// This function is called when a GLFW error occurs
void error_callback(int error, const char *description) {
    cerr << "Error: " << description << endl;
}

// This function is called when a key is pressed
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));

    if (ImGui::GetIO().WantCaptureKeyboard) {
        return;
    }

    if (key == GLFW_KEY_ESCAPE) {
        if (action == GLFW_PRESS) {
            if (*(wc->is_cursorVisible)) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            *(wc->is_cursorVisible) = !*(wc->is_cursorVisible);
        }
        else if (action == GLFW_REPEAT) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }
    }
}

// This function is called when the mouse is clicked
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    // if (ImGui::GetIO().WantCaptureMouse) {
    //     return;
    // }

    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));

    if (!*(wc->is_mainViewportHovered)) {
        return;
    }

	double xmouse, ymouse;
	glfwGetCursorPos(window, &xmouse, &ymouse);

	int width, height;
	glfwGetWindowSize(window, &width, &height);
	
    if (action == GLFW_PRESS) {
		bool shift = (mods & GLFW_MOD_SHIFT) != 0;
		bool ctrl  = (mods & GLFW_MOD_CONTROL) != 0;
		bool alt   = (mods & GLFW_MOD_ALT) != 0;

        if (!*(wc->is_cursorVisible)) {
            /* TODO: DO WE NEED THIS? */
            wc->camera->mouseClicked((float)xmouse, (float)ymouse, shift, ctrl, alt);

            if (button == GLFW_MOUSE_BUTTON_LEFT) {
            }
            else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            }
        }
	}
}

// Mouse scroll
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));

    if (!*(wc->is_mainViewportHovered)) {
        return;
    }

    // TODO: Add a function in the camera class to handle scrolling, also need to avoid ImGui scroll override
    wc->camera->translations.z -= 100.0f * (float)yoffset;
}

// This function is called when the mouse moves
void cursor_position_callback(GLFWwindow* window, double xmouse, double ymouse) {
    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));

    if (!*(wc->is_mainViewportHovered)) {
        return;
    }

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    if (state == GLFW_PRESS && !(*(wc->is_cursorVisible))) {
        wc->camera->mouseMoved((float)xmouse, (float)ymouse);
    }
}

// When a key is pressed
void char_callback(GLFWwindow *window, unsigned int key)
{
    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));
    wc->key_toggles[key] = !wc->key_toggles[key];

    if (ImGui::GetIO().WantCaptureKeyboard) {
        return;
    }
}

// If the window is resized, capture the new size and reset the viewport
void resize_callback(GLFWwindow *window, int width, int height) {
    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));

	glViewport(0, 0, width, height);

    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }
    
    // Update the aspect ratio of the camera
    wc->camera->aspect = (float)width / (float)height;

    // Update the FBO
    wc->mainSceneFBO->resize(width, height);
    wc->frameSceneFBO->resize(width, height);
}

// Looks for the biggest monitor
bool genBiggestWindow(GLFWwindow *&window, const string &window_name) {
    int num_monitors;
    GLFWmonitor** monitors = glfwGetMonitors(&num_monitors);

    int max_area = 0;
    GLFWmonitor* biggest_monitor = nullptr;

    for (int i = 0; i < num_monitors; i++) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        int area = mode->width * mode->height;
        if (area > max_area) {
            max_area = area;
            biggest_monitor = monitors[i];
        }
    }

    if (biggest_monitor != nullptr) {
        const GLFWvidmode* mode = glfwGetVideoMode(biggest_monitor);
        window = glfwCreateWindow(mode->width, mode->height, window_name.c_str(), biggest_monitor, nullptr);

        return true;
    }

    return false;
}

ImVec4 IMCOLOR_RED(1.0f, 0.0f, 0.0f, 1.0f);
ImVec4 IMCOLOR_GREEN(0.0f, 1.0f, 0.0f, 1.0f);
ImVec4 IMCOLOR_BLUE(0.0f, 0.5f, 1.0f, 1.0f);

static const size_t fps_bufferSz = 100;
static std::vector<float> fps_historyBuf(fps_bufferSz, 0.0f);
static size_t fps_bufIdx = 0;

void updateFPS(const float& fps) {
    fps_historyBuf[fps_bufIdx] = fps;
    fps_bufIdx = (fps_bufIdx + 1) % fps_bufferSz;
}

float calculateAverageFPS() {
    float sum = 0.0f;
    for (const float& fps : fps_historyBuf) {
        sum += fps;
    }
    return sum / fps_bufferSz;
}

float getMinFPS() {
    float min = fps_historyBuf[0];
    for (const float& fps : fps_historyBuf) {
        if (fps < min) {
            min = fps;
        }
    }
    return min;
}

float getMaxFPS() {
    float max = fps_historyBuf[0];
    for (const float& fps : fps_historyBuf) {
        if (fps > max) {
            max = fps;
        }
    }
    return max;
}

void initImGuiStyle(ImGuiStyle &style) {
    ImVec4 color_purple = ImVec4(0.733f, 0.698f, 0.914f, 1.0f);
    ImVec4 color_purple_darker = ImVec4(0.5f, 0.45f, 0.7f, 1.0f);
    ImVec4 color_background = ImVec4(0.12f, 0.12f, 0.18f, 1.0f);
    ImVec4 color_text = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
            
    style.Colors[ImGuiCol_WindowBg] = color_background;
    style.Colors[ImGuiCol_Text] = color_text;
    style.Colors[ImGuiCol_TitleBg] = color_purple_darker;
    style.Colors[ImGuiCol_TitleBgActive] = color_purple;
    style.Colors[ImGuiCol_Header] = color_purple_darker;
    style.Colors[ImGuiCol_HeaderHovered] = color_purple;
    style.Colors[ImGuiCol_Button] = color_purple_darker;
    style.Colors[ImGuiCol_ButtonHovered] = color_purple;
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.2f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.3f, 0.3f, 0.4f, 1.0f);
    style.Colors[ImGuiCol_Tab] = color_purple_darker;
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.8f, 0.8f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_TabActive] = color_purple;

    style.Colors[ImGuiCol_Tab] = ImVec4(0.25f, 0.25f, 0.35f, 1.0f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.4f, 0.4f, 0.5f, 1.0f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.733f, 0.698f, 0.914f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.2f, 0.2f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.5f, 0.5f, 0.7f, 1.0f);
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
}

// https://github.com/ocornut/imgui/wiki/Docking
// Creates required dockspace before rendering ImGui windows on top
// TODO: Can we add power save ? https://github.com/ocornut/imgui/wiki/Implementing-Power-Save,-aka-Idling-outside-of-ImGui
void drawGUIDockspace() {
    static bool is_fullscreen = true;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    ImGuiWindowFlags w_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    if (is_fullscreen) {
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

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

void drawGUI(const Camera& camera, float fps, float &particle_scale, bool &is_mainViewportHovered,
    MainScene &mainSceneFBO, MainScene &frameSceneFBO, shared_ptr<EventData> &evtData) {

    drawGUIDockspace();

    ImGui::Begin("Main Viewport");
        const glm::vec3 &cam_pos = camera.pos;
        
        // Have to do a bounding check to make sure the mouse is really within the actual main viewport img for cursor callbacks
        ImVec2 image_sz = ImGui::GetContentRegionAvail();
        ImVec2 fbo_imageSz = ImVec2(mainSceneFBO.getFBOwidth(), mainSceneFBO.getFBOheight());
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

        ImGui::Image((ImTextureID)mainSceneFBO.getColorTexture(), final_sz, ImVec2(0, 1), ImVec2(1, 0));
        is_mainViewportHovered = ImGui::IsItemHovered();
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

        updateFPS(fps);
        float avgFPS = calculateAverageFPS();
        float minFPS = getMinFPS();
        float maxFPS = getMaxFPS();

        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Avg FPS: %.1f", avgFPS);
        ImGui::Text("Min FPS: %.1f", minFPS);
        ImGui::Text("Max FPS: %.1f", maxFPS);
        ImGui::Separator();

        ImGui::PlotLines("##FPS History", fps_historyBuf.data(), fps_historyBuf.size(), fps_bufIdx, nullptr, 0.0f, maxFPS + 10.0f, ImVec2(0, 80));
        ImGui::Separator();

        ImGui::Text("Time Window (%.3f, %.3f)", evtData->getTimeWindow_L(), evtData->getTimeWindow_R());
        
        ImGui::SliderFloat("Left", &evtData->getTimeWindow_L(), evtData->getMinTimestamp(), evtData->getMaxTimestamp()); // TODO format
        ImGui::SliderFloat("Right", &evtData->getTimeWindow_R(), evtData->getMinTimestamp(), ceil(evtData->getMaxTimestamp()));
        ImGui::SliderFloat("##FrameLength", &evtData->getFrameLength(), 0, evtData->getMaxTimestamp()); 
        ImGui::SameLine();
        if (ImGui::Button("-")) { // TODO clean code
            evtData->getTimeWindow_L() = glm::max(evtData->getTimeWindow_L() - evtData->getFrameLength(), evtData->getMinTimestamp());
            evtData->getTimeWindow_R() = glm::max(evtData->getTimeWindow_R() - evtData->getFrameLength(), evtData->getMinTimestamp());
        } 
        ImGui::SameLine();
        if (ImGui::Button("+")) {
            evtData->getTimeWindow_L() = glm::min(evtData->getTimeWindow_L() + evtData->getFrameLength(), evtData->getMaxTimestamp());
            evtData->getTimeWindow_R() = glm::min(evtData->getTimeWindow_R() + evtData->getFrameLength(), evtData->getMaxTimestamp());
        }
        ImGui::SameLine();
        ImGui::Text("Frame Length (ms)");
        ImGui::Separator();

        ImGui::Text("Space Window");

        ImGui::SliderFloat("Top", &evtData->getSpaceWindow().x, evtData->getMin_XYZ().y, evtData->getMax_XYZ().y); 
        ImGui::SliderFloat("RightS", &evtData->getSpaceWindow().y, evtData->getMin_XYZ().x, ceil(evtData->getMax_XYZ().x));
        ImGui::SliderFloat("Bottom", &evtData->getSpaceWindow().z, evtData->getMin_XYZ().y, ceil(evtData->getMax_XYZ().y));
        ImGui::SliderFloat("LeftS", &evtData->getSpaceWindow().w, evtData->getMin_XYZ().x, evtData->getMax_XYZ().x); 

        ImGui::Separator();

        ImGui::Text("Processing options");
        ImGui::Checkbox("Morlet Shutter", &evtData->getMorlet()); // Todo add h and f sliders; Fix time normalizations
        ImGui::Checkbox("PCA", &evtData->getPCA());

    ImGui::End();

    ImGui::Begin("Frame");
        ImGui::Text("Digital Coded Exposure"); 

        // TODO ask Andrew about aspect ratio standards/preferences
        image_sz = ImGui::GetContentRegionAvail();
        final_sz = ImVec2(image_sz.x, image_sz.y); // fbo viewport is static ish
        ImGui::Image((ImTextureID)frameSceneFBO.getColorTexture(), final_sz);
    ImGui::End();
}

float randFloat() {
    return static_cast<float>(rand()) / RAND_MAX;
};

vec3 randXYZ() {
    return vec3(randFloat(), randFloat(), randFloat());
}