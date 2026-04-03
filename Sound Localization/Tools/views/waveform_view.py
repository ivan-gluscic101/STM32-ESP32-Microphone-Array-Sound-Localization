"""
waveform_view.py — Pomični osciloskopski prikaz N kanala mikrofona.
Implementirano: 2026-04-02
"""

import numpy as np
import matplotlib.pyplot as plt


class WaveformView:
    """
    Prikazuje jedan složeni subplot valnog oblika po aktivnom mikrofonu.

    Parametri
    ---------
    axes            : lista matplotlib Axes objekata (jedan po aktivnom kanalu)
    active_channels : list[int] — indeksi kanala koji se prikazuju (0–3)
    cfg             : config modul (ili bilo koji objekt s potrebnim atributima)
    """

    def __init__(self, axes, active_channels: list[int], cfg):
        self.axes            = axes
        self.active_channels = active_channels
        self.cfg             = cfg
        self._lines          = []
        self._status_txt     = None   # ubacuje App nakon što kreira figure text

        # Vremenski vektor za x-os
        t     = np.linspace(0.0, cfg.DISPLAY_SECONDS, cfg.DISPLAY_SAMPLES, endpoint=False)
        zeros = np.zeros(cfg.DISPLAY_SAMPLES)

        for i, ch in enumerate(active_channels):
            ax = axes[i]

            # Linija signala za ovaj kanal
            ln, = ax.plot(t, zeros, lw=0.7, color=cfg.CHANNEL_COLORS[ch])
            self._lines.append(ln)

            # Oznake i granice osi
            ax.set_ylabel(cfg.CHANNEL_NAMES[ch], fontsize=7.5, labelpad=4)
            ax.set_xlim(0, cfg.DISPLAY_SECONDS)
            ax.set_ylim(cfg.Y_MIN, cfg.Y_MAX)
            ax.set_yticks([2750, 3000, 3250, 3500, 3750, 4095])
            ax.set_yticklabels(["2750", "3000", "3250", "3500", "3750", "4095"],
                               fontsize=7)
            ax.grid(True, alpha=0.25)

            # Sekundarna y-os u voltima (desna strana)
            ax2 = ax.twinx()
            ax2.set_ylim(cfg.Y_MIN * cfg.VREF / cfg.ADC_MAX,
                         cfg.Y_MAX * cfg.VREF / cfg.ADC_MAX)
            v_ticks = [round(v * cfg.VREF / cfg.ADC_MAX, 2)
                       for v in [2750, 3000, 3250, 3500, 3750, 4095]]
            ax2.set_yticks(v_ticks)
            ax2.set_yticklabels([f"{v:.2f} V" for v in v_ticks],
                                fontsize=6.5, color="gray")

        # Oznaka x-osi samo na zadnjem (donjem) subplotu
        axes[-1].set_xlabel("Vrijeme [s]", fontsize=9)

    def set_status_text(self, text_artist) -> None:
        """Ubaci dijeljeni tekstualni artist statusne trake s razine figure."""
        self._status_txt = text_artist

    def update(self, rolling_buffers: np.ndarray, frame_count: int) -> list:
        """
        Ažurira sve linije valnih oblika novim podacima iz kliznog međuspremnika.

        Parametri
        ---------
        rolling_buffers : ndarray oblika (n_active, DISPLAY_SAMPLES)
        frame_count     : ukupan broj primljenih frameova

        Vraća
        -----
        Listu matplotlib artista koji su se promijenili (za blit optimizaciju).
        """
        # Postavi nove y-podatke za svaku liniju
        for i, ln in enumerate(self._lines):
            ln.set_ydata(rolling_buffers[i])

        artists = list(self._lines)

        # Ažuriraj statusnu traku ako je dostupna
        if self._status_txt is not None:
            cfg       = self.cfg
            frame_dur = cfg.SAMPLES_PER_CHAN / cfg.SAMPLE_RATE_HZ
            elapsed   = frame_count * cfg.FRAME_SKIP * frame_dur
            ms_per_fr = cfg.FRAME_SKIP * frame_dur * 1000
            self._status_txt.set_text(
                f"Frame #{frame_count}  |  "
                f"Proteklo audio: {elapsed:.1f} s  |  "
                f"~{ms_per_fr:.0f} ms/frame"
            )
            artists.append(self._status_txt)

        return artists
