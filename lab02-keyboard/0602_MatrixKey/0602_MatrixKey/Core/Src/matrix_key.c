/**
  ******************************************************************************
  * @file    matrix_key.c
  * @brief   3×4 矩阵键盘：逐行扫描 + 移位寄存器去抖 + 事件队列
  *
  * 与常见写法的区别：
  *   1. 扫描完全由 TIM6 的 1 ms 中断驱动，主循环里没有任何延时或轮询死等；
  *   2. 去抖不靠 HAL_Delay，而是给每个按键维护一个 4 位移位寄存器，
  *      连续 4 次采样都为"按下"才确认，连续 4 次都为"松开"才释放；
  *   3. 中断只负责把键值压进环形队列，主循环按自己的节奏取，
  *      这样以后加显示、串口、控制逻辑都不会影响按键响应。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "matrix_key.h"
#include "stm32f4xx_hal.h"

/* Private define ------------------------------------------------------------*/
#define ROW_NUM            3u
#define COL_NUM            4u
#define KEY_NUM            (ROW_NUM * COL_NUM)     /* 12 个按键 */

#define SCAN_PERIOD_MS     5u                      /* 每 5 ms 扫一遍 */
#define DEBOUNCE_MASK      0x0Fu                   /* 4 次采样全 1 / 全 0 */

#define ROW_PORT           GPIOE
#define ROW_FIRST_PIN      GPIO_PIN_4              /* 行线 PE4~PE6 */
#define ROW_ALL_PINS       (GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6)

#define COL_PORT           GPIOE
#define COL_ALL_PINS       (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)

#define QUEUE_DEPTH        8u

/* Private variables ---------------------------------------------------------*/

/**
  * 扫描顺序 → 键值。
  * 开发板上键盘的物理行序与 PE4~PE6 的接线顺序不一样，
  * 这里用一张表对应：扫描第 0 行实际是键盘的第 3 行（键 9~12）……
  * 换板子时只改这张表即可。
  */
static const uint8_t s_key_code[KEY_NUM] =
{
  9u, 10u, 11u, 12u,   /* 扫描行 0 */
  1u,  2u,  3u,  4u,   /* 扫描行 1 */
  5u,  6u,  7u,  8u    /* 扫描行 2 */
};

static uint8_t  s_history[KEY_NUM];   /* 每个键 4 次采样历史，bit0 最新 */
static uint16_t s_pressed;            /* 去抖确认后的按下位图 */
static uint8_t  s_queue[QUEUE_DEPTH];
static uint8_t  s_qhead;
static uint8_t  s_qtail;
static uint8_t  s_qcount;
static uint8_t  s_divider;            /* 1 ms → 5 ms 分频 */

/* Private functions ---------------------------------------------------------*/

/** 三根行线全部恢复高电平 */
static void rows_release(void)
{
  ROW_PORT->BSRR = (uint32_t)ROW_ALL_PINS;
}

/** 只把第 index 行拉低 */
static void row_activate(uint8_t index)
{
  uint32_t pin = (uint32_t)ROW_FIRST_PIN << index;

  ROW_PORT->BSRR = (uint32_t)ROW_ALL_PINS;   /* 先全部拉高，保证只有一行低 */
  ROW_PORT->BSRR = pin << 16u;               /* 再拉低当前行 */
}

/**
  * @brief  完整扫描一次，返回 12 位按键位图（bit=1 表示该键按下）
  */
static uint16_t read_matrix(void)
{
  uint16_t bitmap = 0u;
  uint32_t cols;
  uint8_t  row;
  uint8_t  col;

  for (row = 0u; row < ROW_NUM; row++)
  {
    row_activate(row);

    /* 行线电平切换后，列线（上拉）需要一点时间回到高电平 */
    for (volatile uint8_t settle = 0u; settle < 30u; settle++)
    {
      __NOP();
    }

    cols = COL_PORT->IDR & (uint32_t)COL_ALL_PINS;

    for (col = 0u; col < COL_NUM; col++)
    {
      if ((cols & (uint32_t)(1u << col)) == 0u)      /* 读到低电平 = 该列被按下 */
      {
        bitmap |= (uint16_t)(1u << (row * COL_NUM + col));
      }
    }
  }

  rows_release();
  return bitmap;
}

/** 键值入队；队列满时丢弃最新事件，保证中断不会被拖住 */
static void queue_push(uint8_t key)
{
  if (s_qcount >= QUEUE_DEPTH)
  {
    return;
  }

  s_queue[s_qtail] = key;
  s_qtail = (uint8_t)((s_qtail + 1u) % QUEUE_DEPTH);
  s_qcount++;
}

/* Exported functions --------------------------------------------------------*/

void MatrixKey_Init(void)
{
  uint8_t i;

  for (i = 0u; i < KEY_NUM; i++)
  {
    s_history[i] = 0u;
  }

  s_pressed = 0u;
  s_qhead   = 0u;
  s_qtail   = 0u;
  s_qcount  = 0u;
  s_divider = 0u;

  rows_release();
}

void MatrixKey_OnTick(void)
{
  uint16_t raw;
  uint8_t  i;

  /* 1 ms 节拍分频到 5 ms，降低中断里的扫描开销 */
  s_divider++;
  if (s_divider < SCAN_PERIOD_MS)
  {
    return;
  }
  s_divider = 0u;

  raw = read_matrix();

  for (i = 0u; i < KEY_NUM; i++)
  {
    uint16_t mask   = (uint16_t)(1u << i);
    uint8_t  sample = (uint8_t)(((raw & mask) != 0u) ? 1u : 0u);

    /* 移位寄存器：最新采样进 bit0，旧的左移出去 */
    s_history[i] = (uint8_t)(((s_history[i] << 1) | sample) & DEBOUNCE_MASK);

    if ((s_pressed & mask) == 0u)
    {
      if (s_history[i] == DEBOUNCE_MASK)         /* 连续 4 次按下 → 确认 */
      {
        s_pressed |= mask;
        queue_push(s_key_code[i]);
      }
    }
    else
    {
      if (s_history[i] == 0u)                    /* 连续 4 次松开 → 释放 */
      {
        s_pressed &= (uint16_t)(~mask);
      }
    }
  }
}

uint8_t MatrixKey_Pop(uint8_t *key)
{
  if ((s_qcount == 0u) || (key == 0))
  {
    return 0u;
  }

  *key = s_queue[s_qhead];
  s_qhead = (uint8_t)((s_qhead + 1u) % QUEUE_DEPTH);
  s_qcount--;

  return 1u;
}
