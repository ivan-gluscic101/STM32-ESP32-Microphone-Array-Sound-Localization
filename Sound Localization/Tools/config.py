"""
config.py — Sve podešive konstante za Sound Localization vizualizator.

ISPRAVKE:
  1. EFFECTIVE_RATE_HZ popravljen: bio je SAMPLES_PER_CHAN/(FRAME_SKIP*FRAME_DURATION_S)
     = 4000, ali to je SAMPLE_RATE_HZ, ne FPS. Ispravno: 1/(FRAME_SKIP*FRAME_DURATION_S)
     = 1/0.128 ≈ 7.8125 FPS.
  2. DISPLAY_SAMPLES preimenovan u DISPLAY_FRAMES za jasnoću — ovo je broj
     frameova koji stane u DISPLAY_SECONDS, ne broj audio uzoraka.
  3. ROLLING_BUFFER_LEN dodan — ukupan broj uzoraka u rolling bufferu
     = DISPLAY_FRAMES × SAMPLES_PER_CHAN. Ovo je prava dimenzija za numpy niz.
"""

import sys
import math

# ── Serijska veza ─────────────────────────────────────────────────────────────
COM_PORT  = sys.argv[1] if len(sys.argv) > 1 else "COM8"
BAUD_RATE = 921600

# ── Hardver / firmware (mora se podudarati s audio_common.h) ──────────────────
NUM_CHANNELS     = 4
SAMPLES_PER_CHAN = 512
SAMPLE_RATE_HZ   = 64000   # TIM8 ARR=2655 → 64 kHz trigger; scan mode → 64 kHz/kanalu
FRAME_SKIP       = 16      # vrijedi samo kad je SEND_AUDIO_FRAMES=1 u firmwareu
ADC_MAX          = 4095
VREF             = 3.3

# ── Prikaz (Python strana) ────────────────────────────────────────────────────
SHOW_WAVEFORM    = False   # True = prikaži valne oblike; False = samo kut+jakost

# ── Fizikalne konstante mikrofona (mora se podudarati s audio_common.h) ───────
SPEED_OF_SOUND   = 343.0
MIC_DIST_M       = 0.20
CH_DELAY_S       = 0.9e-6

# Izvedene fizikalne granice
TDOA_MAX_S       = MIC_DIST_M / SPEED_OF_SOUND
TDOA_MAX_SAMPLES = TDOA_MAX_S * SAMPLE_RATE_HZ
ANGULAR_RES_DEG  = math.degrees(
    math.asin(min(1.0, (1 / SAMPLE_RATE_HZ) * SPEED_OF_SOUND / MIC_DIST_M))
)

# ── Mapiranje kanala ──────────────────────────────────────────────────────────
CHANNEL_NAMES  = ["mic1 PA0", "mic2 PB14", "mic3 PC0", "mic4 PC1"]
CHANNEL_COLORS = ["#1f77b4",  "#ff7f0e",   "#2ca02c",  "#d62728"]
DISPLAY_CHANNEL = [0, 1, 2, 3]
ACTIVE_CHANNEL_VIEW = 2

# ── Postavke prikaza valnog oblika ────────────────────────────────────────────
DISPLAY_SECONDS = 2
Y_MIN           = 0       # ISPRAVKA: bilo 2750 — sada puni raspon ADC-a
Y_MAX           = 4095

# ── Postavke prikaza smjera zvuka ─────────────────────────────────────────────
SPHERE_MAX_DOTS      = 50
SPHERE_DOT_COLOR     = "#e63946"
SPHERE_DOT_ALPHA     = 0.85
SPHERE_DOT_SIZE      = 80
SPHERE_DOT_LIFETIME  = 2.0   # ISPRAVKA: bilo 0.5s — prekratko za praćenje

# ── Izvedene konstante ───────────────────────────────────────────────────────
SOF = bytes([0xAA, 0xBB])
EOF = bytes([0xCC, 0xDD])

FRAME_DURATION_S  = SAMPLES_PER_CHAN / SAMPLE_RATE_HZ         # 0.032 s

# ISPRAVKA: Ovo je frekvencija DOLAZEĆIH FRAMEOVA, ne sample rate
EFFECTIVE_FPS     = 1.0 / (FRAME_SKIP * FRAME_DURATION_S)     # ≈ 7.8125 FPS
DISPLAY_FRAMES    = int(DISPLAY_SECONDS * EFFECTIVE_FPS)       # ≈ 15 frameova

# Ukupan broj uzoraka u rolling bufferu za prikaz
ROLLING_BUFFER_LEN = DISPLAY_FRAMES * SAMPLES_PER_CHAN         # ≈ 7680 uzoraka

# Backward compatibility
DISPLAY_SAMPLES   = ROLLING_BUFFER_LEN
EFFECTIVE_RATE_HZ = EFFECTIVE_FPS