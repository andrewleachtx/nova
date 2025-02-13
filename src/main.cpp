// TODO: fix
// #include "include.h"

#include <dv-processing/core/core.hpp>
#include <dv-processing/core/frame.hpp>
#include <dv-processing/io/mono_camera_recording.hpp>
#include <dv-processing/core/frame.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <memory>

// TODO: Probably just using namespace std is fine
using std::cout, std::cerr, std::endl;
using std::vector, std::string, std::make_shared, std::shared_ptr, std::pair, std::array, std::tuple;
using std::stoi, std::stoul, std::min, std::max, std::numeric_limits, std::abs;

// We can pass in a user pointer to callback functions - shouldn't require an updater; vars have inf lifespan
// GLFWwindow *g_window;
bool g_keyToggles[256] = {false};

int main() {
    cout << "Go package resolution https://davidmcelroy.org/?p=9964!!" << endl;
    return 0;
}