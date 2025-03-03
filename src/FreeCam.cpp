#define _USE_MATH_DEFINES
#include <cmath> 
#include <iostream>
#include <string>

#include "FreeCam.h"
#include "MatrixStack.h"

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

#include <bitset>

using std::cout, std::endl, std::string;
using glm::vec2, glm::vec3, glm::mat4;

// FIXME: Get rid of magic numbers
FreeCam::FreeCam() {
    aspect = 1.0f;
    fovy = (90.0f * (float)(M_PI / 180.0f));
    znear = 0.1f;
    zfar = 10'000.0f;

    t_factor = 1000.0f;
    r_factor = 0.001f;
    zoom_factor = 1.0f;

    pos = vec3(0.0f);
    // Yaw must be -pi so we "spawn in" looking down the negative z axis
    yaw = -(float)M_PI;
    pitch = 0.0f;
    mousePrev = vec2(0.0f);

    keyStates.reset();

    vel = vec3(0.0f);
    float CAM_SPEED_SCALE(150.0f);
    acceleration = 5.0f * CAM_SPEED_SCALE;
    deceleration = 4.5f * CAM_SPEED_SCALE;
    maxSpeed = 4.0f * CAM_SPEED_SCALE;
}
FreeCam::~FreeCam() {}

void FreeCam::setInitPos(float x, float y, float z) {
	pos = glm::vec3(x, y, z);
}

void FreeCam::setForward(glm::vec3 dir) {
    yaw = atan2(dir.z, dir.x);
    pitch = asin(dir.y);
}

vec3 FreeCam::calcForward() const {
    float x, y, z;
    x = (float)(cos(yaw) * cos(pitch));
    y = (float)(sin(pitch));
    z = (float)(sin(yaw) * cos(pitch));

    return vec3(x, y, z);
}

void FreeCam::mouseClicked(float x, float y, bool shift, bool ctrl, bool alt) {
	mousePrev.x = x;
	mousePrev.y = y;
}

void FreeCam::mouseMoved(float x, float y) {
	glm::vec2 mouseCurr(x, y);
	glm::vec2 dv = mouseCurr - mousePrev;

	// yaw changes per dx
	yaw += r_factor * dv.x;
    // There is a random dv.x that is extremely massive

    // pitch changes per dy, enforce [-90, 90] limit
	pitch -= r_factor * dv.y;
	pitch = std::clamp(pitch, -(float)(89.0f * M_PI / 180.0f), (float)(89.0f * M_PI / 180.0f));

	mousePrev = mouseCurr;
}

void FreeCam::keyDown(unsigned int key) {
    if (key <= GLFW_KEY_LAST) {
        keyStates.set(key);
    }
}

void FreeCam::keyUp(unsigned int key) {
    if (key <= GLFW_KEY_LAST) {
        keyStates.reset(key);
    }
}


void FreeCam::update(float dt) {
    glm::vec3 moveDirection(0.0f);

    glm::vec3 forward = calcForward();
    
    // No pitch anymore (minecraft moment)
    forward.y = 0.0f;
    forward = glm::normalize(forward);

    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    if (keyStates.test(GLFW_KEY_W)) {
        moveDirection += forward;
    }
    if (keyStates.test(GLFW_KEY_S)) {
        moveDirection -= forward;
    }
    if (keyStates.test(GLFW_KEY_A)) {
        moveDirection -= right;
    }
    if (keyStates.test(GLFW_KEY_D)) {
        moveDirection += right;
    }
    if (keyStates.test(GLFW_KEY_SPACE)) {
        moveDirection += up;
    }
    if (keyStates.test(GLFW_KEY_LEFT_SHIFT)) {
        moveDirection -= up;
    }

    if (keyStates.test(GLFW_KEY_Z)) {
        fovy -= zoom_factor * dt;
    }
    if (keyStates.test(GLFW_KEY_X)) {
        fovy += zoom_factor * dt;
    }
    fovy = glm::clamp(fovy, glm::radians(4.0f), glm::radians(114.0f));

    if (glm::length(moveDirection) > 0.0f) {
        moveDirection = glm::normalize(moveDirection);

        vel += moveDirection * acceleration * dt;

        if (glm::length(vel) > maxSpeed) {
            vel = glm::normalize(vel) * maxSpeed;
        }
    } else {
        if (glm::length(vel) > 0.0f) {
            glm::vec3 decel = glm::normalize(vel) * deceleration * dt;
            if (glm::length(decel) > glm::length(vel)) {
                vel = glm::vec3(0.0f);
            } else {
                vel -= decel;
            }
        }
    }

    // Update position based on velocity
    pos += vel * dt;
}

glm::mat4 FreeCam::calcLookAt() const {
	glm::vec3 eye = pos;
    glm::vec3 forward = calcForward();
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

	return glm::lookAt(eye, eye + forward, up);
}

void FreeCam::applyProjectionMatrix(MatrixStack& P) const {
	P.multMatrix(glm::perspective(fovy, aspect, znear, zfar));
}

void FreeCam::applyViewMatrix(MatrixStack& MV) const {
    MV.multMatrix(calcLookAt());
}

void FreeCam::applyCameraMatrix(MatrixStack& MV) const {
	MV.multMatrix(glm::inverse(calcLookAt()));
}