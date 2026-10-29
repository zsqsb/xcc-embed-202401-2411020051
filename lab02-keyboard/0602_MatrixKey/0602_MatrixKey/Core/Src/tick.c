/**
  ******************************************************************************
  * @file    tick.c
  * @brief   1 kHz 系统节拍：TIM6 定时器中断（寄存器方式配置）
  *
  * TIM6 是基本定时器，挂在 APB1 上：
  *   APB1 = 42 MHz，定时器时钟 = 84 MHz（APB1 分频不为 1 时翻倍）
  *   预分频 84 → 计数频率 1 MHz
  *   自动重装 1000 → 每 1000 个计数产生一次更新中断 = 1 ms
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "tick.h"
#include "stm32f4xx_hal.h"
#include "matrix_key.h"

/* Private define ------------------------------------------------------------*/
#define TIM6_CLK_HZ       84000000u                        /* 定时器输入时钟 */
#define TICK_COUNTER_HZ   1000000u                         /* 计数频率 1 MHz */
#define TICK_PSC          (TIM6_CLK_HZ / TICK_COUNTER_HZ)  /* 预分频值 84 */
#define TICK_ARR          (TICK_COUNTER_HZ / 1000u)        /* 1000 次 = 1 ms */
#define TICK_IRQ_PRIO     2u

/* Private variables ---------------------------------------------------------*/
static volatile uint32_t s_millis;

/* Exported functions --------------------------------------------------------*/

void Tick_Init(void)
{
  /* 1. 打开 TIM6 时钟（APB1 外设） */
  RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;

  /* 2. 配置时基：PSC / ARR 都要减 1，因为计数器从 0 开始 */
  TIM6->PSC = (uint32_t)(TICK_PSC - 1u);
  TIM6->ARR = (uint32_t)(TICK_ARR - 1u);

  /* 3. 产生一次更新事件，让 PSC/ARR 立即生效，同时清掉更新标志 */
  TIM6->EGR = TIM_EGR_UG;
  TIM6->SR  = 0u;

  /* 4. 允许更新中断并启动计数 */
  TIM6->DIER = TIM_DIER_UIE;
  TIM6->CR1  = TIM_CR1_CEN;

  /* 5. 使能 NVIC 中断（优先级低于 SysTick，避免影响 HAL 计时） */
  NVIC_SetPriority(TIM6_DAC_IRQn, TICK_IRQ_PRIO);
  NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

uint32_t Tick_Millis(void)
{
  return s_millis;
}

/**
  * @brief  TIM6 更新中断：1 ms 一次
  * @note   STM32F4 里 TIM6 与 DAC 共用中断向量，所以函数名固定为 TIM6_DAC_IRQHandler
  */
void TIM6_DAC_IRQHandler(void)
{
  if ((TIM6->SR & TIM_SR_UIF) != 0u)
  {
    TIM6->SR = (uint32_t)~TIM_SR_UIF;   /* 状态寄存器是写 0 清除 */
    s_millis++;

    MatrixKey_OnTick();                 /* 键盘扫描挂在这个节拍上 */
  }
}
