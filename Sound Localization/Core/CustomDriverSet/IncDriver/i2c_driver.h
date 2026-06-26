#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

/*
 * i2c_driver.h — Minimalni blocking I2C1 master driver (bare-metal registri).
 *
 * Spoj na Nucleo G474RE:
 *   I2C1_SCL = PB8  (AF4)
 *   I2C1_SDA = PB9  (AF4)
 *   400 kHz Fast-mode, 7-bitno adresiranje.
 *
 * Ne koristi HAL/LL I2C — radi direktno na I2C1 registrima (CMSIS device
 * header) pa ne zahtijeva dodavanje stm32g4xx_*_i2c.c u build. Sve operacije
 * su blocking s timeoutom; zovu se iz IMU_Task (niski prioritet), nikad iz ISR.
 *
 * Pinovi su odabrani da ne konfliktiraju s: ADC (PB14, PC0-2), UART4 (PC10/11),
 * TIM8, SWD (PA13/14/PB3).
 */

#include <stdint.h>

/* Inicijalizira GPIO (PB8/PB9 AF open-drain + pull-up) i I2C1 @ 400 kHz. */
void Custom_I2C1_Init(void);

/*
 * Upiši `len` bajtova u registar `reg` na uređaju `dev_addr` (7-bit).
 * @return 0 = OK, !=0 = greška (NACK / timeout).
 */
int I2C1_WriteReg(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len);

/* Upiši jedan bajt u registar (convenience). @return 0 = OK. */
int I2C1_WriteByte(uint8_t dev_addr, uint8_t reg, uint8_t val);

/*
 * Pročitaj `len` bajtova počevši od registra `reg` (auto-increment) s uređaja.
 * @return 0 = OK, !=0 = greška.
 */
int I2C1_ReadReg(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len);

/*
 * Provjeri odaziva li se uređaj na adresi `dev_addr` (ACK na adresni okvir).
 * @return 0 = uređaj prisutan, !=0 = nema odaziva.
 */
int I2C1_Ping(uint8_t dev_addr);

#endif /* I2C_DRIVER_H */
