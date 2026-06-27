#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include "core/Config.h"
#include <string>
#include <vector>
#include <alsa/asoundlib.h>

// Pipeline stage 1: talks to ALSA and pulls raw samples from the sound card
// (e.g. a USB interface). Lives on the audio thread (see audioThread in
// main.cpp), never the render thread: it keeps all the ALSA quirks (xruns,
// formats, device hints) in one place.
//
// Typical use (see main.cpp):
//   1. getPlugAndPlayDevice()  -> auto-pick the right card
//   2. openDevice(name)        -> open and configure (44.1kHz, mono, float)
//   3. capturePeriod(buffer)   -> in a loop, fills 512 samples per call
//   4. closeDevice()           -> clean shutdown

// A sound card found while scanning (scanDevices).
struct AudioDevice
{
    std::string name; // ALSA logical name (e.g. "hw:0,0")
};

class AudioCapture
{
public:
    AudioCapture();
    ~AudioCapture(); // closes the device if still open

    // Opens and configures the ALSA device with the project's fixed parameters
    // (44.1kHz, mono, float, 512-sample period). Call once before capturing.
    // deviceName: e.g. "default" or "plughw:1,0" (see getPlugAndPlayDevice).
    // Returns false if ALSA rejects any requested parameter. After opening it
    // also tries to raise the capture gain via amixer (trying several control
    // names, which differ per card); if none is found it warns instead of
    // failing silently.
    bool openDevice(const std::string &deviceName);

    // Closes the device. Safe to call even if it was never opened.
    void closeDevice();

    // Core call, run in a loop on the audio thread. Reads ONE period (512
    // samples, ~11.6ms at 44.1kHz) into buffer (auto-resized). Blocking: ALSA
    // waits until 512 samples are ready. Returns false on error or xrun (the
    // caller just skips the cycle; the next one works again).
    bool capturePeriod(std::vector<float> &buffer);

    // Returns the first real hardware capture interface (a "hw:") as a
    // "plughw:" name (so ALSA converts format/channels if the card does not
    // support the exact parameters). Returns "default" if nothing is found.
    // Avoids hardcoding the card name every time it is re-plugged.
    static std::string getPlugAndPlayDevice();

private:
    // Hardware parameters fixed by the project (see Config.h); shared with
    // FftProcessor and FeatureExtractor (Config::kWindowSize).
    const unsigned int m_sampleRate = Config::kSampleRate;
    const unsigned int m_channels = Config::kChannels; // mono
    const snd_pcm_uframes_t m_periodSize = Config::kWindowSize; // ~11.6ms

    snd_pcm_t *m_pcmHandle = nullptr;
    bool m_isInitialized = false;

    // On an xrun (ALSA buffer overrun: the CPU did not read in time) just
    // snd_pcm_prepare() to restart the device from a clean state.
    void handleXrun(int errorCode);

    // Lists the available INPUT interfaces (excludes "Output" devices). Used
    // only by getPlugAndPlayDevice().
    static std::vector<AudioDevice> scanDevices();
};

#endif // AUDIO_CAPTURE_H
