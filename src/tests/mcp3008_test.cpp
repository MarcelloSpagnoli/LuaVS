// mcp3008_test.cpp
//
// Standalone test: reads the pot on MCP3008 CH0 (via SPI) and two buttons wired
// directly to two GPIO pins of the Raspberry Pi 5 (digital read).
//
// Button wiring (ACTIVE-HIGH: pressed = HIGH):
//   - Button 1: one leg to 3.3V, the other to GPIO17 (physical pin 11)
//   - Button 2: one leg to 3.3V, the other to GPIO27 (physical pin 13)
//   Uses the internal pull-down, so no external resistor is needed: idle reads
//   low (0), pressing pulls it to 3.3V and the program reads "pressed".
//
// Dependency: sudo apt install libgpiod-dev

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>
#include <thread>
#include <chrono>

// ---------- MCP3008 (pot on CH0, via SPI) ----------

int open_spi(const char* device) {
    int fd = open(device, O_RDWR);
    if (fd < 0) {
        std::perror("Apertura SPI fallita");
        return -1;
    }
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = 1350000; // 1.35 MHz
    ioctl(fd, SPI_IOC_WR_MODE, &mode);
    ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    return fd;
}

int read_channel(int fd, int channel) {
    uint8_t tx[3] = {0x01, static_cast<uint8_t>((0x08 | channel) << 4), 0x00};
    uint8_t rx[3] = {0, 0, 0};

    struct spi_ioc_transfer tr;
    std::memset(&tr, 0, sizeof(tr));
    tr.tx_buf = reinterpret_cast<unsigned long>(tx);
    tr.rx_buf = reinterpret_cast<unsigned long>(rx);
    tr.len = 3;
    tr.speed_hz = 1350000;
    tr.bits_per_word = 8;

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        std::perror("Lettura SPI fallita");
        return -1;
    }
    return ((rx[1] & 0x03) << 8) | rx[2];
}

// ---------- Buttons on GPIO (libgpiod v2, character-device API) ----------

static const char* GPIO_CHIP = "/dev/gpiochip0";
static const unsigned int BUTTON1_OFFSET = 17; // GPIO17, physical pin 11
static const unsigned int BUTTON2_OFFSET = 27; // GPIO27, physical pin 13

struct gpiod_line_request* setup_buttons(struct gpiod_chip** chip_out) {
    struct gpiod_chip* chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        std::perror("Apertura del chip GPIO fallita");
        return nullptr;
    }

    struct gpiod_line_settings* settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_DOWN);
    // No active_low: active-high signal, pressed = ACTIVE when it goes to 3.3V

    unsigned int offsets[2] = {BUTTON1_OFFSET, BUTTON2_OFFSET};

    struct gpiod_line_config* line_cfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(line_cfg, offsets, 2, settings);

    struct gpiod_request_config* req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "test_bottoni");

    struct gpiod_line_request* request =
        gpiod_chip_request_lines(chip, req_cfg, line_cfg);

    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);

    if (!request) {
        std::perror("Richiesta delle linee GPIO fallita");
        gpiod_chip_close(chip);
        return nullptr;
    }

    *chip_out = chip;
    return request;
}

int main() {
    int spi_fd = open_spi("/dev/spidev0.0");
    if (spi_fd < 0) {
        std::fprintf(stderr, "Controlla che SPI sia abilitato (raspi-config).\n");
        return 1;
    }

    struct gpiod_chip* gpio_chip = nullptr;
    struct gpiod_line_request* buttons = setup_buttons(&gpio_chip);
    if (!buttons) {
        std::fprintf(stderr, "Controlla che %s esista e che tu abbia i permessi.\n", GPIO_CHIP);
        close(spi_fd);
        return 1;
    }

    std::printf("Gira il potenziometro e premi i bottoni (Ctrl+C per uscire)\n\n");

    while (true) {
        int raw = read_channel(spi_fd, 0);
        double percent = (raw / 1023.0) * 100.0;

        bool b1 = (gpiod_line_request_get_value(buttons, BUTTON1_OFFSET) == GPIOD_LINE_VALUE_ACTIVE);
        bool b2 = (gpiod_line_request_get_value(buttons, BUTTON2_OFFSET) == GPIOD_LINE_VALUE_ACTIVE);

        std::printf("pot=%4d (%.0f%%)   bottone1=%-10s   bottone2=%-10s\n",
                    raw, percent,
                    b1 ? "premuto" : "rilasciato",
                    b2 ? "premuto" : "rilasciato");

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    gpiod_line_request_release(buttons);
    gpiod_chip_close(gpio_chip);
    close(spi_fd);
    return 0;
}