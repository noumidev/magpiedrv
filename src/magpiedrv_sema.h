/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv_sema.h - Magpie semaphore */

#pragma once

#include <pspsdk.h>

extern SceUID magpiedrv_sema_evtflg;

typedef enum {
    MAGPIEDRV_SEMAEVENT_READY       = 0x04,
    MAGPIEDRV_SEMAEVENT_REPLY_READY = 0x10,
    MAGPIEDRV_SEMAEVENT_HIF_READY   = 0x80,
} magpiedrv_SemaEvent;

int magpiedrv_sema_Initialize(void);

int magpiedrv_sema_Wait(SceUInt timeout);
int magpiedrv_sema_WaitHif(void);
