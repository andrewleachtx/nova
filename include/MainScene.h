#pragma once
#include <GL/glew.h>

/*
    Basically just a wrapper class for a FBO (Framebuffer object) that allows us to
    render to a texture. We need this (AFAIK) so we can place the main viewport
    on the actual docked viewport in ImGui.

    This way main.cpp isn't flooded with more boilerplate.
*/
// TODO: Best renamed to just FBOWrapper or FBO
class MainScene {
public:
    MainScene();
    ~MainScene();

    /**
     * @brief Initialize the FBO with the specifications of the GLFW frame.
     * @param width 
     * @param height 
     * @param frame 
     * @return bool true if successful
     */
    bool initialize(int width, int height, bool frame = false);

    /**
     * @brief Resizes / reinitializes with the callback on user resize
     * @param width 
     * @param height 
     * @param frame 
     */
    void resize(int width, int height, bool frame = false);

    /**
     * @brief Binds the FBO for rendering. This is necessary to render to the texture.
     */
    void bind() const;

    /**
     * @brief Unbinds the FBO. This is necessary to render to the screen.
     */
    void unbind() const;

    GLuint getColorTexture() const;
    GLuint getFBOwidth() const { return width; }
    GLuint getFBOheight() const { return height; }

    bool getDirtyBit() { return dirtyBit; }
    void setDirtyBit(bool x) { dirtyBit = x; }
    
private:
    GLuint fbo;
    GLuint colorTexture;
    GLuint depthRBO;
    int width;
    int height;

    bool dirtyBit;
};