xcc-embed-202401-2411020051

> 嵌入式系统课程作业仓库

## 个人信息

- 姓名：周诗淇
- 班级：2024级 电信1班
- 学号：2411020051

## 仓库说明

本仓库用于存放嵌入式系统课程全部实验代码，所有实验统一存放在此公开仓库，不新建多个独立仓库。使用 GitHub Desktop 完成本地 commit + push，保留完整开发提交记录，不使用网页拖拽上传。

## 仓库目录

```
xcc-embed-202401-2411020051
├── README.md
├── .gitignore
├── lab01-led          # 实验1：LED 流水灯
└── lab02-keyboard     # 实验2：4×4 矩阵键盘（后续新增）
```

## .gitignore 功能说明

过滤 Keil、STM32CubeMX 编译临时文件、axf、hex 以及庞大的 HAL 驱动库 Drivers 文件夹，只保留核心工程源码。

## 实验记录

### lab01-led

- 实验名称：STM32 LED 流水灯
- 功能：实现 LED 循环流水点亮（左右往返）
- 开发平台：STM32 开发板 + STM32CubeMX + Keil5

### lab02-keyboard（后续新增）

- 实验名称：STM32 4×4 矩阵键盘
- 功能：矩阵按键扫描，按键检测
- 开发平台：STM32 开发板 + STM32CubeMX + Keil5

