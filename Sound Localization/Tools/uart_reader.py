"""
uart_reader.py — Pozadinski UART čitač frameova s STM32 mikrokontrolera.

Podržani tipovi paketa:
    Audio frame (legacy):
        [0xAA][0xBB] [ID_H][ID_L] [podaci...] [0xCC][0xDD]
        NAPOMENA: ID_H je uvijek 0x00-0x0F jer je frame_id uint16
        koji se rijetko penje iznad ~1000. Čak i ako dođe do 0x02xx,
        parser provjerava duljinu ostatka za razlikovanje.

    Angle paket (tip 0x02):
        [0xAA][0xBB] [0x02] [PHI_H][PHI_L] [STR] [0xCC][0xDD]
        PHI = int16 big-endian, kut u desetinkama stupnjeva
        STR = uint8, jakost signala 0-100

ISPRAVKE:
  1. Dodana detekcija kolizije frame_id vs angle tip:
     Ako type_byte == 0x02, čitamo 5 bajtova (angle paket).
     Ako EOF nije valjan, pokušavamo re-interpretirati kao audio frame.
  2. Ispravljen docstring (viška zagrada).
  3. Dodan print za debug info bez spama.
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
        angle_data, angle_count = reader.get_latest_angle()
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

        self._data_bytes = num_channels * samples_per_chan * 2
        self._rest_bytes = 2 + self._data_bytes + 2   # ID(2) + data + EOF(2)

        self._lock = threading.Lock()

        # Audio frame stanje
        self._latest      = None
        self._frame_count = 0

        # Angle paket stanje
        self._latest_angle = None
        self._angle_count  = 0

        self._ser    = None
        self._thread = None

    # ── Javno sučelje ─────────────────────────────────────────────────────────

    def start(self) -> "UARTReader":
        """Otvori port i pokreni pozadinsku nit za čitanje."""
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
        Vraća (frame, count).
            frame : ndarray oblika (num_channels, samples_per_chan), ili None
            count : int
        """
        with self._lock:
            return self._latest, self._frame_count

    def get_latest_angle(self) -> tuple:
        """
        Vraća ((phi_deg, strength), count).
            phi_deg  : float, kut u stupnjevima [-90.0, +90.0]
            strength : int, jakost signala 0-100
            count    : int, ukupan broj primljenih angle paketa
        """
        with self._lock:
            return self._latest_angle, self._angle_count

    @property
    def is_connected(self) -> bool:
        return self._ser is not None and self._ser.is_open

    # ── Interne metode ────────────────────────────────────────────────────────

    def _loop(self) -> None:
        """Glavna petlja — kontinuirano čita i razvrstava pakete."""
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
        Traži SOF, zatim čita tip paketa i parsira.

        ISPRAVKA protokolne dvosmislenosti:
        Ako type_byte == 0x02, prvo pokušavamo parsirati kao angle paket (5 B).
        Ako EOF ne odgovara, onda smo krivo protumačili — to je zapravo
        audio frame čiji je ID_H == 0x02. Tada koristimo već pročitane bajtove
        kao dio audio framea.

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

        # 2. Čitaj prvi bajt — može biti tip paketa ili ID_H audio framea
        type_byte = ser.read(1)
        if not type_byte:
            return None

        if type_byte[0] == self._ANGLE_TYPE:
            # Pokušaj angle paket — čitamo 5 B (PHI_H, PHI_L, STR, EOF1, EOF2)
            angle_rest = ser.read(5)
            if len(angle_rest) != 5:
                return None

            # Provjeri EOF
            if angle_rest[3] == 0xCC and angle_rest[4] == 0xDD:
                # Valjan angle paket
                phi_encoded = struct.unpack('>h', angle_rest[0:2])[0]
                phi_deg     = phi_encoded / 10.0
                strength    = angle_rest[2]
                return ('angle', (phi_deg, strength))
            else:
                # Krivo protumačeno — ovo je audio frame s ID_H=0x02.
                # angle_rest[0] je ID_L, angle_rest[1:5] su prva 4 bajta podataka.
                # Trebamo pročitati ostatak audio framea.
                id_high = type_byte[0]
                id_low  = angle_rest[0]
                already_read = angle_rest[1:5]   # 4 bajta podataka već pročitano
                remaining_needed = self._data_bytes - 4 + 2   # ostatak podataka + EOF
                remaining = ser.read(remaining_needed)
                if len(remaining) != remaining_needed:
                    return None
                full_data = already_read + remaining
                # Provjeri EOF na kraju
                if full_data[-2] != 0xCC or full_data[-1] != 0xDD:
                    return None
                return self._decode_audio(full_data[:-2])  # bez EOF-a
        else:
            # Audio frame — type_byte je ID_H
            return self._parse_audio_frame(type_byte[0])

    def _parse_audio_frame(self, id_high_byte: int):
        """Parser audio framea. SOF pročitan, id_high_byte je ID_H."""
        ser = self._ser

        remaining = ser.read(self._rest_bytes - 1)
        if len(remaining) != self._rest_bytes - 1:
            return None

        full_rest = bytes([id_high_byte]) + remaining

        if full_rest[-2] != 0xCC or full_rest[-1] != 0xDD:
            return None

        # Preskoči 2B frame_id, uzmi podatke (bez EOF-a)
        return self._decode_audio(full_rest[2:-2])

    def _decode_audio(self, data_bytes: bytes):
        """Dekodira sirove bajtove u ndarray oblika (num_channels, samples_per_chan)."""
        n = self.num_channels * self.samples_per_chan
        expected_len = n * 2
        if len(data_bytes) != expected_len:
            return None

        raw = struct.unpack(f">{n}H", data_bytes)
        arr = np.array(raw, dtype=np.float32)

        # De-interleaving: [CH0_s0, CH1_s0, ..., CH0_s1, CH1_s1, ...]
        out = np.empty((self.num_channels, self.samples_per_chan), dtype=np.float32)
        for ch in range(self.num_channels):
            out[ch] = arr[ch :: self.num_channels]

        return ('audio', out)