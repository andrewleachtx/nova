#pragma once
#ifndef UTILS_H
#define UTILS_H

#include "include.h"

#include <glm/glm.hpp>
#include <string>
using std::string;

class Program;

struct WindowContext {
    FreeCam* freecam;
    bool* is_cursorVisible;
    bool* key_toggles;
};

Program genPhongProg(string resource_dir);
void sendToPhongShader(const Program& prog, const MatrixStack& P, const MatrixStack& MV, const vec3& lightPos, const vec3& lightCol, const BPMaterial& mat);

/* GLFW Callbacks */
void error_callback(int error, const char *description);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
void cursor_position_callback(GLFWwindow* window, double xmouse, double ymouse);
void char_callback(GLFWwindow *window, unsigned int key);
void resize_callback(GLFWwindow *window, int width, int height);
bool genBiggestWindow(GLFWwindow*& window, string window_name);

void drawGUI(const FreeCam &camera, float fps);
float randFloat();
glm::vec3 randXYZ();

#endif // UTILS_H