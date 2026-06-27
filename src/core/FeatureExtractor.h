#ifndef FEATURE_EXTRACTOR_H
#define FEATURE_EXTRACTOR_H

#include <vector>
#include <mutex>

// Pipeline stage 3: turns the audio buffer + FFT spectrum into the features the
// presets react to:
//   - RMS       -> overall volume, normalized 0-1 (auto-gain)
//   - bands (6) -> energy in 6 frequency bands, normalized 0-1
//   - centroid  -> spectral brightness (low->high), normalized 0-1
//   - onset     -> true on a transient/hit frame
//
// extractFeatures() runs on the audio thread, the getters on the render thread;
// access is guarded by m_mutex. Tuning constants live in Config.h.
class FeatureExtractor {
public:
    FeatureExtractor(int windowSize);
    ~FeatureExtractor() = default;

    // Updates every feature from THIS frame's audio + spectrum. Call once per
    // period in order (capture -> fft -> here): onset compares against the
    // previous frame's spectrum.
    void extractFeatures(const std::vector<float>& audioBuffer,
                         const std::vector<float>& fftMagnitudes);

    // Read from the render thread, under lock since extractFeatures writes in
    // parallel. getBands returns a copy (a reference would be unprotected once
    // the lock is released).
    float getRms() const { std::lock_guard<std::mutex> lock(m_mutex); return m_rms; }
    std::vector<float> getBands() const { std::lock_guard<std::mutex> lock(m_mutex); return m_bands; }
    float getSpectralCentroid() const { std::lock_guard<std::mutex> lock(m_mutex); return m_spectralCentroid; }
    bool isOnset() const { std::lock_guard<std::mutex> lock(m_mutex); return m_isOnset; }

private:
    // All three share the same pipeline: silence gate -> attack/release
    // smoothing -> normalization against the recent maximum (auto-gain).
    float calculateRms(const std::vector<float>& audioBuffer);
    void calculateBands(const std::vector<float>& fftMagnitudes);
    float calculateCentroid(const std::vector<float>& fftMagnitudes);

    // Onset via normalized spectral flux: sum of the per-bin energy INCREASES
    // vs the previous frame, divided by total energy (so the threshold is
    // volume-independent). Disabled in silence.
    void detectOnset(const std::vector<float>& fftMagnitudes);

    int m_windowSize;
    int m_fftSize;

    float m_rms;
    std::vector<float> m_bands;
    float m_spectralCentroid; // 0-1, for the presets
    bool m_isOnset;

    std::vector<float> m_prevMagnitudes; // previous frame spectrum, for onset

    // Tuning constants, copied from Config.h in the constructor.
    float m_onsetThreshold;
    float m_sensitivity;
    float m_attackCoef;
    float m_releaseCoef;

    // Self-calibrating silence gate: m_noiseFloor tracks the background noise
    // (drops instantly to the minimum, rises slowly). Two thresholds
    // (open > close) avoid chattering. m_isSilent is decided in calculateRms
    // and reused by bands/onset.
    float m_noiseFloor;
    float m_gateOpenMargin;
    float m_gateCloseMargin;
    bool m_isSilent;

    // Auto-gain: observed maximum (with decay) + smoothed value, per RMS, bands
    // and centroid. Normalizing against the recent max uses the full 0-1 range
    // and adapts to loud or quiet material without fixed thresholds.
    std::vector<float> m_bandMax;
    std::vector<float> m_smoothedBands;
    float m_rmsMax;
    float m_smoothedRms;
    float m_smoothedCentroid;
    float m_centroidMax;

    // True only on the first frame: calibrates the maxima on real data instead
    // of starting at 1.0 and decaying down (responsive startup).
    bool m_firstFrame;

    mutable std::mutex m_mutex;
};

#endif // FEATURE_EXTRACTOR_H
