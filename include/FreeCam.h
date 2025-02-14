#ifndef FREECAM_H
#define FREECAM_H

#include <memory>
#include <bitset>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

class MatrixStack;
using glm::vec2, glm::vec3, glm::mat4;

class FreeCam {
    public:
        FreeCam();
        ~FreeCam();

        void setInitPos(float x, float y, float z);
        void setForward(vec3 dir);
        vec3 calcForward() const;

        void mouseClicked(float x, float y, bool shift, bool ctrl, bool alt); // to avoid teleporting after moving mouse unclicked
        void mouseMoved(float x, float y);
        void keyDown(unsigned int key);
        void keyUp(unsigned int key);
        void update(float dt);

        mat4 calcLookAt() const;

        void applyProjectionMatrix(MatrixStack& P) const;
        void applyViewMatrix(MatrixStack& MV) const;
        void applyCameraMatrix(MatrixStack& MV) const;

        vec3 pos;
        vec3 vel;
        float yaw;
        float pitch;
        float aspect;
    private:
        float t_factor;
        float r_factor;
        float zoom_factor;

        float fovy;
        float znear;
        float zfar;
	    vec2 mousePrev;

        // Movement stuff
        std::bitset<GLFW_KEY_LAST + 1> keyStates;
        float acceleration;
        float deceleration;
        float maxSpeed;
};


#endif // FREECAM_H
