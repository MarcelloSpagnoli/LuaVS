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

    // --- FeatureExtractor: onset (spectral flux) ---
    // Threshold on the normalized flux (0-1): 0.30 = "30% new energy vs the
    // previous frame". Higher = less sensitive onset. Percussive genres (sharp
    // drum hits against a near-silent background) can work with a low value;
    // continuously-textured music (synths, sustained chords, vocals) has a
    // higher "musical background" flux even with no real hit, so it needs a
    // higher bar to avoid triggering on ordinary timbral movement.
    inline constexpr float kOnsetThreshold = 0.30f;

    // Hysteresis (Schmitt-trigger) lower threshold: after an onset fires at
    // kOnsetThreshold, no new onset is allowed until the flux drops back BELOW
    // this. The gap (kOnsetThreshold - this) must be WIDE enough to swallow the
    // dip WITHIN one hit: a real hit is attack + body/resonance (e.g. snare
    // wires), so its flux has two close peaks with a shallow dip between -- too
    // narrow a gap re-arms on that dip and fires the same hit twice. A genuinely
    // separate hit still drops the flux near 0, well below this, so close hits (a
    // drum roll) are NOT lost: this is a value gate, not a time cooldown.
    inline constexpr float kOnsetThresholdLow = 0.14f;

    // Minimum normalized RMS (0-1, same scale as getRms()) required for a
    // frame to even be considered for onset. The flux RATIO is noise-sensitive
    // at low absolute energy: during a hit's decay tail, totalMagnitude is
    // small, so ordinary bin-level noise becomes a large fraction of it and
    // repeatedly crosses kOnsetThreshold, firing several false onsets from the
    // tail of a single real hit. Requiring real loudness right now (not just
    // "not silent") filters that out without touching the flux formula itself.
    inline constexpr float kOnsetMinRms = 0.25f;

    // Debug only: when true, detectOnset prints the live flux ratio (while above
    // kOnsetThresholdLow) and marks the frames that fire, so the thresholds can
    // be set from real numbers instead of by ear. Leave false in normal use.
    inline constexpr bool kOnsetDebug = false;

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
