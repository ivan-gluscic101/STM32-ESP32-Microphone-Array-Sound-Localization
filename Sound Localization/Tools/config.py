"""
config.py — Sve podešive konstante za Sound Localization vizualizator.
Implementirano: 2026-04-02

Uredi samo ovu datoteku kako bi promijenio portove, frekvencije uzorkovanja,
postavke prikaza i sl. Ostatak koda čita vrijednosti odavde.
"""

import sys
import math

# ── Serijska veza ─────────────────────────────────────────────────────────────
COM_PORT  = sys.argv[1] if len(sys.argv) > 1 else "COM8"
BAUD_RATE = 921600

# ── Hardver / firmware (mora se podudarati s main.c) ─────────────────────────
NUM_CHANNELS     = 4       # broj mikrofona / kanala
SAMPLES_PER_CHAN = 512     # uzoraka po kanalu po frameu
SAMPLE_RATE_HZ   = 16000  # frekvencija uzorkovanja ADC-a [Hz]
FRAME_SKIP       = 4       # šalje se svaki N-ti half-buffer
ADC_MAX          = 4095   # maksimalna vrijednost 12-bitnog ADC-a
VREF             = 3.3    # referentni napon ADC-a [V]

# ── Fizikalne konstante mikrofona (mora se podudarati s TwoDimSoundLoc.h) ────
# Promijeni MIC_DIST_M ovdje i sve izvedene vrijednosti se automatski ažuriraju
SPEED_OF_SOUND   = 343.0   # brzina zvuka pri sobnoj temperaturi [m/s]
MIC_DIST_M       = 0.20    # razmak između mikrofona [m] — trenutno 20 cm
CH_DELAY_S       = 0.9e-6  # ADC sekvencijalni channel offset [s]

# Izvedene fizikalne granice — koriste se za prikaz na sferi i sanity check
TDOA_MAX_S       = MIC_DIST_M / SPEED_OF_SOUND          # maks. TDOA [s]
TDOA_MAX_SAMPLES = TDOA_MAX_S * SAMPLE_RATE_HZ           # maks. TDOA [uzorci]
# Diskretni kutevi koje sustav može razlikovati (jedan uzorak TDOA razlike)
# Pri 20 cm i 16 kHz: TDOA_max = 583 µs = 9.3 uzoraka → ~18 razreda
ANGULAR_RES_DEG  = math.degrees(
    math.asin(min(1.0, (1 / SAMPLE_RATE_HZ) * SPEED_OF_SOUND / MIC_DIST_M))
)  # kutna razlučivost po jednom uzorku TDOA [°] (aproksimacija oko 0°)

# ── Mapiranje kanala ──────────────────────────────────────────────────────────
#   0 = mic1 / PA0    1 = mic2 / PB14
#   2 = mic3 / PC0    3 = mic4 / PC1
CHANNEL_NAMES  = ["mic1 PA0", "mic2 PB14", "mic3 PC0", "mic4 PC1"]
CHANNEL_COLORS = ["#1f77b4",  "#ff7f0e",   "#2ca02c",  "#d62728"]

# Kanali koje firmware šalje (podskup od 0-3)
DISPLAY_CHANNEL = [0, 1, 2, 3]

# Koliko kanala prikazati u waveform prikazu (1-4)
ACTIVE_CHANNEL_VIEW = 2

# ── Postavke prikaza valnog oblika ────────────────────────────────────────────
DISPLAY_SECONDS = 2    # duljina kliznog prozora [s]
Y_MIN           = 2750
Y_MAX           = 4095

# ── Postavke prikaza sfere ────────────────────────────────────────────────────
# Maksimalni broj točaka koje se čuvaju na sferi; najstarija se briše kad se prekorači
SPHERE_MAX_DOTS  = 50
# Boja i prozirnost nove točke smjera zvuka
SPHERE_DOT_COLOR    = "#e63946"
SPHERE_DOT_ALPHA    = 0.85
SPHERE_DOT_SIZE     = 80    # veličina markera (scatter)
SPHERE_DOT_LIFETIME = 0.5   # trajanje točke na sferi [s]

# ── Izvedene konstante (ne mijenjaj) ─────────────────────────────────────────
SOF = bytes([0xAA, 0xBB])
EOF = bytes([0xCC, 0xDD])

FRAME_DURATION_S  = SAMPLES_PER_CHAN / SAMPLE_RATE_HZ
EFFECTIVE_RATE_HZ = int(SAMPLES_PER_CHAN / (FRAME_SKIP * FRAME_DURATION_S))
DISPLAY_SAMPLES   = DISPLAY_SECONDS * EFFECTIVE_RATE_HZ
