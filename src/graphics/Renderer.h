#pragma once
#include <raylib.h>
#include <string>
#include <vector>
#include "core/Config.h"
#include "core/LuaEngine.h"

class FeatureExtractor;
class InputManager;

// Last stage of the pipeline: runs on the main thread, opens the raylib window
// and each frame passes the audio features to the active Lua preset, which draws.
class Renderer
{
public:
    Renderer(int width = Config::kWindowWidth, int height = Config::kWindowHeight);
    ~Renderer();

    // Opens the fullscreen window and the OpenGL context. False on failure.
    bool init();

    // Blocking render loop: runs until ESC or the window is closed.
    void run();

    void setAudioFeatures(FeatureExtractor* features) { m_audioFeatures = features; }
    void setLuaEngine(LuaEngine* engine) { m_luaEngine = engine; }
    // Optional: if nullptr, presets get knobs stuck at 0, no crash.
    void setInputManager(InputManager* inputManager) { m_inputManager = inputManager; }
    // Presets the buttons cycle through + index of the one already active.
    void setPresetList(const std::vector<std::string>& presets, int currentIndex) {
        m_presets = presets;
        m_presetIndex = currentIndex;
    }

private:
    // Window size (= monitor resolution in fullscreen), passed to the presets
    // as the drawing area.
    int m_width, m_height;

    // Audio features, read each frame and passed to the Lua preset.
    FeatureExtractor* m_audioFeatures;
    float m_rms;
    float m_centroid;
    float m_onset;
    std::vector<float> m_bands;

    // Pots (5 knobs). m_inputManager may stay nullptr.
    InputManager* m_inputManager;
    std::vector<float> m_knobs;

    LuaEngine* m_luaEngine;

    // Presets the buttons cycle through and index of the active one.
    std::vector<std::string> m_presets;
    int m_presetIndex;

    void handleButtons();
    void render();
};
