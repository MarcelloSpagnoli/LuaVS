#include "FeatureExtractor.h"
#include "Config.h"
#include <algorithm>
#include <cmath>
#include <iostream>

FeatureExtractor::FeatureExtractor(int windowSize)
    : m_windowSize(windowSize),
      m_rms(0.0f),
      m_spectralCentroid(0.0f),
      m_isOnset(false),
      m_onsetThreshold(Config::kOnsetThreshold),
      m_sensitivity(Config::kSensitivity),
      m_attackCoef(Config::kAttackCoef),
      m_releaseCoef(Config::kReleaseCoef),
      m_gateOpenMargin(Config::kGateOpenMargin),
      m_gateCloseMargin(Config::kGateCloseMargin),
      m_isSilent(true)
{
    m_fftSize = (m_windowSize / 2) + 1;
    m_bands.resize(6, 0.0f);
    m_prevMagnitudes.resize(m_fftSize, 0.0f);

    // Maxima start at 1.0 only to avoid a divide-by-zero on the first frame;
    // they are recalibrated immediately on real data (see m_firstFrame).
    m_bandMax.resize(6, 1.0f);
    m_smoothedBands.resize(6, 0.0f);
    m_rmsMax = 1.0f;
    m_smoothedRms = 0.0f;
    m_smoothedCentroid = 0.0f;
    m_centroidMax = 1.0f;
    m_noiseFloor = 1.0f;
    m_firstFrame = true;
}

void FeatureExtractor::extractFeatures(const std::vector<float>& audioBuffer,
                                       const std::vector<float>& fftMagnitudes)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Fixed order: RMS decides m_isSilent, reused by bands/centroid/onset.
    m_rms = calculateRms(audioBuffer);
    calculateBands(fftMagnitudes);
    m_spectralCentroid = calculateCentroid(fftMagnitudes);
    detectOnset(fftMagnitudes);

    m_prevMagnitudes = fftMagnitudes;
    m_firstFrame = false;
}

// RMS: 4-step pipeline (raw -> gate -> smoothing -> normalization).
float FeatureExtractor::calculateRms(const std::vector<float>& audioBuffer) {
    if (audioBuffer.size() < static_cast<size_t>(m_windowSize)) return 0.0f;

    float sumOfSquares = 0.0f;
    for (int n = 0; n < m_windowSize; ++n)
        sumOfSquares += audioBuffer[n] * audioBuffer[n];
    float rawRms = std::sqrt(sumOfSquares / m_windowSize);

    // Self-calibrating noise floor: drops instantly to the minimum, rises very
    // slowly (kNoiseFloorRiseCoef). No absolute level to guess.
    if (m_firstFrame || rawRms < m_noiseFloor)
        m_noiseFloor = std::max(rawRms, 0.0001f);
    else
        m_noiseFloor *= Config::kNoiseFloorRiseCoef;

    // Hysteresis gate on the raw value (decides immediately): two thresholds
    // to avoid opening/closing repeatedly around the boundary.
    if (m_isSilent) {
        if (rawRms > m_noiseFloor * m_gateOpenMargin) m_isSilent = false;
    } else {
        if (rawRms < m_noiseFloor * m_gateCloseMargin) m_isSilent = true;
    }

    // Attack/release smoothing: from here the pipeline works on a smooth value.
    float smoothingTarget = m_isSilent ? 0.0f : rawRms;
    if (m_firstFrame) {
        m_smoothedRms = smoothingTarget;
    } else {
        float coef = (smoothingTarget > m_smoothedRms) ? m_attackCoef : m_releaseCoef;
        m_smoothedRms += coef * (smoothingTarget - m_smoothedRms);
    }

    // Observed max on the smoothed value (not the raw, which would jump), with
    // ~40s decay (kMaxDecayCoef): keeps the dynamic contrast.
    if (m_firstFrame) {
        m_rmsMax = std::max(m_smoothedRms, 0.001f);
    } else if (m_smoothedRms > m_rmsMax) {
        m_rmsMax = m_smoothedRms;
    } else {
        m_rmsMax *= Config::kMaxDecayCoef;
        if (m_rmsMax < 0.001f) m_rmsMax = 0.001f;
    }

    // Floor never exceeds 30% of the max, else after hours of music it would
    // creep up and re-close the gate on a good signal.
    m_noiseFloor = std::min(m_noiseFloor, m_rmsMax * 0.3f);

    // Normalization: tanh for soft saturation, divided by tanh(sensitivity) so
    // the output reaches exactly 1.0 at the recent maximum.
    float softSaturated = std::tanh((m_smoothedRms / m_rmsMax) * m_sensitivity);
    float maxPossible = std::max(std::tanh(m_sensitivity), 0.0001f);
    return softSaturated / maxPossible;
}

