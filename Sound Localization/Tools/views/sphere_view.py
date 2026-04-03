"""
sphere_view.py — 2D projekcija sfere odozgo za vizualizaciju smjera zvuka.
Implementirano: 2026-04-02

Konvencija koordinata
---------------------
    azimut    : 0–360° u smjeru kazaljke sata od "prednje strane" (vrh kruga)
    elevacija : 0° = horizont (rub kruga), 90° = točno iznad (središte)

Konvencija crtanja
------------------
    Krug predstavlja horizont (elevacija = 0°).
    Točka na azimut=A, elevacija=E crta se na polarnom radijusu r = cos(E°),
    pa zvukovi odozgo idu prema središtu, a horizontalni prema rubu.

Primjeri korištenja
-------------------
    sphere_view.add_dot(azimuth_deg=45, elevation_deg=30)
    sphere_view.add_dot(azimuth_deg=270, elevation_deg=0, color="#00ff88", label="mic2")
    sphere_view.clear_dots()
"""

from __future__ import annotations

import math
import time
from collections import deque
from dataclasses import dataclass, field
from typing import Optional

import matplotlib.pyplot as plt


@dataclass
class _Dot:
    """Interna struktura koja opisuje jednu točku smjera zvuka na sferi."""
    azimuth_deg:   float
    elevation_deg: float
    color:         str           = "#e63946"
    alpha:         float         = 0.85
    size:          float         = 80
    label:         Optional[str] = None
    born:          float         = field(default_factory=time.monotonic)


class SphereView:
    """
    2D prikaz sfere na regularnoj (ne-polarnoj) Axes osi.

    Parametri
    ---------
    ax       : matplotlib Axes na koji se crta
    max_dots : koliko historijskih točaka čuvati
    lifetime : trajanje točke u sekundama
    """

    def __init__(self, ax: plt.Axes, max_dots: int = 50, lifetime: float = 0.5, cfg=None):
        self.ax        = ax
        self._max      = max_dots
        self._lifetime = lifetime
        self._cfg      = cfg
        self._dots: deque[_Dot] = deque()

        # Lista plot artista za aktivne točke — brišemo i crtamo svaki frame
        self._dot_artists: list = []

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
        Dodaj točku smjera zvuka na sferu.

        Parametri
        ---------
        azimuth_deg   : horizontalni kut u stupnjevima, 0° = prednja strana
        elevation_deg : vertikalni kut, 0° = horizont, 90° = izravno iznad
        color         : boja točke (bilo koji matplotlib string)
        alpha         : prozirnost 0–1
        size          : promjer markera u točkama
        label         : opcionalna tekstualna oznaka
        """
        if len(self._dots) >= self._max:
            self._dots.popleft()
        self._dots.append(_Dot(azimuth_deg, elevation_deg, color, alpha, size, label))

    def clear_dots(self) -> None:
        """Ukloni sve točke s prikaza sfere."""
        self._dots.clear()

    def update(self) -> list:
        """
        Osvježi prikaz točaka. Poziva se jednom po animacijskom frameu.
        Briše stare artist objekte, uklanja zastarjele točke, crta nove.
        Vraća listu promijenjenih artista.
        """
        # Ukloni matplotlib artiste prethodnog framea s osi
        for artist in self._dot_artists:
            artist.remove()
        self._dot_artists = []

        # Ukloni točke kojima je isteklo trajanje
        now = time.monotonic()
        while self._dots and (now - self._dots[0].born) > self._lifetime:
            self._dots.popleft()

        # Nacrtaj preostale točke kao obične plot markere
        for dot in self._dots:
            x, y = self._to_xy(dot.azimuth_deg, dot.elevation_deg)
            # Pretvori size (scatter-like px²) u markersize (promjer u pt)
            ms = (dot.size ** 0.5)
            artist, = self.ax.plot(
                x, y,
                marker='o',
                markersize=ms,
                color=dot.color,
                alpha=dot.alpha,
                zorder=10,
                linestyle='none',
            )
            self._dot_artists.append(artist)

            # Opcionalna tekstualna oznaka
            if dot.label:
                txt = self.ax.text(
                    x + 0.08, y + 0.08, dot.label,
                    fontsize=7, color=dot.color, alpha=dot.alpha, zorder=11,
                )
                self._dot_artists.append(txt)

        return self._dot_artists

    # ── Pomoćne metode za koordinate ──────────────────────────────────────────

    @staticmethod
    def _to_xy(azimuth_deg: float, elevation_deg: float) -> tuple[float, float]:
        """
        Pretvori (azimut, elevacija) → (x, y) u projekciji jediničnog kruga.
            radijus = cos(elevacija) → 1 na horizontu, 0 u zenitu
            kut     = 90° - azimut  → 0° azimuta pokazuje gore (sjever)
        """
        r     = math.cos(math.radians(elevation_deg))
        theta = math.radians(90.0 - azimuth_deg)
        return r * math.cos(theta), r * math.sin(theta)

    # ── Statički sloj ─────────────────────────────────────────────────────────

    def _build_static_layer(self) -> None:
        """Crta fiksne dekoracije sfere: kružnice elevacije, žbice i oznake."""
        ax = self.ax
        ax.set_aspect("equal")
        ax.set_xlim(-1.25, 1.25)
        ax.set_ylim(-1.25, 1.25)
        ax.axis("off")
        ax.set_facecolor("#0d1117")

        # Kružnice elevacije: 0° (horizont), 30°, 60°
        for elev, ls, lw, alpha in [
            (0,  "-",  1.4, 0.70),
            (30, "--", 0.8, 0.45),
            (60, "--", 0.8, 0.45),
        ]:
            r      = math.cos(math.radians(elev))
            circle = plt.Circle(
                (0, 0), r,
                fill=False,
                linestyle=ls, linewidth=lw,
                edgecolor="#4a90d9", alpha=alpha, zorder=2,
            )
            ax.add_patch(circle)

        # Točka zenita
        ax.plot(0, 0, "o", ms=3, color="#4a90d9", alpha=0.6, zorder=3)

        # Azimutalne žbice svakih 45°
        for az in range(0, 360, 45):
            x, y = SphereView._to_xy(az, 0)
            ax.plot([0, x], [0, y], color="#4a90d9", lw=0.6, alpha=0.3, zorder=2)

        # Kardinalne oznake azimuta
        for az, txt in [(0, "0°"), (90, "90°"), (180, "180°"), (270, "270°")]:
            x, y = SphereView._to_xy(az, 0)
            ax.text(
                x * 1.13, y * 1.13, txt,
                ha="center", va="center",
                fontsize=7.5, color="#8ab4f8", zorder=4,
            )

        # Oznake elevacijskih kružnica
        for elev, label in [(30, "30°"), (60, "60°")]:
            r = math.cos(math.radians(elev))
            ax.text(
                r + 0.03, 0.03, label,
                ha="left", va="bottom",
                fontsize=6.5, color="#4a90d9", alpha=0.7, zorder=4,
            )

        # Naslov s fizikalnim parametrima ako je cfg dostupan
        if self._cfg is not None:
            title = (
                f"Smjer zvuka\n"
                f"d={self._cfg.MIC_DIST_M*100:.0f} cm  |  "
                f"res≈{self._cfg.ANGULAR_RES_DEG:.1f}°/uzorku  |  "
                f"TDOA_max≈{self._cfg.TDOA_MAX_SAMPLES:.1f} uzoraka"
            )
        else:
            title = "Smjer zvuka"
        ax.set_title(title, fontsize=7.5, color="#c9d1d9", pad=6)
