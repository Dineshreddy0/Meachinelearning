# ESP32 Real-Time DSP Voice Command Recognition & Appliance Control

This is a practical starter implementation for the capstone described in the supplied presentation.

## Selected hardware path
- ESP32 DevKit V1 / ESP32-WROOM-32
- INMP441 I²S digital microphone
- 2-channel isolated relay module (use a properly enclosed, correctly rated module)
- Optional status LED

The implementation uses **I²S audio capture → DC removal → Hamming window → FFT → Mel filter-bank log energies → DCT (13 MFCCs) → nearest-centroid classifier → confidence/margin check → relay lockout**.

Commands: `LIGHT ON`, `LIGHT OFF`, `FAN ON`, `FAN OFF`.

> This is a prototype, not a certified mains controller. Never connect mains wiring to a breadboard or directly to the ESP32. Have mains-side installation performed by a qualified person. ESP32 GPIO must only drive the isolated relay module's low-voltage input. Test first with low-voltage lamps/LED loads.

## 1. Wiring

### INMP441 → ESP32
| INMP441 | ESP32 |
|---|---|
| VDD | 3V3 |
| GND | GND |
| SCK/BCLK | GPIO 26 |
| WS/LRCL | GPIO 25 |
| SD | GPIO 33 |
| L/R | GND (left channel) |

### Relay module inputs
| Relay input | ESP32 |
|---|---|
| IN1 (light) | GPIO 18 |
| IN2 (fan) | GPIO 19 |
| GND | GND |
| VCC | Follow the module specification |

The code assumes **active-LOW relay inputs**. Change `RELAY_ACTIVE_LOW` in `config.h` if your module is active-HIGH. Verify relay input voltage compatibility with ESP32 3.3 V GPIO.

## 2. Prepare Python environment

Python 3.10+ recommended:

```bash
python -m venv .venv
# Windows: .venv\\Scripts\\activate
# Linux/macOS: source .venv/bin/activate
pip install -r requirements.txt
```

## 3. Record training samples

Record each command in a quiet room, keeping the microphone about 20–40 cm away. Collect at least 30–50 recordings per command, ideally from multiple speakers and distances.

```bash
python record_audio.py --label LIGHT_ON --count 40
python record_audio.py --label LIGHT_OFF --count 40
python record_audio.py --label FAN_ON --count 40
python record_audio.py --label FAN_OFF --count 40
```

Follow the prompts. Samples are saved under `dataset/<LABEL>/`. Add `UNKNOWN` examples (ordinary conversation/noise) if you want to evaluate rejection; they are not used as positive command centroids.

## 4. Train and export model

```bash
python train_model.py
```

The script creates `firmware/VoiceAppliance/model.h` with one mean MFCC centroid per command. It also prints a simple leave-one-out training estimate. This is only a development check; report final performance using a separate held-out test set, as required by the project presentation.

Copy the generated `model.h` into the Arduino sketch folder if it is generated elsewhere.

## 5. Flash firmware

1. Install Arduino IDE.
2. Install **ESP32 by Espressif Systems** board package. The sketch targets Arduino-ESP32 2.x (legacy I²S driver API).
3. Open `firmware/VoiceAppliance/VoiceAppliance.ino`.
4. Select an ESP32 Dev Module board and the correct serial port.
5. Confirm pins in `config.h`, then upload.
6. Open Serial Monitor at 115200 baud.

Say one of the four commands. Serial Monitor prints the predicted label, distance, and acceptance status. The relay output changes only when a command passes the score threshold and lockout.

## 6. Calibration

The distance threshold depends on the microphone, room, speaker, and training set. Start with `MAX_DISTANCE` in `config.h`, observe Serial Monitor, and tune it using validation recordings. The firmware also requires a distance margin between the best and second-best classes. If false activations occur, lower `MAX_DISTANCE`, increase `MIN_MARGIN`, collect more varied samples, and improve VAD/noise handling.

## 7. Project files

- `record_audio.py` – records 16 kHz mono WAV files.
- `train_model.py` – extracts MFCC features and exports the centroid model.
- `dsp_features.py` – Python reference feature implementation.
- `firmware/VoiceAppliance/VoiceAppliance.ino` – ESP32 acquisition, DSP, recognition, relay logic.
- `firmware/VoiceAppliance/config.h` – pin and threshold configuration.
- `firmware/VoiceAppliance/model.h` – generated model placeholder (replace by training output).

## Notes / limitations

- This baseline uses averaged MFCCs and nearest-centroid classification. It is intentionally compact, but less powerful than a trained tiny neural network or DTW.
- The same sample rate, frame length, hop, Mel filter count, and DCT coefficient count must be used in training and firmware.
- Use held-out recordings to measure accuracy, confusion matrix, false activations, and median end-to-end latency.
- If your board is ESP32-S3 or your microphone is analog, pin assignments and audio driver setup will differ.
