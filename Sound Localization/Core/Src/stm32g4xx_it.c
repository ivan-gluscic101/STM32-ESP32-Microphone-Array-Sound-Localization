/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32g4xx_it.c
  * @brief   Interrupt Service Routines — ISPRAVLJENO
  ******************************************************************************
  *
  *  ISPRAVKA:
  *    Queue handle preimenovan: queueXommDataHandle → queueDmaEventHandle
  *    (jasnije ime koje opisuje svrhu)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "stm32g4xx_it.h"

/* USER CODE BEGIN Includes */
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
  * @brief DMA1 Channel1 — ADC half-buffer i full-buffer interrupt.
  *
  * Kad DMA napuni prvu polovicu buffera (HT), šaljemo 0 u queue.
  * Kad DMA napuni drugu polovicu (TC), šaljemo 1 u queue.
  * processTask čita queue i obrađuje odgovarajuću polovicu.
  *
  * SIGURNOST:
  *   osMessagePut s timeout=0 je FreeRTOS ISR-safe (koristi xQueueSendFromISR).
  *   Ako je queue pun, poruka se odbacuje — to znači da processTask kasni,
  *   što se ne bi trebalo dogoditi s razdvojenim taskovima (obrada < 1 ms).
  */
void DMA1_Channel1_IRQHandler(void)
{
  if (LL_DMA_IsActiveFlag_HT1(DMA1))
  {
      LL_DMA_ClearFlag_HT1(DMA1);
      osMessagePut(queueDmaEventHandle, 0U, 0);   /* ping: prva polovica */
  }
  if (LL_DMA_IsActiveFlag_TC1(DMA1))
  {
      LL_DMA_ClearFlag_TC1(DMA1);
      osMessagePut(queueDmaEventHandle, 1U, 0);   /* pong: druga polovica */
  }
}

void ADC1_2_IRQHandler(void)
{
}

void EXTI15_10_IRQHandler(void)
{
  if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_13) != RESET)
  {
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_13);
  }
}

void TIM6_DAC_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim6);
}

/* USER CODE BEGIN 1 */
/* USER CODE END 1 */