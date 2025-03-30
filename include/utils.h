#pragma once
#ifndef UTILS_H
#define UTILS_H

#include "include.h"

#include <glm/glm.hpp>
#include <string>
#include <memory>

class Program;
class MainScene;
class EventData;

struct WindowContext {
    Camera* camera;
    bool* is_cursorVisible;
    bool* key_toggles;
    bool* is_mainViewportHovered;
    MainScene* mainSceneFBO;
    MainScene* frameSceneFBO;
};
std::string OpenFileDialog();

Program genPhongProg(const std::string &resource_dir);
void sendToPhongShader(const Program &prog, const MatrixStack &P, const MatrixStack &MV, const vec3 &lightPos, const vec3 &lightCol, const BPMaterial &mat);

Program genBasicProg(const std::string &resource_dir);

// GLFW Callbacks //
void error_callback(int error, const char *description);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void cursor_position_callback(GLFWwindow *window, double xmouse, double ymouse);
void char_callback(GLFWwindow *window, unsigned int key);
void resize_callback(GLFWwindow *window, int width, int height);
bool genBiggestWindow(GLFWwindow *&window, const std::string &window_name="GLFW Window");

// ImGui //
void initImGuiStyle(ImGuiStyle &style);
void drawGUIDockspace();
void drawGUI(const Camera& camera, float fps, float &particle_scale, bool &is_mainViewportHovered,
    MainScene &mainSceneFBO, FrameScene &frameScenceFBO, std::shared_ptr<EventData> &evtData, std::string &datafilepath, 
    std::string &video_name, bool &recording);

float randFloat();
glm::vec3 randXYZ();

// Helpers & Debug //
#define printvec3(var) pv3(#var, var)
inline void pv3(const char *varname, const glm::vec3 &vec) {
    printf("%s: %f, %f, %f\n", varname, vec.x, vec.y, vec.z);
}

#define printmat3(var) pm3(#var, var)
inline void pm3(const char *varname, const glm::mat3 &mat) {
    printf("%s:\n", varname);
    printf("%f, %f, %f\n", mat[0][0], mat[0][1], mat[0][2]);
    printf("%f, %f, %f\n", mat[1][0], mat[1][1], mat[1][2]);
    printf("%f, %f, %f\n", mat[2][0], mat[2][1], mat[2][2]);
}


#endif // UTILS_H