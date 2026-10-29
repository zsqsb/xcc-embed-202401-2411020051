/**
  ******************************************************************************
  * @file    matrix_key.h
  * @brief   3×4 矩阵键盘：移位寄存器去抖 + 事件队列
  ******************************************************************************
  */

#ifndef __MATRIX_KEY_H
#define __MATRIX_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** 队列为空时的返回值 */
#define MATRIX_KEY_NONE   0u

/**
  * @brief  清空状态，行线全部拉高
  * @note   必须在 Tick_Init() 之前调用
  */
void MatrixKey_Init(void);

/**
  * @brief  1 ms 节拍回调（由 TIM6 中断调用），内部每 5 ms 扫描一次
  * @note   中断里只做采样、去抖和入队，不做任何延时等待
  */
void MatrixKey_OnTick(void);

/**
  * @brief  从事件队列取一个键值
  * @param  key 输出参数，取值 1~12
  * @retval 1 = 取到键值；0 = 队列为空
  */
uint8_t MatrixKey_Pop(uint8_t *key);

#ifdef __cplusplus
}
#endif

#endif /* __MATRIX_KEY_H */
