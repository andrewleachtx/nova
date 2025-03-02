#pragma once
#ifndef UTILS_H
#define UTILS_H

#include "include.h"

#include <glm/glm.hpp>
#include <string>

class Program;

struct WindowContext {
    Camera* camera;
    bool* is_cursorVisible;
    bool* key_toggles;
};

Program genPhongProg(const std::string &resource_dir);
void sendToPhongShader(const Program &prog, const MatrixStack &P, const MatrixStack &MV, const vec3 &lightPos, const vec3 &lightCol, const BPMaterial &mat);

/* GLFW Callbacks */
void error_callback(int error, const char *description);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
void cursor_position_callback(GLFWwindow* window, double xmouse, double ymouse);
void char_callback(GLFWwindow *window, unsigned int key);
void resize_callback(GLFWwindow *window, int width, int height);
bool genBiggestWindow(GLFWwindow *&window, const std::string &window_name="GLFW Window");

void drawGUI(const Camera& camera, float fps, float &particle_scale, int &focused_evt, size_t num_evts);

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