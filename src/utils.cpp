#include "utils.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include "include.h"

#include <memory>
#include <iostream>
#include <vector>
#include <string>
#include <random>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Windows.h>
#include <shobjidl.h>
#include <fstream>

using std::cout, std::endl, std::cerr;
using std::shared_ptr, std::make_shared;
using std::vector, std::string;
using glm::vec3;

static const vector<int> unitConversions({1000000000, 1000, 1}); // aedat data is in us
static const vector<string> timeUnits({"(s)", "(ms)", "(us)"});
static vector<string> unitLabels;
static enum unitLabelsIndex {
    timeWindow,
    framePeriod,
    shutterInitial,
    shutterFinal,
    FWHM
};

void initLabels() {
    unitLabels = vector<string>({
        "Time Window [%.3f, %.3f] (",
        "Frame Period (",
        "Shutter Initial (",
        "Shutter Final (",
        "Full Width at Half Measure (",
    });
}

void updateUnitLabels(string formattedUnit) {
    for (auto &elem : unitLabels) {
        elem.replace(elem.find('('), formattedUnit.size(), formattedUnit);    
    }
}



// FIXME: Doesn't throw an error when file doesn't exist
// FIXME: Remove the argc / argv that takes in a file
string OpenFileDialog(string& initialDirectory) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) return "";

    IFileOpenDialog* pFileOpen;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    if (FAILED(hr)) {
        CoUninitialize();
        return "";
    }
    
    //Sets file filter
    const COMDLG_FILTERSPEC fileTypes[] = {
        {L"AEDAT4 Files", L"*.aedat4"}
    };
    hr = pFileOpen->SetFileTypes(1, fileTypes);
    hr = pFileOpen->SetTitle(L"Select a data AEDAT4 File");

    // Set the initial directory
    IShellItem* pInitialFolder = nullptr;
    std::wstring winitialDirectory = std::wstring(initialDirectory.begin(),initialDirectory.end());
    hr = SHCreateItemFromParsingName(winitialDirectory.c_str(), nullptr, IID_PPV_ARGS(&pInitialFolder));
    if (SUCCEEDED(hr)) {
        pFileOpen->SetDefaultFolder(pInitialFolder);
        pInitialFolder->Release();
    }

    hr = pFileOpen->Show(NULL);
    if (SUCCEEDED(hr)) {
        IShellItem* pItem;
        hr = pFileOpen->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            PWSTR filePath;
            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
            if (SUCCEEDED(hr)) {
                std::wstring ws(filePath);
                string path(ws.begin(), ws.end());
                CoTaskMemFree(filePath);
                pItem->Release();
                pFileOpen->Release();
                CoUninitialize();
                return path;
            }
        }
    }
    pFileOpen->Release();
    CoUninitialize();
    return "";
}

bool isValidFilePath(string filePath) {
    // Check if the file exists
    std::ifstream file(filePath);
    if (!file) {
        return false;
    }

    // Check if the file extension is ".aedat4"
    size_t extensionPos = filePath.find_last_of('.');
    if (extensionPos == string::npos || filePath.substr(extensionPos) != ".aedat4") {
        return false;
    }

    return true;
}

Program genPhongProg(const string &resource_dir) {
    Program prog = Program();
    prog.setShaderNames(resource_dir + "phong.vsh", resource_dir + "phong.fsh");
    prog.setVerbose(true);
    prog.init();

    prog.addUniform("P");
    prog.addUniform("MV");
    prog.addUniform("MV_it");

    prog.addAttribute("aPos");
    prog.addAttribute("aNor");
    prog.addAttribute("aTex");

    prog.addUniform("lightPos");
    prog.addUniform("lightCol");
    prog.addUniform("ka");
    prog.addUniform("kd");
    prog.addUniform("ks");
    prog.addUniform("s");

    // prog.setVerbose(false);

    return prog;
}

