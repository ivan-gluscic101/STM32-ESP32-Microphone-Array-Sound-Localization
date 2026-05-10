/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32g4xx_it.c
  * @brief   Interrupt Service Routines
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "stm32g4xx_it.h"
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "queue.h"
#include "cmsis_os.h"
/* USER CODE END Includes */

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim6;

/* USER CODE BEGIN EV */
extern osMessageQId queueDmaEventHandle;
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/

void NMI_Handler(void)
{
  while (1) {}
}

void HardFault_Handler(void)
{
  while (1) {}
}

void MemManage_Handler(void)
{
  while (1) {}
}

void BusFault_Handler(void)
{
  while (1) {}
}

void UsageFault_Handler(void)
{
  while (1) {}
}

void DebugMon_Handler(void)
{
}

/******************************************************************************/
/* STM32G4xx Peripheral Interrupt Handlers                                    */
/******************************************************************************/

/**
 * @brief DMA1 Channel 1 — ADC double-buffer half/full transfer.
 *
 * Šalje flag u queueDmaEventHandle:
 *   0 = prva polovica adc_buffer[] je gotova (HT interrupt)
 *   1 = druga polovica adc_buffer[] je gotova (TC interrupt)
 *
 * NVIC prioritet = 5 = configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY →
 * smije koristiti xQueueSendFromISR.
 */
void DMA1_Channel1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel1_IRQn 0 */
  BaseType_t woken = pdFALSE;
  uint32_t   msg;

  if (LL_DMA_IsActiveFlag_HT1(DMA1)) {
    LL_DMA_ClearFlag_HT1(DMA1);
    msg = 0u;
    xQueueSendFromISR((QueueHandle_t)queueDmaEventHandle, &msg, &woken);
  }
  if (LL_DMA_IsActiveFlag_TC1(DMA1)) {
    LL_DMA_ClearFlag_TC1(DMA1);
    msg = 1u;
    xQueueSendFromISR((QueueHandle_t)queueDmaEventHandle, &msg, &woken);
  }
  portYIELD_FROM_ISR(woken);
  /* USER CODE END DMA1_Channel1_IRQn 0 */
}

/**
 * @brief ADC1/ADC2 global interrupt — nije korišten; ostavljen prazan.
 */
void ADC1_2_IRQHandler(void)
{
}

/**
 * @brief EXTI line[15:10] — korisnička tipka (PC13).
 */
void EXTI15_10_IRQHandler(void)
{
  if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_13) != RESET)
  {
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_13);
  }
}

/**
 * @brief TIM6 — FreeRTOS tick (HAL_IncTick pozvan iz HAL_TIM_PeriodElapsedCallback).
 */
void TIM6_DAC_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim6);
}

/* USER CODE BEGIN 1 */
/* USER CODE END 1 */
