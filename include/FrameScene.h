#pragma once
#include "MainScene.h"
// #include <GL/glew.h>

/*
    Derived class of MainScene.
    Includes attributes and members (getters/setters) specifically relating to the digital 
    coded exposure viewport.
*/
class FrameScene: public MainScene {
public:
    FrameScene() : MainScene::MainScene(), morlet(false), pca(false),
        autoUpdate(false), freq(-1.0f), fps(-1.0f), lastRenderTime(-1.0f) {}
    ~FrameScene() {}

    bool initialize(int width, int height) { return MainScene::initialize(width, height, true); };

    bool &isMorlet() { return morlet; }
    bool &getPCA() { return pca; }
    bool &isAutoUpdate() { return autoUpdate; }
    float &getFreq() { return freq; }
    float &getFPS() { return fps; }
    float &getFramePeriod() { return framePeriod; }

    float getLastRenderTime() { return lastRenderTime; } 
    void setLastRenderTime(float x) { lastRenderTime = x; } 


private:
    bool morlet;
    bool pca;
    bool autoUpdate;
    float freq;
    float fps;
    float framePeriod;

    float lastRenderTime;
};