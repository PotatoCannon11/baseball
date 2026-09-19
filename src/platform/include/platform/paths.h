#pragma once

#include <string>

// Base and pref path lookup, wrapping SDL3's SDL_GetBasePath / SDL_GetPrefPath
// rather than reimplementing per-OS logic: SDL already handles the
// .app-bundle path on macOS, argv[0]-relative resolution on Linux, and the
// module path on Windows correctly, so re-deriving it here would just be a
// worse copy. This header is the only place outside SDL itself that should
// know these two function names exist.
//
// Both allocate (they build a std::string) and are only ever called at
// startup / on explicit save-preferences actions, never from the sim or
// frame hot path.
namespace platform {

// Directory containing the running executable, with a trailing separator.
// Empty string if SDL could not determine it.
std::string get_base_path();

// Writable per-user preferences directory for (org, app), created if it
// does not exist. Empty string if SDL could not determine or create it.
std::string get_pref_path(const std::string& org, const std::string& app);

}  // namespace platform
