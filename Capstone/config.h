#pragma once

// INMP441 I2S wiring
#define I2S_BCLK 26
#define I2S_WS   25
#define I2S_DIN  33

// Relay inputs (low-voltage isolated relay module only)
#define RELAY_LIGHT_PIN 18
#define RELAY_FAN_PIN   19
#define STATUS_LED_PIN  2

#define RELAY_ACTIVE_LOW true

// Audio / DSP settings (must match dsp_features.py)
#define SAMPLE_RATE 16000
#define FRAME_LEN 400
#define HOP_LEN 160
#define FFT_SIZE 512
#define N_MELS 20
#define N_MFCC 13
#define CAPTURE_SECONDS 1
#define CAPTURE_SAMPLES (SAMPLE_RATE * CAPTURE_SECONDS)

// Tune with real validation recordings
#define MAX_DISTANCE 22.0f
#define MIN_MARGIN 2.0f
#define COMMAND_LOCKOUT_MS 1800
#define VAD_RMS_THRESHOLD 0.008f
