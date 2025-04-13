#define _USE_MATH_DEFINES
#include <cmath> 
#include <iostream>
#include <string>

#include "Camera.h"
#include "MatrixStack.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

#include <bitset>

using std::cout, std::endl, std::string;

Camera::Camera() {
    yaw = -(float)M_PI;
    pitch = 0.0f;
    aspect = 1.0f;
    
    t_factor = 0.001f;
    r_factor = 0.01f;
    zoom_factor = 0.01f;
    scroll_zoom_factor = 0.1f;

    fovy = (45.0f * (float)(M_PI / 180.0f));
    znear = 5.0f;
    zfar = 100'000.0f;
    
    rotations = glm::vec2(0.0f);
    translations = glm::vec3(0.0f, 0.0f, -10.0f);
    mousePrev = glm::vec2(0.0f);

    evt_center = glm::vec3(0.0f);
}
Camera::~Camera() {}

void Camera::setEvtCenter(const glm::vec3 &center) {
    evt_center = center;
}

void Camera::setInitPos(float x, float y, float z) {
	pos = glm::vec3(x, y, z);
    translations = -pos;
}

void Camera::setForward(const glm::vec3 &dir) {
    yaw = atan2(dir.z, dir.x);
    pitch = asin(dir.y);
}

glm::vec3 Camera::calcForward() const {
    float x, y, z;
    x = (float)(cos(yaw) * cos(pitch));
    y = (float)(sin(pitch));
    z = (float)(sin(yaw) * cos(pitch));

    return glm::vec3(x, y, z);
}

void Camera::zoom(float amt) {
    translations.z *= (1.0f - scroll_zoom_factor * amt);
    pos -= glm::vec3(translations);
}

void Camera::mouseClicked(float x, float y, bool shift, bool ctrl, bool alt) {
	mousePrev.x = x;
	mousePrev.y = y;

    if (shift) {
		state = Camera::TRANSLATE;
	}
    else if(ctrl) {
		state = Camera::SCALE;
	}
    else {
		state = Camera::ROTATE;
	}
}

void Camera::mouseMoved(float x, float y)
{
	glm::vec2 mouseCurr(x, y);
	glm::vec2 dv = mouseCurr - mousePrev;
	switch (state) {
		case Camera::ROTATE : {
			rotations += r_factor * dv;
			break;
        }
		case Camera::TRANSLATE : {
			translations.x -= translations.z * t_factor * dv.x;
			translations.y += translations.z * t_factor * dv.y;
            break;
        }
		case Camera::SCALE : {
			translations.z *= (1.0f - zoom_factor * dv.y);
			break;
        }
	}

    pos = -glm::vec3(translations);
	mousePrev = mouseCurr;
}

glm::mat4 Camera::calcLookAt() const {
	glm::vec3 eye = pos;
    glm::vec3 forward = calcForward();
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

	return glm::lookAt(eye, eye + forward, up);
}

void Camera::applyProjectionMatrix(MatrixStack& P) const {
	P.multMatrix(glm::perspective(fovy, aspect, znear, zfar));
}

void Camera::applyViewMatrix(MatrixStack& MV) const {
    MV.translate(evt_center);
    MV.translate(translations);
	MV.rotate(rotations.y, glm::vec3(1.0f, 0.0f, 0.0f));
	MV.rotate(rotations.x, glm::vec3(0.0f, 1.0f, 0.0f));
    MV.translate(-evt_center);
}

void Camera::applyCameraMatrix(MatrixStack& MV) const {
	MV.multMatrix(glm::inverse(calcLookAt()));
}