#include "Renderer.h"
#include "core/FeatureExtractor.h"
#include "core/InputManager.h"
#include <raylib.h>
#include <cstdlib>
#include <iostream>

Renderer::Renderer(int width, int height)
    : m_width(width), m_height(height),
      m_audioFeatures(nullptr),
      m_rms(0.f), m_centroid(0.f), m_onset(0.f),
      m_inputManager(nullptr), m_luaEngine(nullptr),
      m_presetIndex(0)
{
}

Renderer::~Renderer()
{
    if (IsWindowReady())
        CloseWindow();
}

// Initializes raylib: opens the fullscreen window and the OpenGL context.
bool Renderer::init()
{
    // If launched without a display set (e.g. from SSH or a text console), use
    // the local screen :0, i.e. the monitor attached to the Pi. This way plain
    // "./LuaVS" always works, from any shell, with no prefix.
    if (!getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY"))
        setenv("DISPLAY", ":0", 1);

    // Fixed 1280x720 working resolution (thesis 2.2: at Full HD the VideoCore
    // misses 60 FPS, at 1280x720 it makes it). FLAG_FULLSCREEN_MODE +
    // InitWindow(1280,720) does a real video mode switch: the GPU renders at
    // 1280x720 and the monitor scales to full screen in hardware. Drawing goes
    // straight to the framebuffer, so MSAA 4x gives edge antialiasing.
    // NB: needs an X11 session (Wayland does not allow the mode switch).
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_FULLSCREEN_MODE);
    // Only show raylib errors: hides its info/warning chatter (e.g. the benign
    // "Closest fullscreen videomode" line confirming the 1280x720 mode switch).
    SetTraceLogLevel(LOG_ERROR);

    InitWindow(m_width, m_height, "LuaVS");
    if (!IsWindowReady())
    {
        std::cerr << "[ERRORE] Creazione finestra raylib fallita" << std::endl;
        return false;
    }

    // Actual resolution obtained (1280x720), passed to the presets as the
    // drawing area.
    m_width = GetScreenWidth();
    m_height = GetScreenHeight();

    HideCursor();
    SetExitKey(KEY_ESCAPE);

    std::cout << "[OK] Renderer e raylib inizializzati ("
              << m_width << "x" << m_height << ")" << std::endl;
    return true;
}

// Main loop: runs until ESC or the window is closed.
void Renderer::run()
{
    while (!WindowShouldClose())
        render();
}

// Draws one frame: updates the audio features, reads input, and delegates the
// drawing to the active Lua preset.
void Renderer::render()
{
    float dt = GetFrameTime();

    if (m_audioFeatures)
    {
        m_rms = m_audioFeatures->getRms();
        m_centroid = m_audioFeatures->getSpectralCentroid();
        m_onset = m_audioFeatures->isOnset() ? 1.0f : 0.0f;
        m_bands = m_audioFeatures->getBands();
    }

    if (m_inputManager)
    {
        m_inputManager->update();
        m_knobs = m_inputManager->getValues();
        handleButtons();
    }

    // Draw straight to the screen at native resolution: presets get the real
    // area (W, H) and adapt to it, on any monitor.
    BeginDrawing();
    ClearBackground(BLACK);

    if (m_luaEngine)
        m_luaEngine->updateAndRender(m_rms, m_centroid, m_onset, m_bands, dt, m_knobs,
                                     (float)GetScreenWidth(), (float)GetScreenHeight());

    EndDrawing();
}

// Hardware buttons: cycle preset next/previous, wrapping around the list.
void Renderer::handleButtons()
{
    if (m_presets.empty() || !m_luaEngine)
        return;

    bool next = m_inputManager->wasButtonPressed(0);
    bool prev = m_inputManager->wasButtonPressed(1);
    if (!next && !prev)
        return;

    int count = static_cast<int>(m_presets.size());
    if (next)
        m_presetIndex = (m_presetIndex + 1) % count;
    else
        m_presetIndex = (m_presetIndex - 1 + count) % count;

    std::cout << "[INPUT] Cambio preset (" << (next ? "avanti" : "indietro") << "): "
              << m_presets[m_presetIndex] << std::endl;
    m_luaEngine->loadPreset(m_presets[m_presetIndex]);
}
