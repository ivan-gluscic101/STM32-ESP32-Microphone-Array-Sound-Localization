"""
waveform_view.py — Pomični osciloskopski prikaz N kanala mikrofona.

ISPRAVKE:
  1. Y-os koristi cfg.Y_MIN i cfg.Y_MAX (sada 0–4095, puni ADC raspon)
     umjesto hardkodiranih 2750–4095.
  2. Y-tick-ovi generirani dinamički iz Y_MIN/Y_MAX.
  3. Sekundarna y-os (volti) vezana uz isti Y_MIN/Y_MAX.
"""

import numpy as np
import matplotlib.pyplot as plt


class WaveformView:
    """
    Prikazuje osciloskopski prikaz po aktivnom mikrofonu.

    Parametri
    ---------
    axes            : lista matplotlib Axes objekata
    active_channels : list[int] — indeksi kanala (0–3)
    cfg             : config modul
    """

    def __init__(self, axes, active_channels: list[int], cfg):
        self.axes            = axes
        self.active_channels = active_channels
        self.cfg             = cfg
        self._lines          = []
        self._status_txt     = None

        # Vremenski vektor za x-os
        n_samples = cfg.ROLLING_BUFFER_LEN
        t     = np.linspace(0.0, cfg.DISPLAY_SECONDS, n_samples, endpoint=False)
        zeros = np.zeros(n_samples)

        # Generiraj Y tick-ove dinamički
        y_range = cfg.Y_MAX - cfg.Y_MIN
        n_ticks = 6
        y_step = y_range / (n_ticks - 1)
        y_ticks = [int(cfg.Y_MIN + i * y_step) for i in range(n_ticks)]

        for i, ch in enumerate(active_channels):
            ax = axes[i]

            ln, = ax.plot(t, zeros, lw=0.7, color=cfg.CHANNEL_COLORS[ch])
            self._lines.append(ln)

            ax.set_ylabel(cfg.CHANNEL_NAMES[ch], fontsize=7.5, labelpad=4)
            ax.set_xlim(0, cfg.DISPLAY_SECONDS)
            ax.set_ylim(cfg.Y_MIN, cfg.Y_MAX)
            ax.set_yticks(y_ticks)
            ax.set_yticklabels([str(v) for v in y_ticks], fontsize=7)
            ax.grid(True, alpha=0.25)

            # Sekundarna y-os u voltima — vezana uz cfg.Y_MIN/Y_MAX
            ax2 = ax.twinx()
            v_min = cfg.Y_MIN * cfg.VREF / cfg.ADC_MAX
            v_max = cfg.Y_MAX * cfg.VREF / cfg.ADC_MAX
            ax2.set_ylim(v_min, v_max)
            v_ticks = [v * cfg.VREF / cfg.ADC_MAX for v in y_ticks]
            ax2.set_yticks(v_ticks)
            ax2.set_yticklabels([f"{v:.2f} V" for v in v_ticks],
                                fontsize=6.5, color="gray")

        axes[-1].set_xlabel("Vrijeme [s]", fontsize=9)

    def set_status_text(self, text_artist) -> None:
        """Ubaci dijeljeni tekstualni artist statusne trake."""
        self._status_txt = text_artist

    def update(self, rolling_buffers: np.ndarray, frame_count: int) -> list:
        """
        Ažurira linije valnih oblika.

        Parametri
        ---------
        rolling_buffers : ndarray oblika (n_active, ROLLING_BUFFER_LEN)
        frame_count     : ukupan broj primljenih frameova

        Vraća listu promijenjenih artista.
        """
        for i, ln in enumerate(self._lines):
            ln.set_ydata(rolling_buffers[i])

        artists = list(self._lines)

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