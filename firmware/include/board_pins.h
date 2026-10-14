// Pin maps for the two supported builds. Pick one with -DMALFOID_BOARD_* (see platformio.ini).
#pragma once

#if defined(MALFOID_BOARD_XIAO_S3)

// Build path A: Seeed Studio XIAO ESP32S3 + STEMMA QT breakouts. No custom PCB.
#define MALFOID_BOARD_NAME "xiao-s3"
namespace pins {
constexpr int kI2cSda = 5;          // D4
constexpr int kI2cScl = 6;          // D5
constexpr int kImuInt1 = 4;         // D3 - RTC GPIO, so motion can wake the wand from deep sleep
constexpr int kGripTouch = 1;       // D0 - touch channel 1, wired to the copper pad under the emerald
constexpr int kTipLed = 2;          // D1 - SK6812 data in
constexpr int kLedPower = -1;       // not switched on this build
constexpr int kHapticEnable = -1;   // DRV2605L breakout ties EN high
constexpr int kCastButton = 0;      // on-board BOOT button doubles as a cast trigger
constexpr int kChargeStatus = -1;   // XIAO's charger status LED is not routed to a GPIO
}  // namespace pins

#elif defined(MALFOID_BOARD_CORE)

// Build path B: Malfoid Core rev A (ESP32-S3-MINI-1-N8). See hardware/README.md.
#define MALFOID_BOARD_NAME "core-a"
namespace pins {
constexpr int kI2cSda = 8;
constexpr int kI2cScl = 9;
constexpr int kImuInt1 = 10;        // RTC GPIO
constexpr int kGripTouch = 4;       // TOUCH4 through 510 R to the thumb pad
constexpr int kTipLed = 38;         // via SN74LVC1T45 level shifter
constexpr int kLedPower = 39;       // high = LED rail on (drives the 2N7002 / P-FET switch)
constexpr int kHapticEnable = 11;   // DRV2605L EN, low = shutdown
constexpr int kCastButton = 0;      // BOOT button
constexpr int kChargeStatus = 12;   // MCP73831 STAT: low = charging
}  // namespace pins

#else
#error "Select a board: build with -DMALFOID_BOARD_XIAO_S3 or -DMALFOID_BOARD_CORE"
#endif
