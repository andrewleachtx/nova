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
    else if (action == GLFW_PRESS) {
        wc->freecam->keyDown(key);
    }
    else if (action == GLFW_RELEASE) {
        wc->freecam->keyUp(key);
    }

}

// This function is called when the mouse is clicked
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));

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
            wc->freecam->mouseClicked((float)xmouse, (float)ymouse, shift, ctrl, alt);

            if (button == GLFW_MOUSE_BUTTON_LEFT) {
            }
            else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            }
        }
	}
}

// This function is called when the mouse moves
void cursor_position_callback(GLFWwindow* window, double xmouse, double ymouse) {
    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    /* FIXME: I don't know if I need WantCaptureMouse here */
    // if (!(*(wc->is_cursorVisible)) && !ImGui::GetIO().WantCaptureMouse) {
    if (!(*(wc->is_cursorVisible))) {
        wc->freecam->mouseMoved((float)xmouse, (float)ymouse);
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
    wc->freecam->aspect = (float)width / (float)height;
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

ImGuiWindowFlags wflags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize; 
void drawGUI(const FreeCam& camera, float fps, float &particle_scale, int &focused_evt, size_t num_evts) {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::Begin("Debug", nullptr, wflags);
        const glm::vec3& cam_pos = camera.pos;
        ImGui::Text("Camera (World): (");
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextColored(IMCOLOR_RED, "%.3f, ", cam_pos.x);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextColored(IMCOLOR_GREEN, "%.3f, ", cam_pos.y);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextColored(IMCOLOR_BLUE, "%.3f", cam_pos.z);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::Text(")");

        const glm::vec3& cam_vel = camera.vel;
        ImGui::Text("Camera Speed: %.3f", glm::length(cam_vel));
        ImGui::Text("Current Time: %.3f", glfwGetTime());

        ImGui::SliderFloat("Particle Scale", &particle_scale, 0.1f, 2.5f);
    ImGui::End();

    // Top Right FPS Counter //
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 300, 0), ImGuiCond_Always);
    ImGui::Begin("FPS", nullptr, wflags);
        updateFPS(fps);
        float avgFPS = calculateAverageFPS();
        float minFPS = getMinFPS();
        float maxFPS = getMaxFPS();

        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Avg FPS: %.1f", avgFPS);
        ImGui::Text("Min FPS: %.1f", minFPS);
        ImGui::Text("Max FPS: %.1f", maxFPS);

        ImGui::PlotLines("##FPS History", fps_historyBuf.data(), fps_historyBuf.size(), fps_bufIdx, nullptr, 0.0f, maxFPS + 10.0f, ImVec2(0, 80));
    ImGui::End();

    // Event Selection //
    ImGui::SetNextWindowPos(ImVec2(0, 150), ImGuiCond_Always);
    ImGui::Begin("Event Selection", nullptr, wflags);
        ImGui::Text("Choose Event To Query:");
        if (ImGui::Button("Clear")) {
            focused_evt = -1;
        }
        if (num_evts > 0) {
            ImGui::SliderInt("Focused Event", &focused_evt, -1, static_cast<int>(num_evts) - 1);
        }
        else {
            ImGui::Text("No events loaded");
        }
    ImGui::End();
} 

float randFloat() {
    return static_cast<float>(rand()) / RAND_MAX;
};

vec3 randXYZ() {
    return vec3(randFloat(), randFloat(), randFloat());
}