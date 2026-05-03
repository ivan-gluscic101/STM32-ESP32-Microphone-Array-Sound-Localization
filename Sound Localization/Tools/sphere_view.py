"""
sphere_view.py — Polarni prikaz smjera zvuka za 1D (2-mikrofonski) sustav.

ISPRAVKE:
  1. Sfera zamijenjena polarnim dijagramom jer sustav ima samo 2 mikrofona
     i može detektirati samo 1D kut φ ∈ [-90°, +90°].
     3D sfera s azimutom 0°-360° i elevacijom je bila vizualno lijepa
     ali potpuno nereprezentativna — sve točke su padale na rub kruga
     (elevacija=0°) i pozicije nisu odgovarale stvarnom kutu.

  2. Kut φ se sada koristi DIREKTNO kao polarni kut:
     φ = 0°   → zvuk dolazi okomito na os mikrofona (gore na prikazu)
     φ = +90° → zvuk dolazi s mic2 strane (desno)
     φ = -90° → zvuk dolazi s mic1 strane (lijevo)

  3. Oznake prilagođene stvarnom rasponu sustava (±90°).

  4. Koristi se ax.scatter() s nizovima umjesto pojedinačnih ax.plot()
     poziva — znatno brže za 50 točaka.

  5. Lifetime povećan na 2s (bio 0.5s) za bolje praćenje.

Konvencija koordinata
---------------------
    φ ∈ [-90°, +90°] od okomice na os mikrofona
    φ = 0°   → prednja strana (gore)
    φ > 0°   → mic2 strana (desno)
    φ < 0°   → mic1 strana (lijevo)
"""

from __future__ import annotations

import math
import time
from collections import deque
from dataclasses import dataclass, field
from typing import Optional

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches


@dataclass
class _Dot:
    """Jedna točka smjera zvuka."""
    phi_deg:   float           # kut iz firmware-a [-90, +90]
    strength:  int     = 50    # jakost signala 0-100
    color:     str     = "#e63946"
    alpha:     float   = 0.85
    size:      float   = 80
    born:      float   = field(default_factory=time.monotonic)


