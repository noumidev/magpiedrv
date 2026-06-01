/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv_msif_init.h - Memory Stick I/F initialization */

#pragma once

int magpiedrv_msif_InstallInterruptHandler(void);

int magpiedrv_msif_InitializeInterfaceImpl(void);
void magpiedrv_msif_PowerInterface(void);
void magpiedrv_msif_EnableInterface(void);
void magpiedrv_msif_ResetInterface(void);

void magpiedrv_msif_SelectClock(const int clock);
