#pragma once

#include <cstdint>

// Single place for all the project's magic numbers (audio, window,
// FeatureExtractor, InputManager). Fixed values: edit here and recompile.
namespace Config {

    // --- Audio pipeline ---
    inline constexpr unsigned int kSampleRate = 44100; // Hz
    inline constexpr unsigned int kChannels = 1;       // mono
    // ALSA period = FFT window (same block): 512 samples = ~11.6ms.
    inline constexpr int kWindowSize = 512;

    // --- Render window ---
    // Working resolution (thesis 2.2: chosen so the Pi GPU stays at 60 FPS).
    // With fullscreen on X11 it becomes the monitor's video mode; the display
    // hardware scales it to the panel.
    inline constexpr int kWindowWidth = 1280;
    inline constexpr int kWindowHeight = 720;

    // --- FeatureExtractor: fixed tuning ---
    // Threshold on the normalized flux (0-1): 0.15 = "15% new energy vs the
    // previous frame". Higher = less sensitive onset.
    inline constexpr float kOnsetThreshold = 0.15f;

    // Response curve shape (inside the tanh): lower = more dynamic range,
    // higher = snappier. Does not change the ceiling (1.0).
    inline constexpr float kSensitivity = 1.4f;

    // Smoothing: fast rise (attack, responsive to hits), slower fall (release)
    // but not too slow, so it does not stay "muddy" after a peak.
    inline constexpr float kAttackCoef = 0.2f;
    inline constexpr float kReleaseCoef = 0.1f;

    // Silence gate, in multiples of the noise floor: opens above 3x, closes
    // below 1.5x. Two thresholds avoid chattering.
    inline constexpr float kGateOpenMargin = 3.0f;
    inline constexpr float kGateCloseMargin = 1.5f;

    // Noise floor rise per frame: very close to 1, drifts over minutes. The
    // fall is instant to the minimum (handled in code).
    inline constexpr float kNoiseFloorRiseCoef = 1.00001f;

    // Observed-maximum decay: ~0.9997098 = ~40s time constant at 86 frames/s.
    // Keeps the verse/chorus dynamic contrast of a track.
    inline constexpr float kMaxDecayCoef = 0.9997098f;

    // The 6 band boundaries in FFT bins (86.13 Hz/bin): from the lows (band 0,
    // kick/bass) to the highs (band 5, cymbals/air).
    inline constexpr int kBandBoundaries[7] = {1, 2, 5, 12, 29, 70, 186};

    // --- InputManager: pots (MCP3008/SPI) and buttons (GPIO) ---
    inline constexpr int kNumPots = 5;
    inline constexpr int kNumButtons = 2;

    // SPI bus speed to the MCP3008.
    inline constexpr uint32_t kSpiSpeedHz = 1350000; // 1.35 MHz

    // PHYSICAL MCP3008 channels wired to the 5 pots (not contiguous). Indices
    // 0-4 of getValues() follow this order.
    inline constexpr int kPotChannels[kNumPots] = {0, 1, 3, 5, 7};

    // GPIO of the 2 buttons: 0 = next, 1 = previous in the preset cycle.
    inline constexpr unsigned int kButtonGpioOffsets[kNumButtons] = {17, 27};

    // Pot smoothing (symmetric: a knob has no attack/release).
    inline constexpr float kPotSmoothing = 0.3f;

    // Changes smaller than this (out of 1023) are ADC noise, ignored.
    inline constexpr int kPotDeadbandCounts = 10;

    // Anti-bounce: minimum time between two distinct presses of the same button.
    inline constexpr int kButtonDebounceMs = 200;

}
