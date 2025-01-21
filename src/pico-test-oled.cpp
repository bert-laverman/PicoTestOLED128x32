/*
 * Copyright (c) 2024 by Bert Laverman. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <boards/pico.h>
#include <pico/stdlib.h>
#include <pico/unique_id.h>

#include <cstdint>
#include <random>
#include <vector>
#include <array>
#include <list>

#include <raspberry-pi.hpp>
#include <interfaces/pico-spi.hpp>

#include <components/local-led.hpp>

#include <devices/ssd1305.hpp>


using namespace nl::rakis::raspberrypi;
using namespace nl::rakis::raspberrypi::interfaces;
using namespace nl::rakis::raspberrypi::devices;



#if !defined(TARGET_PICO)
#error "This example is for the Raspberry Pi Pico only"
#endif
#if !defined(HAVE_SPI)
#error "This example needs SPI support enabled"
#endif
#if !defined(HAVE_SSD1305)
#error "This example needs SSD1305 support enabled"
#endif




static void errorExit(RaspberryPi& berry, components::LocalLed& led, unsigned numBlips) {
    while (true) {
        for (unsigned i = 0; i < numBlips; i++) {
            led.on();
            berry.sleepMs(500);
            led.off();
            berry.sleepMs(500);
            led.on();
            berry.sleepMs(500);
            led.off();
            berry.sleepMs(500);
        }

        berry.sleepMs(1000);
    }
}


static constexpr uint8_t NUM_MODULES = 1;

int main([[maybe_unused]] int argc, [[maybe_unused]] char*argv[])
{
    RaspberryPi& berry(RaspberryPi::instance());

    stdio_init_all();
    berry.sleepMs(1000);

    printf("Starting up.\n");

    components::LocalLed internalLed(berry, PICO_DEFAULT_LED_PIN);


    try {
        auto spi = berry.addSPI<PicoSPI>("pico-spi-0");
        spi->baudRate(20'000'000); // 20MHz

        spi->open();
        spi_set_format(spi0, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);

        auto oled = std::make_shared<devices::SSD1305<>>();
        spi->device(oled);
        //spi->verbose(true);
        printf("SPI channel 0 using TX=%d, CLK=%d, CS=%d\n", spi->mosiPin(), spi->sclkPin(), spi->csPin());

        printf("Resetting display. Display is using DC=%d and RST=%d\n", oled->dcPin(), oled->rstPin());
        oled->reset();
        oled->sendImmediately(false);

        berry.sleepMs(500);

        printf("Sending first screen.\n");
        oled->clear();
        for (unsigned x = 0; x < oled->width(); x++) {
            oled->set(x, 0); oled->set(x, oled->height()-1);
        }
        for (unsigned y = 0; y < oled->height(); y++) {
            oled->set(0, y); oled->set(oled->width()-1, y);
        }
        oled->sendBuffer();

        unsigned numBlips = 0;
        unsigned x{1};
        unsigned y{1};
        unsigned dx{1};
        unsigned dy{1};

        auto startTs{time_us_64()};
        long frame{0};
        while (true) {
            if (numBlips >= 5) {
                internalLed.toggle();
                numBlips = 0;
                if (internalLed.state()) {
                    auto usSinceStart{time_us_64() - startTs};
                    printf("%ld FPS\n", long(frame / (usSinceStart / 1000000)));
                }
            }
            oled->reset(x, y);
            x += dx;
            if (x >= oled->width()) { dx = -dx; x += dx+dx; }
            y += dy;
            if (y >= oled->height()) { dy = -dy; y += dy+dy; }
            oled->set(x, y);
            oled->sendBuffer();
            frame++;

            berry.sleepMs(100);
            numBlips++;
        }
    }
    catch (std::runtime_error& e) {
        printf("Runtime error: %s\n", e.what());
        errorExit(berry, internalLed, 2);
    }
    catch (...) {
        printf("Unknown exception caught\n");
        errorExit(berry, internalLed, 3);
    }
}
