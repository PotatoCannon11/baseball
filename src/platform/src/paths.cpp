#include "platform/paths.h"

#include <SDL3/SDL.h>

namespace platform {

std::string get_base_path() {
    const char* path = SDL_GetBasePath();
    return path ? std::string(path) : std::string();
}

std::string get_pref_path(const std::string& org, const std::string& app) {
    char* path = SDL_GetPrefPath(org.c_str(), app.c_str());
    if (!path) {
        return std::string();
    }
    std::string result(path);
    SDL_free(path);
    return result;
}

}  // namespace platform
