# Padbox GS-C automatic GameCube / USB boot

This branch enables `HOJA_BOOT_AUTO_GAMECUBE_USB` only for Padbox GS-C.

With no mode-selection hold, the saved default is ignored. USB starts as a
Slippi/WUP-028 GameCube adapter while the GameCube data line also listens and
answers with neutral reports. A recognized GameCube probe/origin command or
complete poll triggers a one-shot wired reboot into native GameCube mode.
Once USB receives a setup request, the listener is released and adapter mode
remains selected until unplugged. There is no short detection timeout, so a
console that begins polling later can still be detected.

Normal boot holds use physical input labels, before gameplay remapping:

| Hold during plug-in | Mode |
| --- | --- |
| None | Native GameCube on GC/Wii; WUP-028 on PC/phone |
| A (`INPUT_CODE_SOUTH`, GPIO 6) | Switch USB |
| X (`INPUT_CODE_WEST`, GPIO 10) | XInput USB |
| Y (`INPUT_CODE_NORTH`, GPIO 11) | Force WUP-028 USB |
| B (`INPUT_CODE_EAST`, GPIO 7) | SInput USB/configurator |
| D-pad Right | Force native GameCube |
| D-pad Down | N64 |
| D-pad Left | SNES |
| Start + Select | UF2 bootloader |

The GameCube path has one brief automatic restart when first detected. It does
not save GameCube as the default or modify analog filtering, calibration, or
gameplay button maps. Subsequent cold boots detect the host again. A passive
USB power supply does not lock in USB; it must be a host sending USB setup
requests. Connecting both active USB and console hosts is outside the intended
use; a console command wins if both are observed in the same task iteration.

Build with Pico SDK 2.2.0:

```sh
bash tests/run-padbox-boot-tests.sh
cmake -S rp2040 -B build -G Ninja -DPICO_SDK_PATH=/path/to/pico-sdk -DCMAKE_BUILD_TYPE=Release
cmake --build build --target padbox_gs_c
```

The Actions workflow uploads `padbox-gs-c-auto-mode`, containing the UF2, BIN,
and manifest. Flash `padbox_gs_c.uf2`, then check:

1. PC direct USB, no hold: Dolphin/Slippi detects a WUP-028 adapter.
2. GC/Wii cable, no hold: native controller works after the automatic restart.
3. Power the console/controller before launching its GameCube software: late
   polling still selects native GameCube.
4. Switch and XInput holds still select their modes; gameplay remaps do not
   change which physical boot buttons to hold.
5. Unplug from console and reconnect to PC: automatic adapter mode returns.
6. Recheck calibration, Auto snapback, ledgedashes, and pivots on hardware.

Automated tests verify boot policy and the firmware build; physical transport
detection and input feel require the actual Padbox.
