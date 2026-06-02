/**
  ******************************************************************************
  * @file    uart_stream.c
  * @brief   USART3 binary streaming over DMA (LL drivers).
  *          PB10 = USART3_TX -> external USB-UART adapter (MATLAB COM port).
  ******************************************************************************
  */
#include "uart_stream.h"
#include "main.h"

static volatile bool tx_busy = false;

static void uart_gpio_init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);

  /* USART3 GPIO Configuration
     PB10 ------> USART3_TX  (wire to USB-UART adapter RX)
     PB11 ------> USART3_RX
  */
  GPIO_InitStruct.Mode       = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed      = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull       = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate  = LL_GPIO_AF_7;

  GPIO_InitStruct.Pin = LL_GPIO_PIN_10;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = LL_GPIO_PIN_11;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static void uart_dma_init(void)
{
  /* DMA1_Channel2: memory -> USART3 TDR, byte-wide, normal mode. */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_2, LL_DMAMUX_REQ_USART3_TX);
  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_2, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PRIORITY_LOW);
  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MODE_NORMAL);
  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PERIPH_NOINCREMENT);
  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PDATAALIGN_BYTE);
  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MDATAALIGN_BYTE);
  LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_2,
                          LL_USART_DMA_GetRegAddr(USART3, LL_USART_DMA_REG_DATA_TRANSMIT));
}

void UartStream_Init(void)
{
  LL_USART_InitTypeDef USART_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART3;
  PeriphClkInit.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3);

  uart_gpio_init();
  uart_dma_init();

  USART_InitStruct.PrescalerValue      = LL_USART_PRESCALER_DIV1;
  USART_InitStruct.BaudRate            = 115200;
  USART_InitStruct.DataWidth           = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits            = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity              = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection   = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling        = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(USART3, &USART_InitStruct);

  LL_USART_SetTXFIFOThreshold(USART3, LL_USART_FIFOTHRESHOLD_1_8);
  LL_USART_SetRXFIFOThreshold(USART3, LL_USART_FIFOTHRESHOLD_1_8);
  LL_USART_DisableFIFO(USART3);
  LL_USART_ConfigAsyncMode(USART3);

  LL_USART_Enable(USART3);
  while ((!(LL_USART_IsActiveFlag_TEACK(USART3))) || (!(LL_USART_IsActiveFlag_REACK(USART3))))
  {
  }
}

bool UartStream_Busy(void)
{
  return tx_busy;
}

void UartStream_Send(const uint8_t *data, uint16_t len)
{
  if (tx_busy || len == 0U)
  {
    return;
  }
  tx_busy = true;

  LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)data);
  LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, len);
  LL_DMA_ClearFlag_TC2(DMA1);
  LL_DMA_ClearFlag_TE2(DMA1);
  LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_2);

  LL_USART_ClearFlag_TC(USART3);
  LL_USART_EnableDMAReq_TX(USART3);
  LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);
}

void UartStream_DmaIrqHandler(void)
{
  if (LL_DMA_IsActiveFlag_TC2(DMA1))
  {
    LL_DMA_ClearFlag_TC2(DMA1);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
    LL_USART_DisableDMAReq_TX(USART3);
    tx_busy = false;
  }

  if (LL_DMA_IsActiveFlag_TE2(DMA1))
  {
    LL_DMA_ClearFlag_TE2(DMA1);
  }
}