Program genInstProg(const std::string &resource_dir) {
    Program prog = Program();
    prog.setShaderNames(resource_dir + "phong_inst.vsh", resource_dir + "phong_inst.fsh");
    prog.setVerbose(true);
    prog.init();

    prog.addUniform("P");
    prog.addUniform("MV");
    prog.addUniform("MV_it");
    
    prog.addUniform("particleScale");

    prog.addUniform("negColor");
    prog.addUniform("posColor");

    prog.addAttribute("aPos");
    prog.addAttribute("aNor");
    prog.addAttribute("aTex");
    prog.addAttribute("aInstPos"); // We additionally require a position matrix per vertex for instancing

    return prog;
}

Program genBasicProg(const string &resource_dir) {
    Program prog = Program();
    prog.setShaderNames(resource_dir + "basic.vsh", resource_dir + "basic.fsh");
    prog.setVerbose(true);
    prog.init();

    prog.addAttribute("pos");
    prog.addUniform("projection");

    // prog.setVerbose(false);

    return prog;
}

void sendToPhongShader(const Program& prog, const MatrixStack& P, const MatrixStack& MV, const vec3& lightPos, const vec3& lightCol, const BPMaterial& mat) {
    glUniformMatrix4fv(prog.getUniform("P"), 1, GL_FALSE, glm::value_ptr(P.topMatrix()));
    glUniformMatrix4fv(prog.getUniform("MV"), 1, GL_FALSE, glm::value_ptr(MV.topMatrix()));
    glUniformMatrix4fv(prog.getUniform("MV_it"), 1, GL_FALSE, glm::value_ptr(glm::inverse(glm::transpose(MV.topMatrix()))));

    glUniform3fv(prog.getUniform("lightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(prog.getUniform("lightCol"), 1, glm::value_ptr(lightCol));

    glUniform3fv(prog.getUniform("ka"), 1, glm::value_ptr(mat.ka));
    glUniform3fv(prog.getUniform("kd"), 1, glm::value_ptr(mat.kd));
    glUniform3fv(prog.getUniform("ks"), 1, glm::value_ptr(mat.ks));
    glUniform1f(prog.getUniform("s"), mat.s);
}

// This function is called when a GLFW error occurs
void error_callback(int error, const char *description) {
    cerr << "Error: " << description << endl;
}

// This function is called when a key is pressed
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));

    if (ImGui::GetIO().WantCaptureKeyboard) {
        return;
    }

    if (key == GLFW_KEY_ESCAPE) {
        if (action == GLFW_PRESS) {
            if (*(wc->is_cursorVisible)) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            *(wc->is_cursorVisible) = !*(wc->is_cursorVisible);
        }
        else if (action == GLFW_REPEAT) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }
    }
}

// This function is called when the mouse is clicked
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    // if (ImGui::GetIO().WantCaptureMouse) {
    //     return;
    // }

    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));

    if (!*(wc->is_mainViewportHovered)) {
        return;
    }

	double xmouse, ymouse;
	glfwGetCursorPos(window, &xmouse, &ymouse);

	int width, height;
	glfwGetWindowSize(window, &width, &height);
	
    if (action == GLFW_PRESS) {
		bool shift = (mods & GLFW_MOD_SHIFT) != 0;
		bool ctrl  = (mods & GLFW_MOD_CONTROL) != 0;
		bool alt   = (mods & GLFW_MOD_ALT) != 0;

        if (!*(wc->is_cursorVisible)) {
            /* TODO: DO WE NEED THIS? */
            wc->camera->mouseClicked((float)xmouse, (float)ymouse, shift, ctrl, alt);

            if (button == GLFW_MOUSE_BUTTON_LEFT) {
            }
            else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            }
        }
	}
}

// Mouse scroll
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));

    if (!*(wc->is_mainViewportHovered)) {
        return;
    }

    wc->camera->zoom(yoffset);
}

