/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv_cmd.h - Magpie commands */

#pragma once

#include <pspsdk.h>

int magpiedrv_SendIplBlock(const u16 len, u8* buf);
