"""
main.py — Ulazna točka za Sound Localization vizualizator.

Pokretanje:
    python main.py
    python main.py COM5
"""

import config
from app import SoundLocalizationApp

if __name__ == "__main__":
    SoundLocalizationApp(config).run()