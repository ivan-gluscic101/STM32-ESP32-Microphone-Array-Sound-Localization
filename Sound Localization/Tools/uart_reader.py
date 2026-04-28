"""
uart_reader.py — Pozadinski UART čitač frameova s STM32 mikrokontrolera.
Implementirano: 2026-04-02

Podržani tipovi paketa:
    Audio frame (tip 0x01 / legacy):
        [0xAA][0xBB] [ID_H][ID_L] [podaci...] [0xCC][0xDD]

    Angle paket (tip 0x02, dodano 2026-04-03):
        [0xAA][0xBB] [0x02] [PHI_H][PHI_L] [STR] [0xCC][0xDD]
        PHI = int16 big-endian, kut u desetinkama stupnjeva (254 = +25.4°)
        STR = uint8, jakost signala 0-100
"""

import struct
import threading

import numpy as np
import serial


class UARTReader:
    """
    Otvara serijski port, čita pakete u daemon niti i izlaže:
        get_latest()       → najnoviji audio frame (ndarray) + broj primljenih
        get_latest_angle() → najnoviji (phi_deg, strength) + broj primljenih

    Primjer korištenja:
        reader = UARTReader("COM8", 921600, num_channels=4, samples_per_chan=512)
        reader.start()
        frame, count = reader.get_latest()
        phi, strength), angle_count = reader.get_latest_angle()
        reader.stop()
    """

    _SOF = bytes([0xAA, 0xBB])
    _EOF = bytes([0xCC, 0xDD])
    _ANGLE_TYPE = 0x02

    def __init__(
        self,
        port: str,
        baud: int,
        num_channels: int,
        samples_per_chan: int,
        timeout: float = 2.0,
    ):
        self.port             = port
        self.baud             = baud
        self.num_channels     = num_channels
        self.samples_per_chan = samples_per_chan
        self.timeout          = timeout

        # Unaprijed izračunate veličine audio framea
        self._data_bytes = num_channels * samples_per_chan * 2   # uint16 → 2 bajta
        self._rest_bytes = 2 + self._data_bytes + 2              # ID(2) + podaci + EOF(2)

        self._lock        = threading.Lock()

        # Audio frame stanje
        self._latest      = None   # ndarray (num_channels, samples_per_chan)
        self._frame_count = 0

        # Angle paket stanje
        self._latest_angle = None  # (phi_deg: float, strength: int)
        self._angle_count  = 0

        self._ser    = None
        self._thread = None

    # ── Javno sučelje ─────────────────────────────────────────────────────────

    def start(self) -> "UARTReader":
        """Otvori port i pokreni pozadinsku nit za čitanje. Vraća self (fluent API)."""
        self._ser    = serial.Serial(self.port, self.baud, timeout=self.timeout)
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()
        return self

    def stop(self) -> None:
        """Zatvori serijski port."""
        if self._ser and self._ser.is_open:
            self._ser.close()

    def get_latest(self) -> tuple:
        """
        Vraća (frame, count) — najnoviji audio frame i broj primljenih.
            frame : ndarray oblika (num_channels, samples_per_chan), ili None
            count : int
        """
        with self._lock:
            return self._latest, self._frame_count

    def get_latest_angle(self) -> tuple:
        """
        Vraća ((phi_deg, strength), count) — najnoviji angle paket.
            phi_deg  : float, kut u stupnjevima [-90.0, +90.0]
            strength : int,   jakost signala 0-100
            count    : int,   ukupan broj primljenih angle paketa
        Vraća (None, 0) dok nije primljen nijedan angle paket.
        """
        with self._lock:
            return self._latest_angle, self._angle_count

    @property
    def is_connected(self) -> bool:
        return self._ser is not None and self._ser.is_open

    # ── Interne metode ────────────────────────────────────────────────────────

    def _loop(self) -> None:
        """Glavna petlja pozadinske niti — kontinuirano čita i razvrstava pakete."""
        while True:
            try:
                result = self._read_packet()
            except serial.SerialException:
                break
            if result is None:
                continue

            ptype, data = result

            if ptype == 'audio':
                with self._lock:
                    self._latest       = data
                    self._frame_count += 1

            elif ptype == 'angle':
                with self._lock:
                    self._latest_angle = data
                    self._angle_count += 1

    def _read_packet(self):
        """
        Traži SOF byte po byte, zatim čita tip paketa i granu na odgovarajući parser.

        Vraća ('audio', ndarray) ili ('angle', (phi_deg, strength)) ili None.
        """
        ser = self._ser

        # 1. Traži SOF: 0xAA 0xBB
        while True:
            b = ser.read(1)
            if not b:
                return None
            if b[0] == 0xAA:
                nb = ser.read(1)
                if nb and nb[0] == 0xBB:
                    break

        # 2. Čitaj prvi bajt koji određuje tip paketa
        type_byte = ser.read(1)
        if not type_byte:
            return None

        if type_byte[0] == self._ANGLE_TYPE: # U main.c smo definirali da se traži 0x02 byte za angle
            return self._parse_angle_packet()
        else:
            # Nije angle paket — type_byte je ID_H audio framea
            return self._parse_audio_frame(type_byte[0])

    def _parse_angle_packet(self):
        """
        Parser angle paketa (tip 0x02).
        SOF i tip bajt već su pročitani. Čita: PHI_H, PHI_L, STR, EOF1, EOF2.
        """
        ser  = self._ser
        rest = ser.read(5)   # PHI_H(1) + PHI_L(1) + STR(1) + EOF(2)
        if len(rest) != 5:
            return None
        if rest[3] != 0xCC or rest[4] != 0xDD:
            return None      # neispravan EOF — odbaci

        # PHI je int16 big-endian, enkodiran kao phi_deg * 10
        phi_encoded = struct.unpack('>h', rest[0:2])[0]   # signed int16
        phi_deg     = phi_encoded / 10.0
        #print(f"[ANGLE] phi={phi_deg:.1f}° strength={strength}")
        strength    = rest[2]
        print(phi_deg)
        return ('angle', (phi_deg, strength))

    def _parse_audio_frame(self, id_high_byte: int):
        """
        Parser audio framea (legacy tip).
        SOF je već pročitan, id_high_byte je ID_H bajt framea.
        Čita preostalih (rest_bytes - 1) bajtova.
        """
        ser = self._ser

        # Trebamo još: ID_L(1) + podaci + EOF(2) = _rest_bytes - 1 bajtova
        remaining = ser.read(self._rest_bytes - 1)
        if len(remaining) != self._rest_bytes - 1:
            return None

        # Rekonstruiraj puni rest = [ID_H, ID_L, podaci..., EOF1, EOF2]
        full_rest = bytes([id_high_byte]) + remaining

        # Provjeri EOF
        if full_rest[-2] != 0xCC or full_rest[-1] != 0xDD:
            return None      # oštećen frame

        # Raspakiraj uint16 vrijednosti (big-endian), preskoči 2B frame_id
        n   = self.num_channels * self.samples_per_chan
        raw = struct.unpack_from(f">{n}H", full_rest, offset=2)
        #print(raw)
        arr = np.array(raw, dtype=np.float32)

        # De-interleaving: [CH0_s0, CH1_s0, ..., CH0_s1, CH1_s1, ...]
        out = np.empty((self.num_channels, self.samples_per_chan), dtype=np.float32)
        for ch in range(self.num_channels):
            out[ch] = arr[ch :: self.num_channels]

        return ('audio', out)
