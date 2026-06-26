#include "i2c_driver.h"
#include "main.h"          /* LL headeri (ll_i2c, ll_gpio, ll_bus, ll_rcc) */

/* ── Timeout ──────────────────────────────────────────────────────────────────
 * Busy-wait brojač; na 170 MHz ~ par ms. Dovoljno za 400 kHz transfere od
 * nekoliko bajtova (najduži MPU burst je ~21 B = ~0.5 ms). */
#define I2C_TIMEOUT   200000u

/*
 * I2C1 TIMINGR za PCLK1 = 170 MHz, 400 kHz Fast-mode.
 * Vrijednost iz STM32CubeMX I2C Timing kalkulatora (G4 @ 170 MHz, Fm 400 kHz).
 * Ako mijenjaš PCLK1, regeneriraj ovu vrijednost (CubeMX → I2C → Timing). */
#define I2C1_TIMINGR  0x30A0A7FBu

/* Čeka dok se flag ne postavi; vraća -1 na timeout, 0 na uspjeh. */
#define WAIT_FLAG(cond)                                  \
    do {                                                 \
        uint32_t _t = I2C_TIMEOUT;                       \
        while (!(cond)) { if (--_t == 0u) return -1; }   \
    } while (0)

void Custom_I2C1_Init(void)
{
    /* 1. Clock: GPIOB + I2C1 */
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);

    /* I2C1 clock source = PCLK1 */
    LL_RCC_SetI2CClockSource(LL_RCC_I2C1_CLKSOURCE_PCLK1);

    /* 2. GPIO PB8 (SCL), PB9 (SDA): AF4, open-drain, pull-up, high speed */
    LL_GPIO_InitTypeDef gpio = {0};
    gpio.Pin        = LL_GPIO_PIN_8 | LL_GPIO_PIN_9;
    gpio.Mode       = LL_GPIO_MODE_ALTERNATE;
    gpio.Speed      = LL_GPIO_SPEED_FREQ_HIGH;
    gpio.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
    gpio.Pull       = LL_GPIO_PULL_UP;     /* GY-521 ima i svoje 4.7k pull-upove */
    gpio.Alternate  = LL_GPIO_AF_4;
    LL_GPIO_Init(GPIOB, &gpio);

    /* 3. I2C init preko LL */
    LL_I2C_Disable(I2C1);

    LL_I2C_InitTypeDef i2c = {0};
    i2c.PeripheralMode  = LL_I2C_MODE_I2C;
    i2c.Timing          = I2C1_TIMINGR;
    i2c.AnalogFilter    = LL_I2C_ANALOGFILTER_ENABLE;
    i2c.DigitalFilter   = 0u;
    i2c.OwnAddress1     = 0u;
    i2c.TypeAcknowledge = LL_I2C_ACK;
    i2c.OwnAddrSize     = LL_I2C_OWNADDRESS1_7BIT;
    LL_I2C_Init(I2C1, &i2c);

    LL_I2C_Enable(I2C1);
}

int I2C1_WriteReg(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
    /* Transfer: NBYTES = 1 (reg) + len, AUTOEND šalje STOP na kraju.
     * SADD se predaje kao 8-bitno polje (7-bit adresa << 1). */
    LL_I2C_HandleTransfer(I2C1, (uint32_t)dev_addr << 1, LL_I2C_ADDRSLAVE_7BIT,
                          (uint32_t)(len + 1u), LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_WRITE);

    /* Pošalji registar */
    WAIT_FLAG(LL_I2C_IsActiveFlag_TXIS(I2C1) || LL_I2C_IsActiveFlag_NACK(I2C1));
    if (LL_I2C_IsActiveFlag_NACK(I2C1)) goto nack;
    LL_I2C_TransmitData8(I2C1, reg);

    /* Pošalji podatke */
    for (uint16_t i = 0u; i < len; i++) {
        WAIT_FLAG(LL_I2C_IsActiveFlag_TXIS(I2C1) || LL_I2C_IsActiveFlag_NACK(I2C1));
        if (LL_I2C_IsActiveFlag_NACK(I2C1)) goto nack;
        LL_I2C_TransmitData8(I2C1, data[i]);
    }

    WAIT_FLAG(LL_I2C_IsActiveFlag_STOP(I2C1));
    LL_I2C_ClearFlag_STOP(I2C1);
    return 0;

nack:
    LL_I2C_ClearFlag_NACK(I2C1);
    LL_I2C_ClearFlag_STOP(I2C1);
    return -2;
}

