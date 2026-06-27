#ifndef FFT_PROCESSOR_H
#define FFT_PROCESSOR_H

#include <vector>
#include <fftw3.h>

// Pipeline stage 2 (after AudioCapture, before FeatureExtractor): takes a block
// of time-domain samples and produces its frequency-domain magnitude spectrum
// via a real FFT (FFTW3).
//
// The input is real, so it uses the r2c ("real to complex") plan: windowSize
// samples in, windowSize/2 + 1 bins out (the spectrum of a real signal is
// symmetric, the second half would be redundant).
//
// A Hann window is applied before the FFT: without it, the block's hard edges
// cause spectral leakage, because the FFT implicitly assumes the signal is
// periodic.
class FftProcessor
{
public:
    // windowSize: samples per block (must match AudioCapture/FeatureExtractor,
    // see main.cpp). Allocates the FFTW buffers and builds the plan once with
    // FFTW_MEASURE (slower to create, faster to run: fine, built once at start).
    FftProcessor(int windowSize);
    ~FftProcessor(); // frees the FFTW plan and buffers

    // Applies the Hann window to inputBuffer and computes the FFT, writing each
    // bin's magnitude into outputMagnitudes (resized to windowSize/2 + 1).
    // Magnitude only, no phase: FeatureExtractor only needs "how much energy is
    // at this frequency".
    void process(const std::vector<float> &inputBuffer, std::vector<float> &outputMagnitudes);

private:
    int m_windowSize;
    int m_fftSize;

    // Buffers and plan owned by FFTW3.
    double *m_fftwIn;
    fftw_complex *m_fftwOut;
    fftw_plan m_plan;

    std::vector<float> m_hannWindow;
    // Precomputes the Hann window coefficients (one per sample) so process()
    // does not recompute them every call.
    void generateHannWindow();
};

#endif // FFT_PROCESSOR_H
