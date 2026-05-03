"""
app.py — SoundLocalizationApp: glavna klasa koja povezuje sve komponente.

Raspored prozora
----------------
    ┌────────────────────────┬──────────────┐
    │  Valni oblik 0         │              │
    ├────────────────────────┤   Polarni    │
    │  Valni oblik 1         │   prikaz     │
    ├────────────────────────┤   smjera     │
    │  ...                   │              │
    └────────────────────────┴──────────────┘

ISPRAVKE:
  1. Rolling buffer koristi ROLLING_BUFFER_LEN umjesto DISPLAY_SAMPLES.
     DISPLAY_SAMPLES bio je krivo izračunat (4000 umjesto ~7680).
  2. Uklonjen neiskorišteni self._lock (threading.Lock koji se nikad
     nije koristio — sva sinkronizacija je u UARTReader-u).
  3. blit=False ostavljen jer polarna sfera mijenja axis limits.
     Za bolji FPS na sporim računalima, razmotri blit=True uz
     postavljanje fiksnih axis limita.
"""

import sys

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.gridspec as gridspec

from uart_reader import UARTReader
from views       import WaveformView, SphereView


class SoundLocalizationApp:
    """
    Glavna aplikacijska klasa.

    Parametri
    ---------
    cfg : config modul
    """

    def __init__(self, cfg):
        self.cfg = cfg

        self._angle_count = 0

        if cfg.SHOW_WAVEFORM:
            self._active_channels = cfg.DISPLAY_CHANNEL[: cfg.ACTIVE_CHANNEL_VIEW]
            self._n_active        = len(self._active_channels)
            self._rolling         = np.zeros((self._n_active, cfg.ROLLING_BUFFER_LEN),
                                             dtype=np.float32)
            self._display_buf = None
            self._frame_count = 0

        self.reader   = None
        self.waveform = None
        self.sphere   = None
        self._ani     = None

    # ── Ulazna točka ──────────────────────────────────────────────────────────

    def run(self) -> None:
        """Otvori serijski port, izgradi figure, pokreni animaciju."""
        cfg = self.cfg

        print(f"[INFO] Spajam se na {cfg.COM_PORT} @ {cfg.BAUD_RATE} baud …")
        print(f"[INFO] Mod prikaza: {'valni oblici + kut' if cfg.SHOW_WAVEFORM else 'samo kut'}")
        if cfg.SHOW_WAVEFORM:
            print(f"[INFO] Aktivni kanali: "
                  f"{[cfg.CHANNEL_NAMES[ch] for ch in self._active_channels]}")

        try:
            self.reader = UARTReader(
                port             = cfg.COM_PORT,
                baud             = cfg.BAUD_RATE,
                num_channels     = cfg.NUM_CHANNELS,
                samples_per_chan = cfg.SAMPLES_PER_CHAN,
            ).start()
            frame_bytes = 2 + 2 + cfg.NUM_CHANNELS * cfg.SAMPLES_PER_CHAN * 2 + 2
            print(f"[OK]   Spojen. Frame veličina: {frame_bytes} B.")
        except Exception as exc:
            print(f"[GREŠKA] {cfg.COM_PORT}: {exc}")
            print("         Provjeri Device Manager → config.py.")
            sys.exit(1)

        fig = self._build_figure()

        self._ani = animation.FuncAnimation(
            fig,
            self._animate,
            interval=50,
            blit=False,
            cache_frame_data=False,
        )

        plt.show()
        self.reader.stop()
        print("[INFO] Zatvoren.")

    # ── Izgradnja figure ──────────────────────────────────────────────────────

    def _build_figure(self) -> plt.Figure:
        """Kreira matplotlib figure s GridSpec rasporedom."""
        cfg = self.cfg

        if cfg.SHOW_WAVEFORM:
            return self._build_figure_waveform()
        else:
            return self._build_figure_angle_only()

    def _build_figure_angle_only(self) -> plt.Figure:
        """Samo polarni prikaz smjera — bez valnih oblika."""
        cfg = self.cfg

        fig = plt.figure(figsize=(7, 7))
        fig.patch.set_facecolor("#0d1117")

        ax = fig.add_subplot(111)

        fig.suptitle(
            f"Sound Localization\n"
            f"{cfg.COM_PORT} @ {cfg.BAUD_RATE} baud  |  {cfg.SAMPLE_RATE_HZ} Hz",
            fontsize=11,
            color="#c9d1d9",
        )

        fig.subplots_adjust(left=0.05, right=0.95, top=0.88, bottom=0.05)

        self.waveform = None
        self.sphere = SphereView(ax, max_dots=cfg.SPHERE_MAX_DOTS,
                                 lifetime=cfg.SPHERE_DOT_LIFETIME, cfg=cfg)
        return fig

    def _build_figure_waveform(self) -> plt.Figure:
        """Valni oblici + polarni prikaz smjera."""
        cfg = self.cfg

        fig = plt.figure(figsize=(14, 2.8 * self._n_active))
        fig.patch.set_facecolor("#0d1117")

        gs = gridspec.GridSpec(
            self._n_active, 2,
            figure=fig,
            width_ratios=[3, 1.2],
            hspace=0.12,
            wspace=0.35,
        )

        wave_axes = []
        for i in range(self._n_active):
            sharex = wave_axes[0] if i > 0 else None
            ax = fig.add_subplot(gs[i, 0], sharex=sharex)
            ax.set_facecolor("#0d1117")
            for spine in ax.spines.values():
                spine.set_edgecolor("#30363d")
            ax.tick_params(colors="#8b949e")
            ax.yaxis.label.set_color("#c9d1d9")
            ax.xaxis.label.set_color("#c9d1d9")
            wave_axes.append(ax)

        sph_ax = fig.add_subplot(gs[:, 1])

        active_names = [cfg.CHANNEL_NAMES[ch] for ch in self._active_channels]
        fig.suptitle(
            f"Sound Localization — {', '.join(active_names)}\n"
            f"{cfg.COM_PORT} @ {cfg.BAUD_RATE} baud  |  {cfg.SAMPLE_RATE_HZ} Hz  |  "
            f"{cfg.SAMPLES_PER_CHAN} uzoraka/kanalu",
            fontsize=10,
            color="#c9d1d9",
        )

        status = fig.text(
            0.01, 0.005,
            "Inicijalizacija — čekam podatke …",
            fontsize=8,
            color="#8b949e",
        )

        fig.subplots_adjust(left=0.07, right=0.97, top=0.88, bottom=0.08,
                            hspace=0.12, wspace=0.38)

        self.waveform = WaveformView(wave_axes, self._active_channels, cfg)
        self.waveform.set_status_text(status)

        self.sphere = SphereView(sph_ax, max_dots=cfg.SPHERE_MAX_DOTS,
                                 lifetime=cfg.SPHERE_DOT_LIFETIME, cfg=cfg)

        return fig

    # ── Animacijski callback ──────────────────────────────────────────────────

    def _animate(self, _frame_index: int) -> list:
        """
        Poziva matplotlib svakih ~50 ms.
        Sfera i valni oblici su potpuno neovisni.
        """
        artists = []

        # ── SMJER ZVUKA ──────────────────────────────────────────────────────
        angle_data, new_angle_count = self.reader.get_latest_angle()
        if angle_data is not None and new_angle_count > self._angle_count:
            self._angle_count = new_angle_count
            phi_deg, strength = angle_data
            dot_size = self.cfg.SPHERE_DOT_SIZE * max(0.2, strength / 100.0)
            self.sphere.add_dot(
                azimuth_deg   = phi_deg,
                elevation_deg = 0.0,
                size          = dot_size,
            )

        artists += self.sphere.update()

        # ── VALNI OBLICI (samo ako je SHOW_WAVEFORM=True) ────────────────────
        if self.waveform is not None:
            raw_frame, count = self.reader.get_latest()
            if raw_frame is not None and count != self._frame_count:
                self._frame_count = count
                for i, ch in enumerate(self._active_channels):
                    self._rolling[i] = np.roll(self._rolling[i],
                                               -self.cfg.SAMPLES_PER_CHAN)
                    self._rolling[i, -self.cfg.SAMPLES_PER_CHAN:] = raw_frame[ch]
                self._display_buf = self._rolling.copy()

            if self._display_buf is not None:
                artists += self.waveform.update(self._display_buf, self._frame_count)

        return artists