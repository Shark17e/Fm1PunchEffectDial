# Fm1PunchEffectDial

A USB-host MIDI controller for SLOOP's **27 punch-in effects** and **track parameters**, built on an **M5Stack Dial** (ESP32-S3, 240x240 round display, touch screen and rotary encoder). One finger and one knob turn the M-VAVE FM-1 into a live performance tool.

The Dial is the **USB host**: it enumerates the FM-1 directly and sends plain MIDI (notes and CCs). No computer, no DAW, no MIDI adapter.

Companion project: [Fm1PunchEffect](https://github.com/Shark17e/Fm1PunchEffect) - the same idea on an AtomS3 + Unit Angle.

## How it works

SLOOP (2.7 and newer) plays the FM-1's 27 punch-in effects from MIDI: notes **F3..G5 (53..79)** on the channel set in **GLO > MIDI 3 > FX CH**. Track parameters follow the standard CC map, the panel buttons can be driven by CC 102..115 (**GLO > MIDI 3 > REMOTE**). This controller speaks all of it and adds a touch interface on top.

### Pages

The round screen is a set of pages. **Button A (a short press of the encoder button)** moves to the next page, wrapping around (FX → FILTER → ... → CONFIG → FX). The encoder does what the page is about: it scrolls the punch-in effects, changes the parameter pair, or changes a channel.

| Page | Controls |
|---|---|
| **FX** | the 27 punch-in effects |
| **FILTER** | CUT / RES |
| **SPACE** | REV / DLY |
| **MOTION** | CHOR / GLIDE |
| **MIX** | LEVEL / PAN |
| **ENV** | ATK / DEC |
| **RELEASE** | REL / GLIDE |
| **TRANSPORT** | PLAY, REC, OCT-, OCT+, reverb and echo THROW |
| **CONFIG** | the MIDI channels, saved on the device |

### FX page (the 27 punch-in effects)

| Gesture | Action |
|---|---|
| Touch the center | Momentary: the effect is on while touched, like holding an FM-1 key |
| Double-tap the center | Latch **HOLD** on the current effect (double-tap again to unlatch) |
| With HOLD active: short tap | Leave the HOLD |
| With HOLD active: touch + rotate | Scrub the effects live; the HOLD is restored on release |
| With HOLD active: rotate | The HOLD moves with the selection (live) |
| Rotate the encoder | Scroll through the 27 effects |
| Tap the outer ring | Jump to the effect at that position |
| Swipe left/right on the ring | Previous / next effect |
| Button A (short press) | Next page |
| Hold the encoder button 1 s | **PANIC**: all 27 effects off, HOLD released |

### Parameter pages (the XY pad)

The finger position is the value: **X** is the left value (red), **Y** is the up/down value (white). Touch to jump, drag to sweep.

| Gesture | Action |
|---|---|
| Touch / drag | Set both values live from the position |
| Release (no HOLD) | Both values are sent back to 0 immediately |
| Double-tap | **HOLD**: the values stay on release |
| Exit HOLD (double-tap) | The values stay; the next touch and release sends them to 0 again |
| Tap the chip on top | Target: **SEL** (the track selected on the FM-1) / **T1** / **T2** / **T3** / **DR** |
| Rotate the encoder | Change the parameter pair: the Y value cycles through the other parameters |
| Button A (short press) | Next page |

### TRANSPORT page

Momentary buttons (PC-style CC 102..115 on the REMOTE channel):

| Button | Sends |
|---|---|
| PLAY | CC 112 |
| REC | CC 113 |
| OCT- | CC 114 |
| OCT+ | CC 115 |
| THR-R | CC 102 + 114 held: reverb THROW |
| THR-E | CC 102 + 115 held: echo THROW |

### CONFIG page

The MIDI channels used by the controller, saved in flash (they survive power cycles):

| Row | Default | Must match on the FM-1 |
|---|---|---|
| FX | CH 16 | GLO > MIDI 3 > FX CH |
| REMOTE | CH 5 | GLO > MIDI 3 > REMOTE |
| SEL TRACK | CH 5 | any channel no track uses (GLO > MIDI 2 > OTHER = SEL) |
| TRACK 1/2/3 | CH 1 / 2 / 3 | GLO > MIDI > CH1 / CH2 / CH3 |
| DRUMS | CH 10 | GLO > DRUMS > CH |

Tap a row to select it, then turn the encoder to change its channel (1..16, wrapping).

## Parameter CC map

Same standard map SLOOP implements; the CCs act on the track the channel plays (or on the selected track, with the SEL target):

| CC | Parameter | CC | Parameter |
|---|---|---|---|
| 74 | CUT (engine cutoff / track filter) | 73 | ATK |
| 71 | RES / Q | 75 | DEC |
| 7 | LEVEL | 72 | REL |
| 10 | PAN | 5 | GLIDE |
| 91 | REV send | 93 | CHORUS send |
| 94 | DELAY send | | |

## FX map

Same layout as the FM-1's keys: white keys = effects 1-16, black keys = effects 17-27.

| # | MIDI note | FX | | # | MIDI note | FX |
|---|---|---|---|---|---|---|
| 1 | F3 (53) | LOOP 4 | | 15 | F5 (77) | ECHO |
| 2 | G3 (55) | LOOP 8 | | 16 | G5 (79) | WOBBLE |
| 3 | A3 (57) | LOOP 16 | | 17 | F#3 (54) | KILL LOW |
| 4 | B3 (59) | LOOP 32 | | 18 | G#3 (56) | KILL MID |
| 5 | C4 (60) | STUTTER | | 19 | A#3 (58) | KILL HIGH |
| 6 | D4 (62) | REVERSE | | 20 | C#4 (61) | REWIND |
| 7 | E4 (64) | STOP | | 21 | D#4 (63) | OCT UP |
| 8 | F4 (65) | HALF | | 22 | F#4 (66) | FLANGER |
| 9 | G4 (67) | LOW | | 23 | G#4 (68) | AUTO-PAN |
| 10 | A4 (69) | HIGH | | 24 | A#4 (70) | FREEZE |
| 11 | B4 (71) | PHONE | | 25 | C#5 (73) | LOOP 8T |
| 12 | C5 (72) | CRUSH | | 26 | D#5 (75) | MONO |
| 13 | D5 (74) | ALIAS | | 27 | F#5 (78) | FADE OUT |
| 14 | E5 (76) | GATE | | | | |

## Hardware

- M5Stack Dial (ESP32-S3, 8 MB flash, 240x240 round display, FT3267 touch, rotary encoder)
- The FM-1 on the Dial's USB-C port

### Power

The Dial is the USB host, so its USB-C port is taken by the FM-1. Power the Dial from:

- an internal LiPo battery (MX1.25-2P connector), or
- the rear screw terminal (6-36 V).

## FM-1 setup (SLOOP)

1. Install SLOOP 2.7 (or newer) on the M-VAVE FM-1 - https://github.com/isod89/sloop-fm1
2. Set **GLO > MIDI 3 > FX CH = 16** and **REMOTE = CH 5**; leave **CH1 = 1, CH2 = 2, CH3 = 3** and **DRUMS > CH = 10**; **MIDI 2 > IN = NOTES**, **OTHER = SEL**. These are the controller's defaults; any set can be changed from the **CONFIG** page and the FM-1 must match.
3. Connect the FM-1 to the Dial with a USB-C cable and power the Dial from battery / terminal.

## Build & flash

Tested with Arduino IDE 2.x / arduino-cli 1.1.1 and **esp32 core 3.3.8**.

Libraries:
- M5Dial
- M5Unified (includes M5GFX)
- [ESP32_Host_MIDI](https://github.com/sauloverissimo/ESP32_Host_MIDI) >= 7.3.0
- Preferences (bundled with the esp32 core)

Board settings (Tools menu, board "ESP32S3 Dev Module"):
- **USB Mode: "Hardware CDC and JTAG"** (required, as on the AtomS3 version)
- PSRAM: Disabled
- Flash Size: 8MB
- Partition Scheme: Default 8MB with spiffs (3MB APP/1.5MB SPIFFS)

With arduino-cli:

```
arduino-cli compile -u -p <PORT> --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=default,PSRAM=disabled,FlashSize=8M,PartitionScheme=default_8MB" .
```

### Flashing notes

- **The COM port disappears** as soon as the sketch starts hosting: the USB-C port becomes the host port for the FM-1. The display is the debug output.
- On boot the controller shows **TAP TO START** and waits: the USB port is not taken over until you tap, so the serial port stays alive and the board can be reflashed by just reconnecting the cable and flashing before tapping.
- If the sketch ever fails to boot, the Dial can be put in **download mode** by holding the **G0 button on the StampS3 module** (on the back) while plugging the USB-C cable, then releasing it.

## Notes on the implementation

- **Raw touch reads** - M5Unified's touch detail freezes `x/y` until a gesture is classified (touch/hold/flick): the pad reads the raw touch point instead, which the driver refreshes on every poll, so the finger position always tracks.
- **Outbox FIFO** - ESP32_Host_MIDI's send calls return false while a USB transfer is in flight, so notes and CCs are queued in ring buffers and flushed every loop; the pad throttles CC traffic to one batch every ~20 ms.
- **Matched off/on pairs** - SLOOP's FX handler only honors a note-off that matches the note the FX channel is holding, so the controller always sends a matched off before the next on (and the panic sends all 27 offs).
- **Channel config** - the CONFIG page writes the channels to NVS (flash) with the Arduino Preferences library.
- The FM-1's own keys always win: pressing an FX key on the FM-1 takes the effect over.

## Credits

- [SLOOP](https://github.com/isod89/sloop-fm1) - the FM-1 firmware this controller speaks to (GPL-3.0), by isod89, based on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hugelton Instruments.
- [ESP32_Host_MIDI](https://github.com/sauloverissimo/ESP32_Host_MIDI) - USB host MIDI stack by sauloverissimo.
- M5Dial / M5Unified / M5GFX by M5Stack.

M-VAVE and FM-1 are trademarks of their owners; this project is not affiliated with M-VAVE or with the SLOOP project.

## License

MIT - see [LICENSE](LICENSE).
