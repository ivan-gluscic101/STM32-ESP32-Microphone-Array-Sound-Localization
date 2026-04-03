"""
app.py — SoundLocalizationApp: glavna klasa koja povezuje sve komponente.
Implementirano: 2026-04-02

Raspored prozora
----------------
    ┌────────────────────────┬──────────────┐
    │  Valni oblik 0         │              │
    ├────────────────────────┤    Sfera     │
    │  Valni oblik 1         │              │
    ├────────────────────────┤              │
    │  ...                   │              │
    └────────────────────────┴──────────────┘

Kako nadograditi
----------------
    • Novi prikaz: ne treba nasljeđivati ništa — kreiraj klasu, proslijedi joj Axes,
      pozovi view.update() unutar _animate() i dodaj artiste u povratnu listu.
    • Promjena rasporeda: uredi _build_figure().
    • Algoritam lokalizacije zvuka: izračunaj (azimut, elevacija) iz frame podataka
      i pozovi self.sphere.add_dot(azimut, elevacija) unutar _animate().
"""

import sys
import threading

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
    cfg : config modul (uvozi se u main.py i ovdje se prosljeđuje)
    """

    def __init__(self, cfg):
        self.cfg = cfg

        # Lista aktivnih kanala i njihov broj
        self._active_channels = cfg.DISPLAY_CHANNEL[: cfg.ACTIVE_CHANNEL_VIEW]
        self._n_active        = len(self._active_channels)

        # Klizni međuspremnici — jedan po aktivnom kanalu
        self._lock        = threading.Lock()
        self._rolling     = np.zeros((self._n_active, cfg.DISPLAY_SAMPLES),
                                     dtype=np.float32)
        self._display_buf  = None  # snimka za GUI nit
        self._frame_count  = 0
        self._angle_count  = 0     # prati koliko angle paketa smo već prikazali

        # Podkomponente — inicijaliziraju se u run()
        self.reader   = None
        self.waveform = None
        self.sphere   = None
        self._ani     = None

    # ── Ulazna točka ──────────────────────────────────────────────────────────

    def run(self) -> None:
        """Otvori serijski port, izgradi figure, pokreni animacijsku petlju."""
        cfg = self.cfg

        print(f"[INFO] Spajam se na {cfg.COM_PORT} @ {cfg.BAUD_RATE} baud …")
        print(f"[INFO] Aktivni kanali: "
              f"{[cfg.CHANNEL_NAMES[ch] for ch in self._active_channels]}")

        try:
            self.reader = UARTReader(
                port             = cfg.COM_PORT,
                baud             = cfg.BAUD_RATE,
                num_channels     = cfg.NUM_CHANNELS,
                samples_per_chan = cfg.SAMPLES_PER_CHAN,
            ).start()
            # Ukupna veličina framea: SOF(2) + ID(2) + podaci + EOF(2)
            frame_bytes = 2 + 2 + cfg.NUM_CHANNELS * cfg.SAMPLES_PER_CHAN * 2 + 2
            print(f"[OK]   Spojen. Očekujem frame veličine {frame_bytes} bajta.")
        except Exception as exc:
            print(f"[GREŠKA] Nije moguće otvoriti {cfg.COM_PORT}: {exc}")
            print("         Provjeri Device Manager i postavi COM_PORT u config.py.")
            sys.exit(1)

        fig = self._build_figure()

        # interval=50 ms → ~20 pokušaja FPS-a; stvarni FPS ograničen je FRAME_SKIP-om
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
        """Kreira matplotlib figure s GridSpec rasporedom: valni oblici + sfera."""
        cfg = self.cfg

        fig = plt.figure(figsize=(14, 2.8 * self._n_active))
        fig.patch.set_facecolor("#0d1117")

        # GridSpec: lijeva kolona (valni oblici) + desna kolona (sfera)
        gs = gridspec.GridSpec(
            self._n_active, 2,
            figure=fig,
            width_ratios=[3, 1.2],
            hspace=0.12,
            wspace=0.35,
        )

        # Axes za valne oblike (lijeva kolona, dijeljena x-os)
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

        # Axes za sferu (desna kolona, proteže se kroz sve redove)
        sph_ax = fig.add_subplot(gs[:, 1])

        # Naslov figure
        active_names = [cfg.CHANNEL_NAMES[ch] for ch in self._active_channels]
        fig.suptitle(
            f"Sound Localization — {', '.join(active_names)}\n"
            f"{cfg.COM_PORT} @ {cfg.BAUD_RATE} baud  |  {cfg.SAMPLE_RATE_HZ} Hz  |  "
            f"{cfg.SAMPLES_PER_CHAN} uzoraka/kanalu  |  "
            f"{cfg.FRAME_SKIP * cfg.FRAME_DURATION_S * 1000:.0f} ms/frame",
            fontsize=10,
            color="#c9d1d9",
        )

        # Statusna traka na dnu figure
        status = fig.text(
            0.01, 0.005,
            "Inicijalizacija — čekam podatke …",
            fontsize=8,
            color="#8b949e",
        )

        # tight_layout nije kompatibilan s set_aspect("equal") na sferi
        fig.subplots_adjust(left=0.07, right=0.97, top=0.88, bottom=0.08,
                            hspace=0.12, wspace=0.38)

        # Instanciranje prikaza
        self.waveform = WaveformView(wave_axes, self._active_channels, cfg)
        self.waveform.set_status_text(status)

        self.sphere = SphereView(sph_ax, max_dots=cfg.SPHERE_MAX_DOTS,
                                 lifetime=cfg.SPHERE_DOT_LIFETIME, cfg=cfg)

        return fig

    # ── Animacijski callback ──────────────────────────────────────────────────

    def _animate(self, _frame_index: int) -> list:
        """
        Poziva matplotlib svakih ~50 ms.
        Sfera i valni oblici su potpuno neovisni — sfera se uvijek crta.
        """
        artists = []

        # ── SMJER ZVUKA — neovisno o audio frameu ────────────────────────────
        # Implementirano: 2026-04-03
        angle_data, new_angle_count = self.reader.get_latest_angle()
        if angle_data is not None and new_angle_count > self._angle_count:
            self._angle_count = new_angle_count
            phi_deg, strength = angle_data
            # Veličina točke proporcionalna jakosti signala (min 20%)
            dot_size = self.cfg.SPHERE_DOT_SIZE * max(0.2, strength / 100.0)
            self.sphere.add_dot(
                azimuth_deg   = phi_deg,
                elevation_deg = 0.0,
                size          = dot_size,
            )

        # Sfera se uvijek osvježava (uklanja zastarjele točke, crta nove)
        artists += self.sphere.update()

        # ── VALNI OBLICI — samo kad ima audio podataka ────────────────────────
        raw_frame, count = self.reader.get_latest()
        if raw_frame is not None and count != self._frame_count:
            self._frame_count = count
            for i, ch in enumerate(self._active_channels):
                self._rolling[i] = np.roll(self._rolling[i], -self.cfg.SAMPLES_PER_CHAN)
                self._rolling[i, -self.cfg.SAMPLES_PER_CHAN:] = raw_frame[ch]
            self._display_buf = self._rolling.copy()

        if self._display_buf is not None:
            artists += self.waveform.update(self._display_buf, self._frame_count)

        return artists
