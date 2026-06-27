#include <algorithm>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>
#include "audio/AudioCapture.h"
#include "core/Config.h"
#include "core/FftProcessor.h"
#include "core/FeatureExtractor.h"
#include "core/InputManager.h"
#include "core/LuaEngine.h"
#include "graphics/Renderer.h"

// Stops the audio thread when the render loop exits (ESC): main sets it false
// after run(), then joins.
bool g_running = true;

// Audio thread: capture -> FFT -> feature extraction, in the background.
void audioThread(AudioCapture& capture, FftProcessor& fft, FeatureExtractor& extractor)
{
    const int windowSize = Config::kWindowSize;
    std::vector<float> audioBuffer(windowSize);
    std::vector<float> fftMagnitudes(windowSize / 2 + 1);

    while (g_running)
    {
        if (!capture.capturePeriod(audioBuffer))
            continue;

        fft.process(audioBuffer, fftMagnitudes);
        // extractFeatures locks internally.
        extractor.extractFeatures(audioBuffer, fftMagnitudes);
    }
}
int main(int argc, char* argv[])
{
    std::cout << "=== LuaVS (raylib + LuaJIT) ===" << std::endl;

    // 1. Audio
    std::string targetDevice = AudioCapture::getPlugAndPlayDevice();
    AudioCapture capture;
    if (!capture.openDevice(targetDevice))
    {
        std::cerr << "[ERRORE] Impossibile aprire dispositivo audio (Mackie scollegata?)" << std::endl;
        return -1;
    }

    const int windowSize = Config::kWindowSize;
    FftProcessor fftProcessor(windowSize);
    FeatureExtractor extractor(windowSize);

    // 2. Input (pots via MCP3008/SPI, buttons via GPIO). Optional: if SPI/GPIO
    // fail, InputManager still works with knobs stuck at 0, nothing fatal.
    InputManager inputManager;

    // 3. Audio thread
    std::thread audioThreadHandle(audioThread, std::ref(capture), std::ref(fftProcessor), std::ref(extractor));

    // 4. Rendering (main thread)
    Renderer renderer(Config::kWindowWidth, Config::kWindowHeight);
    if (!renderer.init())
    {
        std::cerr << "[ERRORE] Impossibile inizializzare il renderer grafico!" << std::endl;
        g_running = false;
        if (audioThreadHandle.joinable()) audioThreadHandle.join();
        return -1;
    }
    renderer.setAudioFeatures(&extractor);
    renderer.setInputManager(&inputManager);

    // 5. Lua engine
    LuaEngine luaEngine;
    if (!luaEngine.init())
    {
        std::cerr << "[ERRORE] Impossibile inizializzare LuaEngine!" << std::endl;
        g_running = false;
        if (audioThreadHandle.joinable()) audioThreadHandle.join();
        return -1;
    }

    // Default preset (or the one given as argv[1]).
    std::string presetPath = "assets/presets/bars_eq.lua";
    if (argc > 1) {
        presetPath = argv[1];
    }

    if (!luaEngine.loadPreset(presetPath))
    {
        std::cerr << "[ERRORE] Impossibile caricare il preset: " << presetPath << std::endl;
        g_running = false;
        if (audioThreadHandle.joinable()) audioThreadHandle.join();
        return -1;
    }
    renderer.setLuaEngine(&luaEngine);

    // Presets the buttons cycle through: scan the folder (so adding a .lua needs
    // no code change), alphabetical for a deterministic order.
    std::vector<std::string> presetList;
    for (const auto& entry : std::filesystem::directory_iterator("assets/presets")) {
        if (entry.path().extension() == ".lua") {
            presetList.push_back(entry.path().string());
        }
    }
    std::sort(presetList.begin(), presetList.end());

    // Index of the already-loaded preset, so the buttons continue from there
    // (0 if not found, e.g. argv[1] outside assets/presets/).
    int presetIndex = 0;
    for (size_t i = 0; i < presetList.size(); ++i) {
        if (presetList[i] == presetPath) { presetIndex = static_cast<int>(i); break; }
    }
    renderer.setPresetList(presetList, presetIndex);

    // 6. Main render loop (blocks until the window is closed)
    std::cout << "[OK] " << presetList.size() << " preset, attivo: " << presetPath
              << " | ESC per uscire" << std::endl;
    renderer.run();

    // 7. Clean shutdown
    g_running = false; // tell the audio thread to leave its loop
    capture.closeDevice();
    if (audioThreadHandle.joinable()) {
        audioThreadHandle.join();
    }

    std::cout << "[OK] Engine spento." << std::endl;
    return 0;
}
