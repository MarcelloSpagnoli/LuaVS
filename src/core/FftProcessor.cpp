#include "FftProcessor.h"
#include <cmath>
#include <iostream>

FftProcessor::FftProcessor(int windowSize) : m_windowSize(windowSize)
{
    m_fftSize = (windowSize / 2) + 1; // real FFT is symmetric
    m_fftwIn = (double *)fftw_malloc(sizeof(double) * m_windowSize);
    m_fftwOut = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * m_fftSize);
    m_plan = fftw_plan_dft_r2c_1d(m_windowSize, m_fftwIn, m_fftwOut, FFTW_MEASURE);
    generateHannWindow();
}

FftProcessor::~FftProcessor()
{
    fftw_destroy_plan(m_plan);
    fftw_free(m_fftwIn);
    fftw_free(m_fftwOut);
}

// Hann window with (N-1) denominator (matches the thesis formula).
void FftProcessor::generateHannWindow()
{
    m_hannWindow.resize(m_windowSize);
    for (int n = 0; n < m_windowSize; n++)
    {
        m_hannWindow[n] = 0.5f * (1.0f - std::cos((2.0f * M_PI * n) / (m_windowSize - 1)));
    }
}

void FftProcessor::process(const std::vector<float> &inputBuffer, std::vector<float> &outputMagnitudes)
{
    if (inputBuffer.size() < static_cast<size_t>(m_windowSize))
    {
        return;
    }

    // Apply the Hann window.
    for (int i = 0; i < m_windowSize; i++)
    {
        m_fftwIn[i] = static_cast<double>(inputBuffer[i]) * m_hannWindow[i];
    }

    fftw_execute(m_plan);

    outputMagnitudes.resize(m_fftSize);

    // Magnitude of each complex bin.
    for (int k = 0; k < m_fftSize; k++)
    {
        double real = m_fftwOut[k][0];
        double imag = m_fftwOut[k][1];
        outputMagnitudes[k] = static_cast<float>(std::sqrt(real * real + imag * imag));
    }
}
