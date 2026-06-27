#include "InputManager.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <gpiod.h>
#include <iostream>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

InputManager::InputManager(const std::string& spiDevice, const std::string& gpioChip)
    : m_spiFd(-1), m_values(kNumPots, 0.0f),
      m_gpioChip(nullptr), m_buttonRequest(nullptr)
{
    m_lastAcceptedRaw.fill(-1);
    m_buttonRawPrev.fill(false);
    m_buttonEdge.fill(false);
    m_lastPressTime.fill(std::chrono::steady_clock::now());

    // --- Pots ---
    m_spiFd = open(spiDevice.c_str(), O_RDWR);
    if (m_spiFd < 0) {
        std::cerr << "[INPUT] Impossibile aprire " << spiDevice
                  << " (SPI abilitato in raspi-config? MCP3008 collegato?)."
                  << " I potenziometri non saranno disponibili." << std::endl;
    } else {
        uint8_t mode = SPI_MODE_0;
        uint8_t bits = 8;
        uint32_t speed = Config::kSpiSpeedHz;
        ioctl(m_spiFd, SPI_IOC_WR_MODE, &mode);
        ioctl(m_spiFd, SPI_IOC_WR_BITS_PER_WORD, &bits);
        ioctl(m_spiFd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

        std::cout << "[OK] InputManager: MCP3008 pronto su " << spiDevice << std::endl;
    }

    // --- Buttons ---
    // Same setup as the test (gpiod v2, character-device API): input, internal
    // pull-down (active-high, pressed = 3.3V), no external resistors needed.
    m_gpioChip = gpiod_chip_open(gpioChip.c_str());
    if (!m_gpioChip) {
        std::cerr << "[INPUT] Impossibile aprire " << gpioChip
                  << ". I bottoni non saranno disponibili." << std::endl;
        return;
    }

    struct gpiod_line_settings* settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_DOWN);

    unsigned int offsets[kNumButtons] = {Config::kButtonGpioOffsets[0], Config::kButtonGpioOffsets[1]};

    struct gpiod_line_config* lineCfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(lineCfg, offsets, kNumButtons, settings);

    struct gpiod_request_config* reqCfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(reqCfg, "LuaVS");

    m_buttonRequest = gpiod_chip_request_lines(m_gpioChip, reqCfg, lineCfg);

    gpiod_request_config_free(reqCfg);
    gpiod_line_config_free(lineCfg);
    gpiod_line_settings_free(settings);

    if (!m_buttonRequest) {
        std::cerr << "[INPUT] Richiesta delle linee GPIO fallita (permessi? gia' in uso?)."
                  << " I bottoni non saranno disponibili." << std::endl;
        gpiod_chip_close(m_gpioChip);
        m_gpioChip = nullptr;
        return;
    }

    std::cout << "[OK] InputManager: bottoni pronti su GPIO"
              << Config::kButtonGpioOffsets[0] << "/GPIO" << Config::kButtonGpioOffsets[1] << std::endl;
}

InputManager::~InputManager() {
    if (m_spiFd >= 0) close(m_spiFd);
    if (m_buttonRequest) gpiod_line_request_release(m_buttonRequest);
    if (m_gpioChip) gpiod_chip_close(m_gpioChip);
}

int InputManager::readChannel(int channel) const {
    // MCP3008 3-byte protocol: start bit + (single-ended | channel), then 2
    // response bytes holding the 10-bit result (same as mcp3008_test.cpp).
    uint8_t tx[3] = {0x01, static_cast<uint8_t>((0x08 | channel) << 4), 0x00};
    uint8_t rx[3] = {0, 0, 0};

    struct spi_ioc_transfer tr;
    std::memset(&tr, 0, sizeof(tr));
    tr.tx_buf = reinterpret_cast<unsigned long>(tx);
    tr.rx_buf = reinterpret_cast<unsigned long>(rx);
    tr.len = 3;
    tr.speed_hz = Config::kSpiSpeedHz;
    tr.bits_per_word = 8;

    if (ioctl(m_spiFd, SPI_IOC_MESSAGE(1), &tr) < 0) return -1;
    return ((rx[1] & 0x03) << 8) | rx[2];
}

void InputManager::update() {
    if (isOpen()) {
        for (int i = 0; i < kNumPots; ++i) {
            int raw = readChannel(Config::kPotChannels[i]);
            if (raw < 0) continue; // failed read this frame, keep previous value

            // Deadband: accept the new reading only if it moved far enough from
            // the last accepted one (Config::kPotDeadbandCounts), so ADC noise
            // does not even become a target for the smoothing below. -1 = very
            // first reading, always accepted.
            if (m_lastAcceptedRaw[i] < 0 || std::abs(raw - m_lastAcceptedRaw[i]) >= Config::kPotDeadbandCounts) {
                m_lastAcceptedRaw[i] = raw;
            }

            float target = m_lastAcceptedRaw[i] / 1023.0f;
            m_values[i] = m_values[i] + Config::kPotSmoothing * (target - m_values[i]);
        }
    }

    updateButtons();
}

void InputManager::updateButtons() {
    m_buttonEdge.fill(false);
    if (!buttonsReady()) return;

    auto now = std::chrono::steady_clock::now();

    for (int i = 0; i < kNumButtons; ++i) {
        bool raw = (gpiod_line_request_get_value(m_buttonRequest, Config::kButtonGpioOffsets[i]) == GPIOD_LINE_VALUE_ACTIVE);
        bool risingEdge = raw && !m_buttonRawPrev[i];
        m_buttonRawPrev[i] = raw;

        if (!risingEdge) continue;

        auto sinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastPressTime[i]).count();
        if (sinceLast >= Config::kButtonDebounceMs) {
            m_buttonEdge[i] = true;
            m_lastPressTime[i] = now;
        }
    }
}

bool InputManager::wasButtonPressed(int index) const {
    if (index < 0 || index >= kNumButtons) return false;
    return m_buttonEdge[index];
}
