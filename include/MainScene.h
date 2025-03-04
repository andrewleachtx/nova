#pragma once
#include <GL/glew.h>

/*
    Basically just a wrapper class for a FBO (Framebuffer object) that allows us to
    render to a texture. We need this (AFAIK) so we can place the main viewport
    on the actual docked viewport in ImGui.

    This way main.cpp isn't flooded with more boilerplate.
*/
class MainScene {
public:
    MainScene();
    ~MainScene();

    bool initialize(int width, int height);

    void resize(int width, int height);

    void bind() const;

    void unbind() const;

    GLuint getColorTexture() const;
    GLuint getFBOwidth() const { return width; }
    GLuint getFBOheight() const { return height; }
    
private:
    GLuint fbo;
    GLuint colorTexture;
    GLuint depthRBO;
    int width;
    int height;
};