// This function is called when the mouse moves
void cursor_position_callback(GLFWwindow* window, double xmouse, double ymouse) {
    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));

    if (!*(wc->is_mainViewportHovered)) {
        return;
    }

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    if (state == GLFW_PRESS && !(*(wc->is_cursorVisible))) {
        wc->camera->mouseMoved((float)xmouse, (float)ymouse);
    }
}

// When a key is pressed
void char_callback(GLFWwindow *window, unsigned int key)
{
    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));
    wc->key_toggles[key] = !wc->key_toggles[key];

    if (ImGui::GetIO().WantCaptureKeyboard) {
        return;
    }
}

// If the window is resized, capture the new size and reset the viewport
void resize_callback(GLFWwindow *window, int width, int height) {
    WindowContext* wc = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));

	glViewport(0, 0, width, height);

    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }
    
    // Update the aspect ratio of the camera
    wc->camera->aspect = (float)width / (float)height;

    // Update the FBO
    wc->mainSceneFBO->resize(width, height);
    wc->frameSceneFBO->resize(width, height, true);
    wc->mainSceneFBO->setDirtyBit(true);
    wc->frameSceneFBO->setDirtyBit(true);
}

// Looks for the biggest monitor
bool genBiggestWindow(GLFWwindow *&window, const string &window_name) {
    int num_monitors;
    GLFWmonitor** monitors = glfwGetMonitors(&num_monitors);

    int max_area = 0;
    GLFWmonitor* biggest_monitor = nullptr;

    for (int i = 0; i < num_monitors; i++) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        int area = mode->width * mode->height;
        if (area > max_area) {
            max_area = area;
            biggest_monitor = monitors[i];
        }
    }

    if (biggest_monitor != nullptr) {
        const GLFWvidmode* mode = glfwGetVideoMode(biggest_monitor);
        window = glfwCreateWindow(mode->width, mode->height, window_name.c_str(), biggest_monitor, nullptr);

        return true;
    }

    return false;
}

ImVec4 IMCOLOR_RED(1.0f, 0.0f, 0.0f, 1.0f);
ImVec4 IMCOLOR_GREEN(0.0f, 1.0f, 0.0f, 1.0f);
ImVec4 IMCOLOR_BLUE(0.0f, 0.5f, 1.0f, 1.0f);

static const size_t fps_bufferSz = 100;
static std::vector<float> fps_historyBuf(fps_bufferSz, 0.0f);
static size_t fps_bufIdx = 0;

void updateFPS(const float& fps) {
    fps_historyBuf[fps_bufIdx] = fps;
    fps_bufIdx = (fps_bufIdx + 1) % fps_bufferSz;
}

float calculateAverageFPS() {
    float sum = 0.0f;
    for (const float& fps : fps_historyBuf) {
        sum += fps;
    }
    return sum / fps_bufferSz;
}

float getMinFPS() {
    float min = fps_historyBuf[0];
    for (const float& fps : fps_historyBuf) {
        if (fps < min) {
            min = fps;
        }
    }
    return min;
}

float getMaxFPS() {
    float max = fps_historyBuf[0];
    for (const float& fps : fps_historyBuf) {
        if (fps > max) {
            max = fps;
        }
    }
    return max;
}

void initImGuiStyle(ImGuiStyle &style) {
    ImVec4 color_purple = ImVec4(0.733f, 0.698f, 0.914f, 1.0f);
    ImVec4 color_purple_darker = ImVec4(0.5f, 0.45f, 0.7f, 1.0f);
    ImVec4 color_background = ImVec4(0.12f, 0.12f, 0.18f, 1.0f);
    ImVec4 color_text = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
            
    style.Colors[ImGuiCol_WindowBg] = color_background;
    style.Colors[ImGuiCol_Text] = color_text;
    style.Colors[ImGuiCol_TitleBg] = color_purple_darker;
    style.Colors[ImGuiCol_TitleBgActive] = color_purple;
    style.Colors[ImGuiCol_Header] = color_purple_darker;
    style.Colors[ImGuiCol_HeaderHovered] = color_purple;
    style.Colors[ImGuiCol_Button] = color_purple_darker;
    style.Colors[ImGuiCol_ButtonHovered] = color_purple;
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.2f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.3f, 0.3f, 0.4f, 1.0f);
    style.Colors[ImGuiCol_Tab] = color_purple_darker;
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.8f, 0.8f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_TabActive] = color_purple;

    style.Colors[ImGuiCol_Tab] = ImVec4(0.25f, 0.25f, 0.35f, 1.0f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.4f, 0.4f, 0.5f, 1.0f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.733f, 0.698f, 0.914f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.2f, 0.2f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.5f, 0.5f, 0.7f, 1.0f);
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
}