// 6 frequency bands, same pipeline as RMS applied to each band.
void FeatureExtractor::calculateBands(const std::vector<float>& fftMagnitudes) {
    // Boundaries in FFT bins (kBandBoundaries): at 86.13 Hz/bin they span from
    // the lows (band 0, kick/bass) to the highs (band 5, cymbals/air).
    for (int b = 0; b < 6; ++b) {
        // Clamp to fftSize: protects if windowSize changes (boundaries fixed).
        int startBin = std::min(Config::kBandBoundaries[b], m_fftSize);
        int endBin = std::min(Config::kBandBoundaries[b + 1], m_fftSize);
        float sum = 0.0f;
        int count = 0;
        for (int bin = startBin; bin < endBin; ++bin) { sum += fftMagnitudes[bin]; count++; }

        if (count == 0) { m_bands[b] = 0.0f; continue; }
        float currentVal = sum / count;

        // Gate -> smoothing -> max -> normalization (same as RMS).
        float smoothingTarget = m_isSilent ? 0.0f : currentVal;
        if (m_firstFrame) {
            m_smoothedBands[b] = smoothingTarget;
        } else {
            float coef = (smoothingTarget > m_smoothedBands[b]) ? m_attackCoef : m_releaseCoef;
            m_smoothedBands[b] += coef * (smoothingTarget - m_smoothedBands[b]);
        }

        if (m_firstFrame) {
            m_bandMax[b] = std::max(m_smoothedBands[b], 0.001f);
        } else if (m_smoothedBands[b] > m_bandMax[b]) {
            m_bandMax[b] = m_smoothedBands[b];
        } else {
            m_bandMax[b] *= Config::kMaxDecayCoef;
            if (m_bandMax[b] < 0.001f) m_bandMax[b] = 0.001f;
        }

        float softSaturated = std::tanh((m_smoothedBands[b] / m_bandMax[b]) * m_sensitivity);
        float maxPossible = std::max(std::tanh(m_sensitivity), 0.0001f);
        m_bands[b] = softSaturated / maxPossible;
    }
}

// Spectral centroid (spectrum center of mass), normalized 0-1 and processed
// with gate + smoothing + auto-gain like RMS and bands.
float FeatureExtractor::calculateCentroid(const std::vector<float>& fftMagnitudes) {
    float weightedSum = 0.0f;    // sum of i*|X_i|
    float totalMagnitude = 0.0f; // sum of |X_i|
    // From i=1: skips the DC bin (f_0=0 adds nothing to the numerator, but its
    // near-0 Hz noise would pollute the denominator).
    for (int i = 1; i < m_fftSize; ++i) {
        weightedSum += i * fftMagnitudes[i];
        totalMagnitude += fftMagnitudes[i];
    }

    // Center of mass in bins, normalized to 0-1 (/fftSize). Gated: in silence
    // there is no meaningful centroid, treat it as 0.
    float rawCentroid = 0.0f;
    if (!m_isSilent && totalMagnitude > 0.0001f)
        rawCentroid = (weightedSum / totalMagnitude) / (float)m_fftSize;

    if (m_firstFrame) {
        m_smoothedCentroid = rawCentroid;
    } else {
        float coef = (rawCentroid > m_smoothedCentroid) ? m_attackCoef : m_releaseCoef;
        m_smoothedCentroid += coef * (rawCentroid - m_smoothedCentroid);
    }

    if (m_firstFrame) {
        m_centroidMax = std::max(m_smoothedCentroid, 0.001f);
    } else if (m_smoothedCentroid > m_centroidMax) {
        m_centroidMax = m_smoothedCentroid;
    } else {
        m_centroidMax *= Config::kMaxDecayCoef;
        if (m_centroidMax < 0.001f) m_centroidMax = 0.001f;
    }

    // 1.0 = the brightest recently. Defensive clamp to stay within 0-1.
    return std::min(m_smoothedCentroid / m_centroidMax, 1.0f);
}

// Onset: normalized spectral flux above threshold (disabled in silence).
void FeatureExtractor::detectOnset(const std::vector<float>& fftMagnitudes) {
    if (m_isSilent) { m_isOnset = false; return; }

    float spectralFlux = 0.0f;
    float totalMagnitude = 0.0f;
    // Sum only the per-bin energy INCREASES vs the previous frame.
    for (int i = 1; i < m_fftSize; ++i) {
        float diff = fftMagnitudes[i] - m_prevMagnitudes[i];
        if (diff > 0.0f) spectralFlux += diff;
        totalMagnitude += fftMagnitudes[i];
    }

    if (totalMagnitude < 0.0001f) { m_isOnset = false; return; }

    // Normalized by total energy: threshold independent of volume.
    m_isOnset = (spectralFlux / totalMagnitude) > m_onsetThreshold;
}
