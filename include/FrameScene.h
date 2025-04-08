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
        autoUpdate(false), freq(0.01f), fps(0.0f) {}
    ~FrameScene() {}

    bool initialize(int width, int height) { return MainScene::initialize(width, height, true); };

    bool &isMorlet() { return morlet; }
    bool &getPCA() { return pca; }
    int &getAutoUpdate() { return autoUpdate; }
    float &getFreq() { return freq; }
    float &getUpdateFPS() { return fps; }
    float &getFramePeriod_T() { return framePeriod_T; }
    uint &getFramePeriod_E() { return framePeriod_E; }

    float getLastRenderTime() const { return lastRenderTime; } 
    void setLastRenderTime(float x) { lastRenderTime = x; } 

    void setEventAutoUpdate() {  }

    static const int MANUAL_UPDATE = 0;
    static const int EVENT_AUTO_UPDATE = 1;
    static const int TIME_AUTO_UPDATE = 2;

private:
    bool morlet;
    bool pca;
    int autoUpdate;
    float freq;
    float fps;
    float framePeriod_T;
    uint framePeriod_E;

    float lastRenderTime;
};