#pragma once

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <raylib.h>
#include <string>
#include <vector>

// Bridge between the C++ engine and the visual "presets", which are Lua scripts
// (assets/presets/*.lua) instead of C++: a visual can be changed or created by
// editing a .lua file, no recompiling.
//
// Uses sol2 (sol::state) over LuaJIT as the C++/Lua binding. init() registers a
// set of drawing functions wrapping raylib (fillRect, fillCircle, line, spline,
// drawCube, ...), so a script can draw in 2D and 3D with the same calls one
// would use in C++. raylib is immediate-mode (each call draws a finished shape),
// so presets stay short.
//
// A .lua preset must return a table with two functions:
//   preset.init()                                               -> once, at load
//   preset.render(rms, centroid, onset, bands, dt, knobs, w, h) -> once per frame
// bands is a 1-indexed Lua table with the 6 frequency bands (low to high, see
// FeatureExtractor::getBands). knobs is a 1-indexed table with the 5 pots
// (0.0-1.0, see InputManager::getValues). dt is the real elapsed time in seconds
// (raylib GetFrameTime): an animating preset must accumulate its own clock with
// "t = t + dt", NOT a fixed step, or its speed changes with the framerate. w, h
// are the real window size in pixels: presets must use these instead of
// hardcoded constants. Extra trailing args a preset does not declare are simply
// ignored by Lua, so older presets stay valid.
class LuaEngine
{
public:
    LuaEngine();
    ~LuaEngine();

    // Creates the Lua state, opens base libraries and registers the raylib
    // drawing functions. Call once at startup, before loadPreset. Returns false
    // if registration fails (almost never).
    bool init();

    // Loads and runs the given .lua file. The script must return a valid table,
    // which becomes the active preset (its init() is then called). Can be called
    // again at runtime to switch preset on the fly.
    bool loadPreset(const std::string &filename);

    // Called once per frame by the Renderer (between BeginDrawing/EndDrawing):
    // invokes preset.render(...) on the active preset. No-op if none is ready.
    // Runtime Lua errors are printed to stderr, they do not crash the program
    // (sol::protected_function). bands/knobs are passed as Lua tables.
    void updateAndRender(float rms, float centroid, float onset,
                         const std::vector<float> &bands, float dt, const std::vector<float> &knobs,
                         float width, float height);

private:
    sol::state m_lua;
    sol::table m_currentPreset; // active preset's Lua table
    bool m_ready;               // true only after a successful loadPreset

    // Current 3D camera, set by presets via setCamera() and used by
    // beginMode3D(). Initialized to a sensible default so a preset calling
    // beginMode3D() without setCamera() still sees something.
    Camera3D m_camera;
};
