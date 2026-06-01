/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv_ioreg.h - MMI/O definitions */

#pragma once

#include <pspsdk.h>

// System control
#define HW_SYSCTRL_RESETEN   *(volatile u32*)0xBC10004C
#define HW_SYSCTRL_BUSCLKEN  *(volatile u32*)0xBC100050
#define HW_SYSCTRL_CLKEN_LO  *(volatile u32*)0xBC100054
#define HW_SYSCTRL_CLKSEL_LO *(volatile u32*)0xBC10005C
#define HW_SYSCTRL_IOEN      *(volatile u32*)0xBC100078

#define SYSCTRL_RESETEN_MSIF1   (1 << 9)
#define SYSCTRL_BUSCLKEN_MSIF1  (1 << 11)
#define SYSCTRL_CLKEN_LO_MSIF1  (1 << 9)
#define SYSCTRL_CLKSEL_LO_MSIF1 (3 << 2)
#define SYSCTRL_IOEN_MSIF1      (1 << 5)

// Wi-Fi Memory Stick I/F
#define HW_MSIF1_COMMAND *(volatile u32*)0xBD300030
#define HW_MSIF1_DATA    *(volatile u32*)0xBD300034
#define HW_MSIF1_STATUS  *(volatile u32*)0xBD300038
#define HW_MSIF1_CONTROL *(volatile u32*)0xBD30003C
#define HW_MSIF1_DMACTRL *(volatile u32*)0xBD300040

// Big Thank You to Hedge from the PSP Homebrew Community Discord for the
// Memory Stick I/F register bit definitions!
#define MSIF1_STATUS_BAD_CMD       (1 << 0)
#define MSIF1_STATUS_BUF_REQUEST   (1 << 1)
#define MSIF1_STATUS_CMD_COMPLETED (1 << 3)
#define MSIF1_STATUS_TIMEOUT       (1 << 8)
#define MSIF1_STATUS_CRC_ERROR     (1 << 9)
#define MSIF1_STATUS_READY         (1 << 12)
#define MSIF1_STATUS_INTERRUPT     (1 << 13)
#define MSIF1_STATUS_DMA_REQUEST   (1 << 14)

#define MSIF1_CONTROL_REI        (1 << 4)
#define MSIF1_CONTROL_DRQ_SL     (1 << 5)
#define MSIF1_CONTROL_FIFO_WRITE (1 << 8)
#define MSIF1_CONTROL_FIFO_CLEAR (1 << 9)
#define MSIF1_CONTROL_INT_CLEAR  (1 << 11)
#define MSIF1_CONTROL_INT_ENABLE (1 << 13)
#define MSIF1_CONTROL_4BIT_MODE  (1 << 14)
