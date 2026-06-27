#pragma once

#include "core/Config.h"
#include <array>
#include <chrono>
#include <string>
#include <vector>

// Forward declarations (libgpiod v2, header included only in the .cpp)
struct gpiod_chip;
struct gpiod_line_request;

// Reads the module's controls: 5 potentiometers on the MCP3008 (SPI ADC) and 2
// buttons on GPIO (see src/tests/mcp3008_test.cpp for the isolated test).
//
// Pots: exposed as 5 values normalized 0.0-1.0. Wired to the PHYSICAL channels
// CH0, CH1, CH3, CH5, CH7 of the MCP3008 (not contiguous, see
// Config::kPotChannels), but getValues()[i] is always indexed 0-4. Consumed by
// the Lua presets as knobs[1]..knobs[5].
//
// Buttons: not for the presets, but to change preset (Renderer cycles the list
// in assets/presets/). wasButtonPressed() returns true ONLY on the rising-edge
// frame (released -> pressed), with a minimum debounce time; reading it as a
// level would change preset dozens of times while the button is held.
//
// Polled once per frame via update() (see Renderer::render): no dedicated thread
// or mutex, since pots/buttons are hand-operated and reading them on the same
// thread that consumes them avoids any synchronization.
class InputManager {
public:
    static constexpr int kNumPots = Config::kNumPots;
    static constexpr int kNumButtons = Config::kNumButtons;

    // spiDevice/gpioChip: device paths. SPI (pots) and GPIO (buttons) are opened
    // INDEPENDENTLY: if one fails (e.g. SPI not enabled, or GPIO not
    // accessible), only that part is disabled (isOpen()/buttonsReady() false)
    // and the rest of the program keeps working.
    explicit InputManager(const std::string& spiDevice = "/dev/spidev0.0",
                          const std::string& gpioChip = "/dev/gpiochip0");
    ~InputManager();

    // Reads all 5 MCP3008 channels + the 2 button states. Call once per frame.
    // If isOpen()/buttonsReady() are false the corresponding part is a no-op
    // (values stay at 0/false).
    void update();

    bool isOpen() const { return m_spiFd >= 0; }
    bool buttonsReady() const { return m_buttonRequest != nullptr; }

    // All 5 normalized 0-1 values, in MCP3008 channel order: handy to pass to
    // the presets in one go.
    const std::vector<float>& getValues() const { return m_values; }

    // True ONLY on the frame where button 'index' (0 or 1) was freshly pressed
    // (debounced rising edge). Out of range or buttons not ready -> false.
    bool wasButtonPressed(int index) const;

private:
    // One MCP3008 channel read (3-byte SPI protocol, see mcp3008_test.cpp):
    // returns 0-1023, or -1 if the ioctl fails.
    int readChannel(int channel) const;

    void updateButtons();

    // --- Pots (SPI/MCP3008) ---
    int m_spiFd;
    std::vector<float> m_values; // smoothed 0.0-1.0, size kNumPots

    // Last raw reading (0-1023) ACCEPTED per pot (see Config::kPotDeadbandCounts):
    // tells "the pot really moved" from "ADC noise" BEFORE smoothing. -1 = no
    // reading accepted yet (the very first valid one is always accepted).
    // Without the deadband the value visibly jitters even with smoothing,
    // because the smoothing keeps chasing the ADC noise (typically +-2/3 steps
    // on a breadboard setup without a filter capacitor).
    std::array<int, kNumPots> m_lastAcceptedRaw;

    // --- Buttons (GPIO) ---
    struct gpiod_chip* m_gpioChip;
    struct gpiod_line_request* m_buttonRequest;

    std::array<bool, kNumButtons> m_buttonRawPrev;   // previous-frame level, for edge detection
    std::array<bool, kNumButtons> m_buttonEdge;      // true only on the debounced rising-edge frame
    std::array<std::chrono::steady_clock::time_point, kNumButtons> m_lastPressTime;
    // Debounce (Config::kButtonDebounceMs): absorbs mechanical bounce and avoids
    // double preset changes on a single press.
};
