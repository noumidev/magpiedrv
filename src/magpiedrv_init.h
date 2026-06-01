/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv_init.h - Magpie initialization */

#pragma once

#include <pspsdk.h>

int magpiedrv_InitializeMagpieImpl(void);
int magpiedrv_UploadFirmwareImpl(const u32 len, u8* firm_buf);
