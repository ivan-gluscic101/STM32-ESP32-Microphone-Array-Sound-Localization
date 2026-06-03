# TDOA STM32 + MATLAB kontinuirani stream - FT232RL varijanta

## Sto je promijenjeno

1. **ADC vise nije single-shot.** `adc_capture.c` koristi circular DMA s half-transfer i transfer-complete interruptima, pa ADC stalno uzorkuje bez rupa.
2. **`main.c` vise ne blokira i ne restartira ADC nakon svakog framea.** Sada samo salje sljedeci gotov frame kada je UART DMA slobodan.
3. **UART je postavljen na 3 Mbaud, sto je realan maksimum za FT232RL.** Originalnih 64 kHz za 4 kanala na 16 bita trazi oko 5.12 Mbit/s na UART 8N1, pa ne moze raditi kroz FT232RL. Zato je `ADC_SAMPLE_RATE_HZ` spusten na 32 kHz.
4. **MATLAB skripta cita kontinuirani tok.** Ne cita samo 4 framea i ne zatvara port, nego stalno trazi sync header, puni klizni buffer i prikazuje azimut/elevaciju.
5. **GCC-PHAT znak je ispravljen.** Pozitivan TDOA sada znaci da MICi kasni za MIC1. To odgovara jednadzbi `MIC_POS(1,:) - MIC_POS(i,:)` u DOA procjeni.
6. **Dodana je sub-sample interpolacija GCC vrha** za bolju TDOA rezoluciju od jednog ADC uzorka.

## Vazno za FT232RL

- Koristi `UART_BAUD_RATE = 3000000` u `app_config.h` i isti `BAUD_RATE = 3000000` u `DelayProcessing.m`.
- `ADC_SAMPLE_RATE_HZ = 32000` daje oko 2.56 Mbit/s payload linijske brzine na UART 8N1, plus vrlo mali sync/header overhead. To ostavlja prakticnu marginu ispod 3 Mbaud.
- `FS` u MATLAB-u mora odgovarati stvarnom TIM3 sample rateu. Uz `TIM3_AUTORELOAD = 5311`, stvarni `FS` je `170e6/5312 = 32003.012 Hz`.
- U FTDI driveru po mogucnosti spusti **Latency Timer** na 1 ms. To ne povecava maksimalni protok, ali smanjuje burstove i kasnjenje.
- Koristi kratke zice i zajednicki GND. Na 3 Mbaud dugi jumperi ili losa masa lako uzrokuju gubitak synca.
- Ako MATLAB i dalje prijavljuje `lost sync`, probaj sigurniju konfiguraciju: `UART_BAUD_RATE = 2000000`, `BAUD_RATE = 2000000`, `ADC_SAMPLE_RATE_HZ = 24000`, a u MATLAB-u postavi `FS = 170e6 / (floor(170e6/24000))` odnosno uskladi s izracunatim `TIM3_AUTORELOAD`.

## Za tocnu lokalizaciju

- Provjeri fizicki redoslijed mikrofona i ADC rankova:
  - MIC1 = PB14 / ADC1_IN5
  - MIC2 = PC0  / ADC1_IN6
  - MIC3 = PC1  / ADC1_IN7
  - MIC4 = PC2  / ADC1_IN8
- Ako rezultat ima stalni kutni pomak, izmjeri poznati smjer i upisi korekciju u `TDOA_OFFSET_US` u MATLAB skripti.

## Datoteke za zamjenu

Zamijeni u STM32 projektu:

- `Core/Src/main.c`
- `Core/Src/adc_capture.c`
- `Core/Src/uart_stream.c`

Provjeri/dodaj u `Core/Inc`:

- `app_config.h`
- `adc_capture.h`
- `uart_stream.h`

MATLAB:

- pokreni `DelayProcessing.m`
