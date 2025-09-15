## LGFX_PPA

This library provides a M5GFX/LovyanGFX layer for PPA, the pixel-processing accelerator module shipped with ESP32-P4 for fast image rotation, scaling, mirroring, and blending.

### Requirements:

- ESP32-P4
- MIPI/DSI panel supported by M5Unified or LovyanGFX (e.g. M5Stack Tab5)
- PSRAM (minimum 4MB)
- esp-idf version >= 5.4

### Usage

See examples folder.


### Developer notes

`PPA_Sprite` inherits from `LGFX_Sprite`, however the buffer is aligned specially for ppa operation, and the bit depth is limited to 16-24-32 bits colors.


### Known bugs

- ppa operations trigger screen flashes with arduino-esp32 v3.3.0 => downgrade to arduino-esp32 v3.2.1
- ppa_srm crashes when mirror_x or mirror_y is set and rotation is odd => wait for arduino-esp32 v3.3.1
- ppa_srm output may be misaligned when reused by ppa_blend => wait for arduino-esp32 v3.3.1

### Resources:

- https://github.com/Lovyan03/LovyanGFX
- https://github.com/M5Stack/M5GFX
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/ppa.html
