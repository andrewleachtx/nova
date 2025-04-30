/*
    Original code from Dr. Shinjiro Sueda's computer graphics and animation courses.

    Retrieved and modified by Andrew Leach, 2025
*/
#pragma once
#ifndef CAMERA_H
#define CAMERA_H

#include <memory>
#include <bitset>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

class MatrixStack;


/**
 * @brief Handler class for movement, modelview and projection matrices, as well as user event callbacks.
 */
class Camera {
    enum {
		ROTATE = 0,
		TRANSLATE,
		SCALE
	};
	
    public:
        Camera();
        ~Camera();
        
        void setEvtCenter(const glm::vec3 &center);
        void setInitPos(float x, float y, float z);
        void setForward(const glm::vec3 &dir);
        glm::vec3 calcForward() const;

        void zoom(float amt);
        void mouseClicked(float x, float y, bool shift, bool ctrl, bool alt);
        void mouseMoved(float x, float y);

        glm::mat4 calcLookAt() const;

        void applyProjectionMatrix(MatrixStack& P) const;
        void applyOrthoMatrix(MatrixStack& P) const;
        void applyViewMatrix(MatrixStack& MV) const;
        void applyCameraMatrix(MatrixStack& MV) const;

        glm::vec3 pos;
        float yaw;
        float pitch;
        float aspect;
        glm::vec3 translations;
    private:
        float t_factor;
        float r_factor;
        float zoom_factor;
        float scroll_zoom_factor;

        float fovy;
        float znear;
        float zfar;

        glm::vec2 rotations;
        // glm::vec3 translations;
        glm::vec2 mousePrev;
        
        // Center of bounding box
        glm::vec3 evt_center;

        int state;
};


#endif // CAMERA_H