class SphereView:
    """
    Polarni prikaz smjera zvuka na regularnoj Axes osi.

    Prikazuje polukrug (-90° do +90°) koji odgovara detektabilnom
    rasponu 2-mikrofonskog sustava. Zvučni eventi se crtaju kao
    točke na odgovarajućem kutu, s veličinom proporcionalnom jakosti
    i prozirnošću koja blijedi s vremenom.

    Parametri
    ---------
    ax       : matplotlib Axes na koji se crta
    max_dots : koliko historijskih točaka čuvati
    lifetime : trajanje točke u sekundama
    cfg      : config modul (opcionalno, za prikaz fizikalnih parametara)
    """

    def __init__(self, ax: plt.Axes, max_dots: int = 50, lifetime: float = 2.0, cfg=None):
        self.ax        = ax
        self._max      = max_dots
        self._lifetime = lifetime
        self._cfg      = cfg
        self._dots: deque[_Dot] = deque()

        # Scatter artist za sve točke — ažuriramo podatke umjesto re-kreiranja
        self._scatter = None

        # Crtanje statičnog pozadinskog sloja
        self._build_static_layer()

    # ── Javno sučelje ─────────────────────────────────────────────────────────

    def add_dot(
        self,
        azimuth_deg: float,
        elevation_deg: float = 0.0,
        color: str = "#e63946",
        alpha: float = 0.85,
        size: float = 80,
        label: Optional[str] = None,
    ) -> None:
        """
        Dodaj točku smjera zvuka.

        azimuth_deg se koristi kao φ kut iz firmware-a [-90°, +90°].
        elevation_deg se ignorira (sustav je 1D).
        """
        if len(self._dots) >= self._max:
            self._dots.popleft()

        strength = int(max(0, min(100, size * 100 / 80)))  # aproksimacija iz size-a
        self._dots.append(_Dot(
            phi_deg=azimuth_deg,
            strength=strength,
            color=color,
            alpha=alpha,
            size=size,
        ))

    def clear_dots(self) -> None:
        """Ukloni sve točke."""
        self._dots.clear()

    def update(self) -> list:
        """
        Osvježi prikaz. Poziva se jednom po animacijskom frameu.
        Uklanja zastarjele točke, ažurira scatter plot.
        Vraća listu promijenjenih artista.
        """
        # Ukloni istekle točke
        now = time.monotonic()
        while self._dots and (now - self._dots[0].born) > self._lifetime:
            self._dots.popleft()

        if not self._dots:
            if self._scatter is not None:
                self._scatter.set_offsets(np.empty((0, 2)))
                self._scatter.set_sizes(np.array([]))
            return [self._scatter] if self._scatter else []

        # Pripremi nizove za scatter
        xs = np.empty(len(self._dots))
        ys = np.empty(len(self._dots))
        sizes = np.empty(len(self._dots))
        alphas = np.empty(len(self._dots))
        colors = []

        for i, dot in enumerate(self._dots):
            x, y = self._phi_to_xy(dot.phi_deg)
            xs[i] = x
            ys[i] = y
            sizes[i] = dot.size

            # Alpha blijedi s vremenom
            age = now - dot.born
            fade = max(0.1, 1.0 - (age / self._lifetime))
            alphas[i] = dot.alpha * fade
            colors.append(dot.color)

        if self._scatter is None:
            self._scatter = self.ax.scatter(
                xs, ys, s=sizes, c=colors, alpha=0.8,
                zorder=10, edgecolors='white', linewidths=0.5
            )
        else:
            self._scatter.set_offsets(np.column_stack([xs, ys]))
            self._scatter.set_sizes(sizes)
            self._scatter.set_facecolors([
                (*plt.matplotlib.colors.to_rgb(c), a)
                for c, a in zip(colors, alphas)
            ])

        return [self._scatter]

    # ── Koordinatna konverzija ────────────────────────────────────────────────

    @staticmethod
    def _phi_to_xy(phi_deg: float) -> tuple[float, float]:
        """
        Pretvori φ [-90°, +90°] u (x, y) na polukrugu radijusa 1.

        φ = 0°   → (0, 1)    gore (okomito na os mikrofona)
        φ = +90° → (1, 0)    desno (mic2 strana)
        φ = -90° → (-1, 0)   lijevo (mic1 strana)

        Zapravo mapiramo φ na polukrug:
            x = sin(φ)
            y = cos(φ)
        """
        rad = math.radians(phi_deg)
        return math.sin(rad), math.cos(rad)

    # ── Statički sloj ─────────────────────────────────────────────────────────

    def _build_static_layer(self) -> None:
        """Crta fiksne dekoracije: polukrug, žbice i oznake."""
        ax = self.ax
        ax.set_aspect("equal")
        ax.set_xlim(-1.35, 1.35)
        ax.set_ylim(-0.35, 1.35)
        ax.axis("off")
        ax.set_facecolor("#0d1117")

        # Gornji polukrug — detektabilno područje
        theta = np.linspace(-90, 90, 200)
        theta_rad = np.radians(theta)
        arc_x = np.sin(theta_rad)
        arc_y = np.cos(theta_rad)
        ax.plot(arc_x, arc_y, color="#4a90d9", lw=1.5, alpha=0.7, zorder=2)

        # Horizontalna linija (baza)
        ax.plot([-1, 1], [0, 0], color="#4a90d9", lw=1.0, alpha=0.5, zorder=2)

        # Središte (okomit smjer)
        ax.plot(0, 0, "o", ms=4, color="#4a90d9", alpha=0.6, zorder=3)

        # Žbice na svakih 15° od -90° do +90°
        for phi in range(-90, 91, 15):
            x, y = self._phi_to_xy(phi)
            alpha = 0.5 if phi % 30 == 0 else 0.2
            lw = 0.8 if phi % 30 == 0 else 0.4
            ax.plot([0, x], [0, y], color="#4a90d9", lw=lw, alpha=alpha, zorder=2)

        # Oznake kuteva
        for phi, txt in [(-90, "-90°"), (-60, "-60°"), (-30, "-30°"),
                         (0, "0°"), (30, "+30°"), (60, "+60°"), (90, "+90°")]:
            x, y = self._phi_to_xy(phi)
            # Pomak oznake van polukruga
            ox, oy = x * 1.15, y * 1.15
            ha = "center"
            if phi < -15:
                ha = "right"
            elif phi > 15:
                ha = "left"
            ax.text(ox, oy, txt,
                    ha=ha, va="center",
                    fontsize=7, color="#8ab4f8", zorder=4)

        # Oznake mikrofona
        ax.text(-1.15, -0.12, "← mic1", ha="center", va="top",
                fontsize=7, color="#1f77b4", alpha=0.8)
        ax.text(1.15, -0.12, "mic2 →", ha="center", va="top",
                fontsize=7, color="#ff7f0e", alpha=0.8)

        # Naslov
        if self._cfg is not None:
            title = (
                f"Smjer zvuka (1D)\n"
                f"d={self._cfg.MIC_DIST_M*100:.0f} cm  |  "
                f"res≈{self._cfg.ANGULAR_RES_DEG:.1f}°/uzorku  |  "
                f"TDOA_max≈{self._cfg.TDOA_MAX_SAMPLES:.1f} uzoraka"
            )
        else:
            title = "Smjer zvuka (1D)"
        ax.set_title(title, fontsize=7.5, color="#c9d1d9", pad=6)