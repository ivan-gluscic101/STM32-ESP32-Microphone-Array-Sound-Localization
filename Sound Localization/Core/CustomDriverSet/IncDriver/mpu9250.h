#ifndef MPU9250_H
#define MPU9250_H

/*
 * mpu9250.h — Driver za InvenSense MPU-9250 / MPU-9255 / MPU-6500 (GY-521 ploča).
 *
 * Detektira čip preko WHO_AM_I i automatski omogućuje magnetometar (AK8963)
 * ako je prisutan (9250/9255). Na 6500 (bez kompasa) yaw radi samo iz žira
 * (drift se akumulira) — MPU_HasMag() vraća 0 u tom slučaju.
 *
 * Senzorski okvir je poravnat s mikrofonskim okvirom (audio_common.h):
 *   IMU +X = +X mikrofonskog niza (bisektrisa M2/M3, azimut 0°)
 *   IMU +Y = +Y (M2 strana, lijevo)
 *   IMU +Z = +Z (gore, M4)
 * Modul montiraj na sredinu ravnine M1-M2-M3, X-os okrenuta "naprijed".
 *
 * Fuzija: Mahony complementary filter (akcel+žiro+mag → quaternion → Eulerovi
 * kutovi). Pitch/roll iz gravitacije (bez drifta), yaw iz kompasa kad postoji.
 *
 * Spoj: I2C1 (PB8/PB9), adresa 0x68 (AD0=GND) ili 0x69 (AD0=VCC). Driver proba
 * obje. Vidi i2c_driver.h.
 */

#include <stdint.h>

/* WHO_AM_I vrijednosti za podržane čipove */
#define MPU_WHOAMI_6500   0x70
#define MPU_WHOAMI_9250   0x71
#define MPU_WHOAMI_9255   0x73

typedef enum {
    MPU_CHIP_UNKNOWN = 0,
    MPU_CHIP_6500,           /* 6-osni: akcel + žiro, bez kompasa */
    MPU_CHIP_9250,           /* 9-osni: + AK8963 magnetometar */
    MPU_CHIP_9255            /* 9-osni: + AK8963 magnetometar */
} mpu_chip_t;

/* Orijentacija ploče u stupnjevima (Tait-Bryan, ZYX konvencija). */
typedef struct {
    float roll_deg;   /* rotacija oko X (-180..+180) */
    float pitch_deg;  /* rotacija oko Y (-90..+90)   */
    float yaw_deg;    /* rotacija oko Z (0..360), iz kompasa ako postoji */
} mpu_orientation_t;

/*
 * Inicijalizira MPU: probudi iz sleepa, postavi raspon ±2000°/s žiro i ±4g
 * akcel, DLPF, i AK8963 magnetometar (ako čip ima kompas).
 * Mora se pozvati nakon Custom_I2C1_Init().
 * @return detektirani tip čipa; MPU_CHIP_UNKNOWN ako nije pronađen / I2C greška.
 */
mpu_chip_t MPU_Init(void);

/* 1 ako je detektiran magnetometar (9250/9255) i radi; inače 0. */
uint8_t MPU_HasMag(void);

/*
 * Pročitaj SIROVI WHO_AM_I bajt (registar 0x75) izravno s magistrale.
 * Za dijagnostiku — usporedi s MPU_WHOAMI_* (0x70=6500, 0x71=9250, 0x73=9255).
 * Vraća 0xFF ako I2C ne odgovori. Mora se zvati nakon Custom_I2C1_Init().
 */
uint8_t MPU_WhoAmI(void);

/* I2C adresa na koju je MPU odgovorio (0x68 ili 0x69), valjano nakon MPU_Init. */
uint8_t MPU_Address(void);

/*
 * Pročitaj senzore, ažuriraj Mahony fuziju i vrati orijentaciju.
 * Zovi periodički iz IMU_Task; `dt_s` je vrijeme od prošlog poziva u sekundama
 * (npr. 0.01 za 100 Hz). Prvih nekoliko sekundi filter konvergira.
 * @return 0 = OK, !=0 = I2C greška (orijentacija nije ažurirana).
 */
int MPU_Update(float dt_s, mpu_orientation_t *out);

/*
 * Žiro bias kalibracija — drži modul MIRNO. Uzme N uzoraka i sprema offset.
 * Pozovi jednom nakon MPU_Init dok je ploča nepomična. Blocking (~0.5 s).
 */
void MPU_CalibrateGyro(void);

#endif /* MPU9250_H */