// https://github.com/ocornut/imgui/wiki/Docking
// Creates required dockspace before rendering ImGui windows on top
// TODO: Can we add power save ? https://github.com/ocornut/imgui/wiki/Implementing-Power-Save,-aka-Idling-outside-of-ImGui
void drawGUIDockspace() {
    static bool is_fullscreen = true;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    ImGuiWindowFlags w_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    if (is_fullscreen) {
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        w_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    }

    ImGui::Begin("DockSpace", nullptr, w_flags);
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("DockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
    ImGui::End();
}

// Helper functions for DrawGUI
static void inputTextWrapper(std::string &name) {
    const unsigned int max_length = 50; // TODO document range and decide if reasonable

    char buf[max_length];
    memset(buf, 0, max_length);
    memcpy(buf, name.c_str(), name.size());
    ImGui::InputText("Video Output Name", buf, max_length);
    name = buf;
}

static void timeWindowWrapper(bool &dTimeWindow, shared_ptr<EventData> &evtData, FrameScene &frameSceneFBO) {
    ImGui::Text(unitLabels[timeWindow].c_str(), evtData->getMinTimestamp(), evtData->getMaxTimestamp());
        
    dTimeWindow |= ImGui::SliderFloat("Initial Time", &evtData->getTimeWindow_L(), evtData->getMinTimestamp(), evtData->getMaxTimestamp(), "%.4f");
    dTimeWindow |= ImGui::SliderFloat("Final Time", &evtData->getTimeWindow_R(), evtData->getMinTimestamp(), evtData->getMaxTimestamp(), "%.4f");
    ImGui::SliderFloat("##FramePeriod_Time", &frameSceneFBO.getFramePeriod_T(), 0, evtData->getMaxTimestamp(), "%.4f"); 

    if (ImGui::Button("-##time")) { 
        dTimeWindow = true;
        evtData->getTimeWindow_L() = evtData->getTimeWindow_L() - frameSceneFBO.getFramePeriod_T();
        evtData->getTimeWindow_R() = evtData->getTimeWindow_R() - frameSceneFBO.getFramePeriod_T();
    } 
    ImGui::SameLine();
    if (ImGui::Button("+##time")) {
        dTimeWindow = true;
        evtData->getTimeWindow_L() = evtData->getTimeWindow_L() + frameSceneFBO.getFramePeriod_T();
        evtData->getTimeWindow_R() = evtData->getTimeWindow_R() + frameSceneFBO.getFramePeriod_T();
    }
    ImGui::SameLine();
    ImGui::Text(unitLabels[framePeriod].c_str());
    ImGui::Separator();

    evtData->getTimeWindow_L() = std::clamp(evtData->getTimeWindow_L(), evtData->getMinTimestamp(), evtData->getMaxTimestamp());
    evtData->getTimeWindow_R() = std::clamp(evtData->getTimeWindow_R(), evtData->getTimeWindow_L(), evtData->getMaxTimestamp());
    frameSceneFBO.getFramePeriod_T() = std::max(frameSceneFBO.getFramePeriod_T(), 0.0f);
}

static void eventWindowWrapper(bool &dEventWindow, shared_ptr<EventData> &evtData, FrameScene &frameSceneFBO) {
    ImGui::Text("Event Window [%d, %d]", 0, evtData->getMaxEvent() - 1);
        
    dEventWindow |= ImGui::SliderInt("Initial Event", (int *) &evtData->getEventWindow_L(), 0, evtData->getMaxEvent() - 1);
    dEventWindow |= ImGui::SliderInt("Final Event", (int *) &evtData->getEventWindow_R(), 0, evtData->getMaxEvent() - 1);
    ImGui::SliderInt("##FramePeriod_Event", (int *) &frameSceneFBO.getFramePeriod_E(), 0, evtData->getMaxEvent() - 1); 

    if (ImGui::Button("-##event")) {
        dEventWindow = true;
        evtData->getEventWindow_L() = evtData->getEventWindow_L() - frameSceneFBO.getFramePeriod_E();
        evtData->getEventWindow_R() = evtData->getEventWindow_R() - frameSceneFBO.getFramePeriod_E();
    }
    ImGui::SameLine();
    if (ImGui::Button("+##event")) {
        dEventWindow = true;
        evtData->getEventWindow_L() = evtData->getEventWindow_L() + frameSceneFBO.getFramePeriod_E();
        evtData->getEventWindow_R() = evtData->getEventWindow_R() + frameSceneFBO.getFramePeriod_E();
    }
    ImGui::SameLine();
    ImGui::Text("Frame Period (events)");
    ImGui::Separator();

    evtData->getEventWindow_L() = std::clamp(evtData->getEventWindow_L(), (uint) 0, evtData->getMaxEvent() - 1);
    evtData->getEventWindow_R() = std::clamp(evtData->getEventWindow_R(), evtData->getEventWindow_L(), evtData->getMaxEvent() - 1);
    frameSceneFBO.getFramePeriod_E() = std::max(frameSceneFBO.getFramePeriod_E(), (uint) 0);
}

static void spaceWindowWrapper(bool &dSpaceWindow, shared_ptr<EventData> &evtData) {
    ImGui::Text("Space Window");
    dSpaceWindow |= ImGui::SliderFloat("Top", &evtData->getSpaceWindow().x, evtData->getMin_XYZ().y, evtData->getMax_XYZ().y); 
    dSpaceWindow |= ImGui::SliderFloat("Right", &evtData->getSpaceWindow().y, evtData->getMin_XYZ().x, evtData->getMax_XYZ().x);
    dSpaceWindow |= ImGui::SliderFloat("Bottom", &evtData->getSpaceWindow().z, evtData->getMin_XYZ().y, evtData->getMax_XYZ().y);
    dSpaceWindow |= ImGui::SliderFloat("Left", &evtData->getSpaceWindow().w, evtData->getMin_XYZ().x, evtData->getMax_XYZ().x); 
    ImGui::Separator();

    evtData->getSpaceWindow().x = std::clamp(evtData->getSpaceWindow().x, evtData->getMin_XYZ().y, evtData->getMax_XYZ().y); 
    evtData->getSpaceWindow().y = std::clamp(evtData->getSpaceWindow().y, evtData->getMin_XYZ().x, evtData->getMax_XYZ().x);
    evtData->getSpaceWindow().z = std::clamp(evtData->getSpaceWindow().z, evtData->getMin_XYZ().y, evtData->getMax_XYZ().y);
    evtData->getSpaceWindow().w = std::clamp(evtData->getSpaceWindow().w, evtData->getMin_XYZ().x, evtData->getMax_XYZ().x); 
}

void drawGUI(const Camera& camera, float fps, float &particle_scale, bool &is_mainViewportHovered,
    MainScene &mainSceneFBO, FrameScene &frameSceneFBO, shared_ptr<EventData> &evtData, std::string& datafilepath,
    std::string &video_name, bool &recording, std::string& datadirectory, bool &loadFile) {

    drawGUIDockspace();

    if (unitLabels.size() == 0) {
        initLabels();
        updateUnitLabels(timeUnits[evtData->getUnitType()]);
        EventData::TIME_CONVERSION = unitConversions[evtData->getUnitType()];
    }

    // Dirty bits
    bool dFile = false;
    bool dTimeWindow = false;
    bool dEventWindow = false;
    bool dSpaceWindow = false;
    bool dProcessingOptions = false;
    ImGui::Begin("Main Viewport");
        const glm::vec3 &cam_pos = camera.pos;
        
        // Have to do a bounding check to make sure the mouse is really within the actual main viewport img for cursor callbacks
        ImVec2 image_sz = ImGui::GetContentRegionAvail();
        ImVec2 fbo_imageSz = ImVec2(mainSceneFBO.getFBOwidth(), mainSceneFBO.getFBOheight());
        float img_aspect = image_sz.x / image_sz.y;
        float fbo_aspect = fbo_imageSz.x / fbo_imageSz.y;

        ImVec2 final_sz;
        if (fbo_aspect > img_aspect) {
            // Width is fixed, adjust height
            final_sz = ImVec2(image_sz.x, image_sz.x / fbo_aspect);
        } else {
            // Height is fixed, adjust width
            final_sz = ImVec2(image_sz.y * fbo_aspect, image_sz.y);
        }

        ImGui::Image((ImTextureID)mainSceneFBO.getColorTexture(), final_sz, ImVec2(0, 1), ImVec2(1, 0));
        is_mainViewportHovered = ImGui::IsItemHovered();
    ImGui::End();

    ImGui::Begin("Load");
        ImGui::Text("File:");

        if (ImGui::Button("Open File")) {
            dFile = true;
            string newFilePath=OpenFileDialog(datadirectory);
            if(isValidFilePath(newFilePath)){
                loadFile = true;
                datafilepath=std::move(newFilePath);
            }
        }
        ImGui::Text("Event Frequency");    
        ImGui::SliderInt("##modFreq", (int *) &EventData::modFreq, 1, 1000);
        EventData::modFreq = std::max((uint) 1, EventData::modFreq);


        // TODO: Cache recent files and state?
    ImGui::End();

    ImGui::Begin("Info");
        ImGui::Text("Camera (World): (%.3f, %.3f, %.3f)", cam_pos.x, cam_pos.y, cam_pos.z);
        ImGui::Separator();
        ImGui::SliderFloat("Particle Scale", &particle_scale, 0.1f, 2.5f);
        ImGui::Separator();
        ImGui::ColorEdit3("Negative Polarity Color", (float *) &evtData->getNegColor());
        ImGui::ColorEdit3("Positive Polarity Color", (float *) &evtData->getPosColor());
        ImGui::Separator();
        dProcessingOptions |= ImGui::SliderFloat("Event Contribution Weight", &BaseFunc::contribution, 0.0f, 1.0f);
        ImGui::Separator();
        if (ImGui::Combo("Time Unit", &evtData->getUnitType(), "s\0ms\0us\0")) {
            dProcessingOptions = true;
            EventData::TIME_CONVERSION = unitConversions[evtData->getUnitType()];
            updateUnitLabels(timeUnits[evtData->getUnitType()]);
        }
        ImGui::Separator();

        // FPS
        updateFPS(fps);
        float avgFPS = calculateAverageFPS();
        float minFPS = getMinFPS();
        float maxFPS = getMaxFPS();

        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Avg FPS: %.1f", avgFPS);
        ImGui::Text("Min FPS: %.1f", minFPS);
        ImGui::Text("Max FPS: %.1f", maxFPS);
        ImGui::Separator();
        ImGui::PlotLines("##FPS History", fps_historyBuf.data(), static_cast<int>(fps_historyBuf.size()), static_cast<int>(fps_bufIdx), nullptr, 0.0f, maxFPS + 10.0f, ImVec2(0, 80));
        ImGui::Separator();

        // Windows
        float normFactor = evtData->getDiffScale() * EventData::TIME_CONVERSION;
        evtData->oddizeTime();
        frameSceneFBO.oddizeTime(normFactor);

        timeWindowWrapper(dTimeWindow, evtData, frameSceneFBO);
        eventWindowWrapper(dEventWindow, evtData, frameSceneFBO); 
        if (dTimeWindow) { // Ensure windows match (time window == events window)
            evtData->getEventWindow_L() = evtData->getFirstEvent(evtData->getTimeWindow_L(), normFactor);
            evtData->getEventWindow_R() = evtData->getLastEvent(evtData->getTimeWindow_R(), normFactor);
        }  
        else if (dEventWindow) { 
            evtData->getTimeWindow_L() = evtData->getTimestamp(evtData->getEventWindow_L(), normFactor);
            evtData->getTimeWindow_R() = evtData->getTimestamp(evtData->getEventWindow_R(), normFactor);
        }
        spaceWindowWrapper(dSpaceWindow, evtData);

        ImGui::Text("Processing options");    
        if (ImGui::Combo("Shutter", &evtData->getShutterType(), "Time Based\0Event Based\0")) {
            dProcessingOptions = true;
            evtData->getEventShutterWindow_L() = 0;
            evtData->getEventShutterWindow_R() = 0;
            evtData->getTimeShutterWindow_L() = 0;
            evtData->getTimeShutterWindow_R() = 0;
        }
        if (evtData->getShutterType() == EventData::TIME_SHUTTER) {
            float frameLength_T = evtData->getTimeWindow_R() - evtData->getTimeWindow_L();
            dProcessingOptions |= ImGui::SliderFloat(unitLabels[shutterInitial].c_str(), &evtData->getTimeShutterWindow_L(), 0, frameLength_T, "%.4f"); 
            dProcessingOptions |= ImGui::SliderFloat(unitLabels[shutterFinal].c_str(), &evtData->getTimeShutterWindow_R(), 0, frameLength_T, "%.4f");  
            
            evtData->getTimeShutterWindow_L() = std::clamp(evtData->getTimeShutterWindow_L(), 0.0f, frameLength_T);
            evtData->getTimeShutterWindow_R() = std::clamp(evtData->getTimeShutterWindow_R(), evtData->getTimeShutterWindow_L(), frameLength_T);
        }
        else if (evtData->getShutterType() == EventData::EVENT_SHUTTER) {
            uint frameLength_E = evtData->getEventWindow_R() - evtData->getEventWindow_L();
            dProcessingOptions |= ImGui::SliderInt("Shutter Initial (events)", (int *) &evtData->getEventShutterWindow_L(), 0, frameLength_E); 
            dProcessingOptions |= ImGui::SliderInt("Shutter Final (events)", (int *) &evtData->getEventShutterWindow_R(), 0, frameLength_E);   
        
            evtData->getEventShutterWindow_L() = std::clamp(evtData->getEventShutterWindow_L(), (uint) 0, frameLength_E);
            evtData->getEventShutterWindow_R() = std::clamp(evtData->getEventShutterWindow_R(), evtData->getEventShutterWindow_L(), frameLength_E);        
        }
        if (dTimeWindow || dEventWindow || dProcessingOptions) { // Ensure internal shutter values match for both options
            if (evtData->getShutterType() == EventData::TIME_SHUTTER) {
                uint startEvent = evtData->getFirstEvent(evtData->getTimeWindow_L(), normFactor);
                uint leftEvent = evtData->getFirstEvent(evtData->getTimeWindow_L() + evtData->getTimeShutterWindow_L(), normFactor);
                uint rightEvent = evtData->getLastEvent(evtData->getTimeWindow_L() + evtData->getTimeShutterWindow_R(), normFactor);
                evtData->getEventShutterWindow_L() = leftEvent - startEvent;
                evtData->getEventShutterWindow_R() = rightEvent - startEvent;
            }
            else if (evtData->getShutterType() == EventData::EVENT_SHUTTER) {
                float startTime = evtData->getTimestamp(evtData->getEventWindow_L(), normFactor);
                float leftTime = evtData->getTimestamp(evtData->getEventWindow_L() + evtData->getEventShutterWindow_L(), normFactor);
                float rightTime = evtData->getTimestamp(evtData->getEventWindow_L() + evtData->getEventShutterWindow_R(), normFactor);
                evtData->getTimeShutterWindow_L() = leftTime - startTime;
                evtData->getTimeShutterWindow_R() = rightTime - startTime;
            }
        }
 
        // Auto update controls
        dProcessingOptions |= ImGui::SliderFloat("FPS", &frameSceneFBO.getUpdateFPS(), 0, 100);
        if (ImGui::Button("Play (Time period)") && frameSceneFBO.getAutoUpdate() == FrameScene::MANUAL_UPDATE) {
            frameSceneFBO.getAutoUpdate() = FrameScene::TIME_AUTO_UPDATE;
            frameSceneFBO.setLastRenderTime(glfwGetTime());
        }
        if (ImGui::Button("Play (Events period)") && frameSceneFBO.getAutoUpdate() == FrameScene::MANUAL_UPDATE) {
            frameSceneFBO.getAutoUpdate() = FrameScene::EVENT_AUTO_UPDATE;
            frameSceneFBO.setLastRenderTime(glfwGetTime());
        }
        frameSceneFBO.getUpdateFPS() = std::max(frameSceneFBO.getUpdateFPS(), 0.0f);

        // "Post" processing
        MorletFunc::h /= normFactor;
        dProcessingOptions |= ImGui::SliderFloat("Frequency (Hz)", &frameSceneFBO.getFreq(), 0.001f, 250); // TODO decide reasonable range
        dProcessingOptions |= ImGui::SliderFloat(unitLabels[FWHM].c_str(), &MorletFunc::h, 0.0001f, (evtData->getTimeWindow_R() - evtData->getTimeWindow_L()) * 0.5, "%.4f");
        dProcessingOptions |= ImGui::Checkbox("Morlet Shutter", &frameSceneFBO.isMorlet());
        dProcessingOptions |= ImGui::Checkbox("PCA", &frameSceneFBO.getPCA());
        dProcessingOptions |= ImGui::Checkbox("Positive Events Only", &evtData->getIsPositiveOnly());
        frameSceneFBO.getFreq() = std::max(frameSceneFBO.getFreq(), 0.01f);
        MorletFunc::h = std::max(MorletFunc::h, 0.0001f) * normFactor;
        ImGui::Separator();

        // Video (ffmpeg) controls
        ImGui::Text("Video options"); // TODO add documentation
        inputTextWrapper(video_name); // TODO consider including library to allow inputting string as parameter
        if (ImGui::Button("Start Record")) {
            recording = true;
        }
        if (ImGui::Button("Stop Record")) {
            recording = false;
        }

    ImGui::End();

    ImGui::Begin("Frame");
        ImGui::Text("Digital Coded Exposure"); 

        // TODO ask Andrew about aspect ratio standards/preferences
        image_sz = ImGui::GetContentRegionAvail();
        final_sz = ImVec2(image_sz.x, image_sz.y); // fbo viewport is static ish
        ImGui::Image((ImTextureID)frameSceneFBO.getColorTexture(), final_sz);
    ImGui::End();

    evtData->normalizeTime();
    frameSceneFBO.normalizeTime(normFactor);
    frameSceneFBO.setDirtyBit(dFile | dTimeWindow | dEventWindow | dSpaceWindow | dProcessingOptions);

    if (loadFile) {
        unitLabels.clear();
    }
}

// ???
float randFloat() { 
    return static_cast<float>(rand()) / RAND_MAX;
};

vec3 randXYZ() {
    return vec3(randFloat(), randFloat(), randFloat());
}

void genVBO(GLuint &vbo, size_t num_bytes, size_t draw_type) {
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, num_bytes, 0, static_cast<GLenum>(draw_type));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}