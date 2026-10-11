# DSTIKE Deauther Watch — personal enhancements

Customizations to the ESP8266 Deauther firmware for my DSTIKE Deauther Watch
(ESP8266, SH1106 I2C OLED, 3 buttons, CH9102 USB-serial → `/dev/ttyACM0`).
For testing my own 2.4 GHz network/devices only.

## What changed

All board-specific tweaks are guarded by `#ifdef DSTIKE_DEAUTHER_WATCH` in
`esp8266_deauther/A_config.h`, so other boards stay stock.

| Area | Change |
|------|--------|
| Boot splash | Subtitle `ru44 // deauther`; version line shows a build tag (`BUILD_TAG`) |
| Battery | `DISPLAY_TIMEOUT` 600 → 60 s (screen sleeps sooner) |
| LED | Brightness 10 → 40; per-mode colors (idle green / scan cyan / attack red) |
| Display: System Info | New menu page: firmware version, MAC, free RAM, uptime |
| Display: Fun mode | Fake-hacker "Hollywood" screensaver (Matrix rain → bogus console → ACCESS GRANTED). Cosmetic only — performs no scanning/attacks and touches no network. |

New display screens live in `esp8266_deauther/DisplayUI.{h,cpp}`
(`drawSysInfo()`, `drawFun()`), wired into the menu, `draw()` dispatch, and the
button-A exit path. Both are reachable from the main menu and exit with button A.

## Build & flash (arduino-cli)

```sh
arduino-cli compile \
  --fqbn deauther:esp8266:generic \
  --build-property "build.extra_flags=-DESP8266 -DDSTIKE_DEAUTHER_WATCH" \
  --output-dir build --upload --port /dev/ttyACM0 \
  esp8266_deauther
```

Requires the Spacehuhn board package
(`https://raw.githubusercontent.com/SpacehuhnTech/arduino/main/package_spacehuhn_index.json`)
and `arduino-cli core install deauther:esp8266`.
