#include "AudioCapture.h"
#include <iostream>
#include <cstring>

namespace
{
    // Tries one amixer control name (names differ per card, e.g. "Capture
    // Volume" on generic USB, "Mic Capture Volume" on Mackie/Onyx). Returns
    // true if the control exists and was set.
    bool trySetAmixerControl(const std::string &ctlDevice, const std::string &name, const std::string &value)
    {
        std::string cmd = "amixer -q -D " + ctlDevice + " cset name='" + name + "' " + value + " >/dev/null 2>&1";
        return std::system(cmd.c_str()) == 0;
    }

    // Sets capture volume to 100% and capture switch on, trying the known names
    // until one works. Returns true if at least one control was set.
    bool boostCaptureGain(const std::string &ctlDevice)
    {
        static const std::vector<std::string> volumeNames = {"Capture Volume", "Mic Capture Volume", "PCM Capture Volume"};
        static const std::vector<std::string> switchNames = {"Capture Switch", "Mic Capture Switch"};

        bool volumeOk = false;
        for (const auto &name : volumeNames)
        {
            if (trySetAmixerControl(ctlDevice, name, "100%"))
            {
                volumeOk = true;
                break;
            }
        }

        bool switchOk = false;
        for (const auto &name : switchNames)
        {
            if (trySetAmixerControl(ctlDevice, name, "on"))
            {
                switchOk = true;
                break;
            }
        }

        return volumeOk || switchOk;
    }
}

AudioCapture::AudioCapture() : m_pcmHandle(nullptr), m_isInitialized(false) {}

AudioCapture::~AudioCapture()
{
    closeDevice();
}

// Plug & play: pick the first real hardware card, as a "plughw:" name.
std::string AudioCapture::getPlugAndPlayDevice()
{
    auto devices = scanDevices();

    for (const auto &dev : devices)
    {
        if (dev.name.find("hw:") != std::string::npos)
        {
            std::string plugName = dev.name;
            size_t pos = plugName.find("hw:");
            plugName.replace(pos, 3, "plughw:");
            return plugName;
        }
    }

    // Nothing external connected: fall back to the system default.
    return "default";
}

std::vector<AudioDevice> AudioCapture::scanDevices()
{
    std::vector<AudioDevice> devices;
    void **hints;

    if (snd_device_name_hint(-1, "pcm", &hints) < 0)
    {
        std::cerr << "Errore durante la scansione dei dispositivi audio." << std::endl;
        return devices;
    }

    for (void **n = hints; *n != nullptr; ++n)
    {
        char *name = snd_device_name_get_hint(*n, "NAME");
        char *ioid = snd_device_name_get_hint(*n, "IOID");

        // IOID null means input+output; keep everything that is not Output-only.
        if (ioid == nullptr || std::strcmp(ioid, "Output") != 0)
        {
            AudioDevice dev;
            dev.name = name ? name : "Sconosciuto";
            devices.push_back(dev);
        }

        if (name)
            free(name);
        if (ioid)
            free(ioid);
    }

    snd_device_name_free_hint(hints);
    return devices;
}

