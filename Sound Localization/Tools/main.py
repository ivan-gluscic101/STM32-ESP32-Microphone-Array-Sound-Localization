"""
main.py — Ulazna točka za Sound Localization vizualizator.
Implementirano: 2026-04-02

Pokretanje:
    python main.py
    python main.py COM5
"""

import config
from app import SoundLocalizationApp

if __name__ == "__main__":
    SoundLocalizationApp(config).run()
