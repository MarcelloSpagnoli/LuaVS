#include <iostream>
#include <vector>
#include <cmath>
#include <unistd.h>
#include "audio/AudioCapture.h"
#include "core/Config.h"
#include "core/FftProcessor.h"
#include "core/FeatureExtractor.h"

int main()
{
    std::cout << "=== TEST INTEGRAZIONE FFT + FEATURE EXTRACTOR ===" << std::endl;

    // 1. Hardware setup
    std::string targetDevice = AudioCapture::getPlugAndPlayDevice();
    AudioCapture capture;
    if (!capture.openDevice(targetDevice))
    {
        std::cerr << "[ERRORE] Impossibile aprire il dispositivo audio." << std::endl;
        return -1;
    }

    // 2. DSP chain setup (same configuration as main.cpp)
    const int windowSize = Config::kWindowSize;
    FftProcessor fft(windowSize);
    FeatureExtractor extractor(windowSize);

    std::vector<float> audioBuffer(windowSize);
    std::vector<float> fftMagnitudes(windowSize / 2 + 1);

    std::cout << "[OK] Catena DSP pronta (RMS, bande, centroide, onset)." << std::endl;
    std::cout << "[INFO] Premi Ctrl+C per uscire.\n" << std::endl;

    // 3. Test loop: capture -> FFT -> feature extraction -> print
    int frameCount = 0;
    while (true)
    {
        if (!capture.capturePeriod(audioBuffer))
        {
            continue;
        }

        fft.process(audioBuffer, fftMagnitudes);
        extractor.extractFeatures(audioBuffer, fftMagnitudes);

        // Print the features every ~20 frames to avoid flooding the terminal
        if (++frameCount % 20 == 0)
        {
            std::cout << "RMS=" << extractor.getRms()
                       << "  Centroid=" << extractor.getSpectralCentroid()
                       << "  Onset=" << (extractor.isOnset() ? "SI" : "no")
                       << std::endl;
        }
    }

    capture.closeDevice();
    return 0;
}