bool AudioCapture::openDevice(const std::string &deviceName)
{
    snd_pcm_hw_params_t *hwParams = nullptr;
    int err;

    err = snd_pcm_open(&m_pcmHandle, deviceName.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0)
    {
        std::cerr << "ALSA ERROR [Apertura]: " << snd_strerror(err) << " (Codice: " << err << ")" << std::endl;
        return false;
    }

    snd_pcm_hw_params_alloca(&hwParams);
    snd_pcm_hw_params_any(m_pcmHandle, hwParams);
    snd_pcm_hw_params_set_access(m_pcmHandle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);

    err = snd_pcm_hw_params_set_format(m_pcmHandle, hwParams, SND_PCM_FORMAT_FLOAT_LE);
    if (err < 0)
    {
        std::cerr << "ALSA ERROR [Formato Float]: " << snd_strerror(err) << std::endl;
        closeDevice();
        return false;
    }

    err = snd_pcm_hw_params_set_channels(m_pcmHandle, hwParams, m_channels);
    if (err < 0)
    {
        std::cerr << "ALSA ERROR [Canali Mono]: " << snd_strerror(err) << " -> Molte schede pro pretendono 2 canali (Stereo) a livello hardware!" << std::endl;
        closeDevice();
        return false;
    }

    unsigned int rate = m_sampleRate;
    err = snd_pcm_hw_params_set_rate_near(m_pcmHandle, hwParams, &rate, nullptr);
    if (err < 0)
    {
        std::cerr << "ALSA ERROR [Sample Rate]: " << snd_strerror(err) << std::endl;
        closeDevice();
        return false;
    }

    snd_pcm_uframes_t frames = m_periodSize;
    err = snd_pcm_hw_params_set_period_size_near(m_pcmHandle, hwParams, &frames, nullptr);
    if (err < 0)
    {
        std::cerr << "ALSA ERROR [Period Size]: " << snd_strerror(err) << std::endl;
        closeDevice();
        return false;
    }

    err = snd_pcm_hw_params(m_pcmHandle, hwParams);
    if (err < 0)
    {
        std::cerr << "ALSA ERROR [Applicazione Parametri HW]: " << snd_strerror(err) << std::endl;
        closeDevice();
        return false;
    }

    // amixer without "-D" targets the system DEFAULT card (HDMI output on the
    // Pi, not the card we just opened), so always point it at the right device.
    // "ctl" controls are per-CARD, not per-PCM-subdevice: address them as
    // "hw:CARD=X" (no ",DEV=..."), even if the PCM was opened as
    // "plughw:CARD=X,DEV=0".
    std::string ctlDevice = deviceName;
    size_t plugPos = ctlDevice.find("plughw:");
    if (plugPos != std::string::npos)
    {
        ctlDevice.replace(plugPos, 7, "hw:");
    }
    size_t commaPos = ctlDevice.find(',');
    if (commaPos != std::string::npos)
    {
        ctlDevice.erase(commaPos);
    }

    // Only warn if no capture control was found; success stays silent.
    if (!boostCaptureGain(ctlDevice))
    {
        std::cerr << "[AVVISO] Nessun controllo di volume/capture riconosciuto su '" << ctlDevice
                  << "': il gain va regolato a mano sull'interfaccia o con 'amixer -D " << ctlDevice
                  << " controls'." << std::endl;
    }

    m_isInitialized = true;
    std::cout << "[OK] Audio: " << deviceName << " (" << m_sampleRate
              << " Hz, buffer " << m_periodSize << ")" << std::endl;
    return true;
}

void AudioCapture::closeDevice()
{
    if (m_pcmHandle)
    {
        snd_pcm_close(m_pcmHandle);
        m_pcmHandle = nullptr;
    }
    m_isInitialized = false;
}

bool AudioCapture::capturePeriod(std::vector<float> &buffer)
{
    if (!m_isInitialized || !m_pcmHandle)
    {
        return false;
    }

    buffer.resize(m_periodSize);

    snd_pcm_sframes_t framesLetti = snd_pcm_readi(m_pcmHandle, buffer.data(), m_periodSize);

    // Negative = problem (likely an overrun): recover and skip this cycle.
    if (framesLetti < 0)
    {
        handleXrun(static_cast<int>(framesLetti));
        return false;
    }

    // Success only if we read exactly the requested period.
    return (framesLetti == static_cast<snd_pcm_sframes_t>(m_periodSize));
}

void AudioCapture::handleXrun(int errorCode)
{
    // -EPIPE means overrun/underrun: prepare the device to restart cleanly.
    if (errorCode == -EPIPE)
    {
        std::cerr << "ALSA WARNING: [Overrun rilevato] La CPU ha perso un blocco! Tento il ripristino..." << std::endl;

        int err = snd_pcm_prepare(m_pcmHandle);
        if (err < 0)
        {
            std::cerr << "ALSA CRITICAL: Impossibile ripristinare il PCM dopo l'XRUN ("
                      << snd_strerror(err) << ")" << std::endl;
        }
        else
        {
            std::cout << "ALSA: Dispositivo ripristinato con successo." << std::endl;
        }
    }
    else
    {
        std::cerr << "ALSA ERROR: Errore sconosciuto durante la lettura ("
                  << snd_strerror(errorCode) << ")" << std::endl;
    }
}