int I2C1_WriteByte(uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    return I2C1_WriteReg(dev_addr, reg, &val, 1u);
}

int I2C1_ReadReg(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    /* Faza 1: zapiši adresu registra BEZ stopa (SOFTEND → repeated start slijedi) */
    LL_I2C_HandleTransfer(I2C1, (uint32_t)dev_addr << 1, LL_I2C_ADDRSLAVE_7BIT,
                          1u, LL_I2C_MODE_SOFTEND,
                          LL_I2C_GENERATE_START_WRITE);

    WAIT_FLAG(LL_I2C_IsActiveFlag_TXIS(I2C1) || LL_I2C_IsActiveFlag_NACK(I2C1));
    if (LL_I2C_IsActiveFlag_NACK(I2C1)) goto nack;
    LL_I2C_TransmitData8(I2C1, reg);

    /* Čekaj kraj prijenosa registra (TC), pa repeated start za čitanje */
    WAIT_FLAG(LL_I2C_IsActiveFlag_TC(I2C1) || LL_I2C_IsActiveFlag_NACK(I2C1));
    if (LL_I2C_IsActiveFlag_NACK(I2C1)) goto nack;

    /* Faza 2: read s AUTOEND (STOP nakon len bajtova) */
    LL_I2C_HandleTransfer(I2C1, (uint32_t)dev_addr << 1, LL_I2C_ADDRSLAVE_7BIT,
                          (uint32_t)len, LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_READ);

    for (uint16_t i = 0u; i < len; i++) {
        WAIT_FLAG(LL_I2C_IsActiveFlag_RXNE(I2C1) || LL_I2C_IsActiveFlag_NACK(I2C1));
        if (LL_I2C_IsActiveFlag_NACK(I2C1)) goto nack;
        data[i] = LL_I2C_ReceiveData8(I2C1);
    }

    WAIT_FLAG(LL_I2C_IsActiveFlag_STOP(I2C1));
    LL_I2C_ClearFlag_STOP(I2C1);
    return 0;

nack:
    LL_I2C_ClearFlag_NACK(I2C1);
    LL_I2C_ClearFlag_STOP(I2C1);
    return -2;
}

int I2C1_Ping(uint8_t dev_addr)
{
    /* Adresni okvir za upis 0 bajtova; ACK = uređaj prisutan */
    LL_I2C_HandleTransfer(I2C1, (uint32_t)dev_addr << 1, LL_I2C_ADDRSLAVE_7BIT,
                          0u, LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_WRITE);

    /* Čekaj STOP (ACK) ili NACK (nema uređaja) */
    {
        uint32_t t = I2C_TIMEOUT;
        while (!LL_I2C_IsActiveFlag_STOP(I2C1) && !LL_I2C_IsActiveFlag_NACK(I2C1)) {
            if (--t == 0u) { LL_I2C_ClearFlag_STOP(I2C1); return -1; }
        }
    }

    if (LL_I2C_IsActiveFlag_NACK(I2C1)) {
        LL_I2C_ClearFlag_NACK(I2C1);
        LL_I2C_ClearFlag_STOP(I2C1);
        return -2;   /* NACK → nema uređaja */
    }
    LL_I2C_ClearFlag_STOP(I2C1);
    return 0;
}
