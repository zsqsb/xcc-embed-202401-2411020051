# 实验二（新版）：3×4 矩阵键盘 —— 中断驱动 + 事件队列

| 项目 | 内容 |
| --- | --- |
| 实验名称 | 3×4 矩阵键盘扫描与 BCD 显示 |
| 姓名 / 学号 | 汪传贵 / 2411020054 |
| 班级 | 24 电信 1 班 |
| 开发板 | STM32F407ZETX |
| 开发工具 | STM32CubeIDE 2.2.0（GNU Tools for STM32 14.3.rel1） |
| 主频 | 168 MHz |

## 一、功能

3×4 矩阵键盘，按下任意键后，PF0～PF7 的 8 个 LED 以 **8421BCD 码**显示键值（1～12）。
十位 → PF4～PF7，个位 → PF0～PF3，LED 低电平点亮。

## 二、硬件连接

| 端口 | 方向 | 功能 |
| --- | --- | --- |
| PE0～PE3 | 上拉输入 | 4 根列线，无按键时为高电平 |
| PE4～PE6 | 推挽输出 | 3 根行线，扫描时逐行拉低 |
| PF0～PF7 | 推挽输出 | 8 个 LED，8421BCD 显示 |

## 三、设计思路

这个版本没有用"延时消抖 + 主循环轮询"，而是把键盘做成一个**独立的事件源**：

```
TIM6 更新中断（1 kHz）
        │  每 1 ms 进一次
        ▼
MatrixKey_OnTick()  ── 5 ms 分频 ──► 扫描 12 个按键
        │
        │  4 位移位寄存器去抖（连续 4 次采样一致）
        ▼
   按键确认 → 键值入队（环形队列，深度 8）
        │
        ▼
主循环 MatrixKey_Pop() → led_show_bcd()
```

### 1. TIM6 提供 1 ms 节拍

TIM6 挂在 APB1 上，APB1 = 42 MHz，定时器时钟翻倍为 84 MHz：

- 预分频 84 → 计数频率 1 MHz
- 自动重装 1000 → 每 1 ms 产生一次更新中断

TIM6 用寄存器直接配置（`PSC / ARR / DIER / CR1`），不依赖 HAL 的 TIM 驱动。

### 2. 移位寄存器去抖

每个按键维护 1 字节历史，只有低 4 位有效：

```c
s_history[i] = ((s_history[i] << 1) | sample) & 0x0F;
```

- 连续 4 次采样都是 1（`0x0F`）→ 确认按下，产生一次键值；
- 连续 4 次采样都是 0（`0x00`）→ 确认释放。

5 ms 采样 × 4 次 = 20 ms 消抖窗口，不用任何 `HAL_Delay`，也不会卡住主循环。

### 3. 中断只做"采样 + 入队"

中断服务函数里不做显示、不做延时，只把确认后的键值压进环形队列；
主循环按自己的节奏取。这样中断执行时间稳定在几微秒，不会影响其他任务。

## 四、程序结构

| 文件 | 作用 |
| --- | --- |
| `Core/Src/main.c` | 初始化 + 主循环：从队列取键值、刷新 BCD 显示 |
| `Core/Src/tick.c` | TIM6 1 kHz 节拍，中断里调用键盘扫描 |
| `Core/Src/matrix_key.c` | 行扫描、移位寄存器去抖、事件队列 |
| `Core/Inc/tick.h`、`matrix_key.h` | 对外接口 |
| `Core/Src/gpio.c` | CubeMX 生成的引脚初始化 |
| `0602_MatrixKey.ioc` | CubeMX 工程配置 |

主循环只有三行有效逻辑：

```c
while (1)
{
  if (MatrixKey_Pop(&key) != 0u)
  {
    led_show_bcd(key);
  }
}
```

## 五、关键代码

### 1. 1 ms 节拍（寄存器配置 TIM6）

```c
void Tick_Init(void)
{
  RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;

  TIM6->PSC = 84u - 1u;        /* 84 MHz / 84 = 1 MHz */
  TIM6->ARR = 1000u - 1u;      /* 1000 个计数 = 1 ms */

  TIM6->EGR = TIM_EGR_UG;      /* 立即装载 PSC/ARR */
  TIM6->SR  = 0u;
  TIM6->DIER = TIM_DIER_UIE;
  TIM6->CR1  = TIM_CR1_CEN;

  NVIC_SetPriority(TIM6_DAC_IRQn, 2u);
  NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

void TIM6_DAC_IRQHandler(void)
{
  if ((TIM6->SR & TIM_SR_UIF) != 0u)
  {
    TIM6->SR = (uint32_t)~TIM_SR_UIF;   /* 写 0 清标志 */
    s_millis++;
    MatrixKey_OnTick();
  }
}
```

### 2. 行扫描（BSRR 只动 PE4～PE6）

```c
static void row_activate(uint8_t index)
{
  uint32_t pin = (uint32_t)ROW_FIRST_PIN << index;

  ROW_PORT->BSRR = (uint32_t)ROW_ALL_PINS;   /* 先全部拉高 */
  ROW_PORT->BSRR = pin << 16u;               /* 再拉低当前行 */
}
```

### 3. 去抖与入队

```c
s_history[i] = (uint8_t)(((s_history[i] << 1) | sample) & DEBOUNCE_MASK);

if ((s_pressed & mask) == 0u)
{
  if (s_history[i] == DEBOUNCE_MASK)      /* 连续 4 次按下 */
  {
    s_pressed |= mask;
    queue_push(s_key_code[i]);            /* 键值入队 */
  }
}
```

## 六、键值映射

开发板键盘的物理行序与 PE4～PE6 的接线顺序不同，代码里用一张表描述：

```c
static const uint8_t s_key_code[12] =
{
  9u, 10u, 11u, 12u,   /* 扫描行 0 = 键盘第 3 行 */
  1u,  2u,  3u,  4u,   /* 扫描行 1 = 键盘第 1 行 */
  5u,  6u,  7u,  8u    /* 扫描行 2 = 键盘第 2 行 */
};
```

如果上板发现键值对不上，只改这张表即可。

## 七、编译验证

| 方式 | 结果 |
| --- | --- |
| 命令行 GNU Tools for STM32 14.3.rel1（`-Wall -Wextra`） | 23/23 文件编译通过，0 错误 0 警告，链接成功 |
| STM32CubeIDE 2.2.0 headless build（Debug / Release） | `Build Finished. 0 errors, 0 warnings` |

```
   text    data     bss     dec
   4968      12    1604    6584
```

## 八、使用方法

1. STM32CubeIDE → File → Import → Existing Projects into Workspace，选择本文件夹；
2. Project → Build（首次会自动生成 `Debug/` 目录）；
3. 接好开发板，Debug/Run 下载；
4. 逐个按键，观察 8 个 LED 的 BCD 显示。

## 九、说明

- TIM6 中断优先级设为 2，低于 SysTick（HAL 计时用），互不影响；
- 环形队列满时会丢弃新事件，保证中断不被拖住；
- 同时按下多个键时，按扫描顺序取第一个确认的键；
- 工程里没有 `HAL_Delay`，也不依赖 `HAL_GetTick` 做节拍。
