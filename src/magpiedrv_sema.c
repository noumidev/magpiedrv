/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv_sema.c - Magpie semaphore */

#include "magpiedrv_sema.h"

#include <stdbool.h>

#include <pspsdk.h>

#include "magpiedrv.h"
#include "magpiedrv_msif_cmd.h"
#include "magpiedrv_msif_pkt.h"
#include "LibPspExploit/libpspexploit.h"

#define HIF_RETRIES 4

static SceUID sema_tid;
SceUID magpiedrv_sema_evtflg;

static bool hif_ready = false;

/* Checks the Magpie host I/F status */
static int GetSema(u8* sema) {
    magpiedrv_Buffer buf;

    memset(buf.bytes, 0, sizeof(buf));

    *sema = 0;

    while (true) {
        magpiedrv_msif_SyncPacket(false);

        if (magpiedrv_msif_ReadReg(0x54, 1, buf.bytes) < 0) {
            return -1;
        }

        *sema = buf.bytes[0];

        if (!hif_ready && ((*sema & 0x80) != 0)) {
            MAGPIEDRV_LOCK_INTERRUPTS {
                hif_ready = true;
            }
        
            sceKernelSetEventFlag(magpiedrv_sema_evtflg, MAGPIEDRV_SEMAEVENT_HIF_READY);
        }

        if ((*sema & 0x1F) != 0) {
            return MAGPIEDRV_ERROR_OK;
        }

        if (*sema != 0) {
            buf.bytes[0] = ~(*sema);

            magpiedrv_msif_WriteReg(0x54, 1, buf.bytes);
        }
    }

    return MAGPIEDRV_ERROR_OK;
}

/* Handles (most) host I/F events */
static int sema_thread_main(const SceSize args, void* argp) {
    (void)args;
    (void)argp;

    u8 sema;

    while (true) {
        if (GetSema(&sema) < 0) {
            sema = 0x10;
        }

        if ((sema & 4) != 0) {
            sceKernelSetEventFlag(magpiedrv_sema_evtflg, MAGPIEDRV_SEMAEVENT_READY);
        }

        if ((sema & 0x10) != 0) {
            sceKernelSetEventFlag(magpiedrv_sema_evtflg, MAGPIEDRV_SEMAEVENT_REPLY_READY);
        }
    }

    return 0;
}

int magpiedrv_sema_Initialize(void) {
    static bool IS_INITIALIZED = false;

    if (IS_INITIALIZED) {
        return MAGPIEDRV_ERROR_ALREADY_INITED;
    }

    magpiedrv_sema_evtflg = sceKernelCreateEventFlag("magpiedrv_sema_evtflg", 0x201, 0, NULL);

    if (magpiedrv_sema_evtflg < 0) {
        return MAGPIEDRV_ERROR_NO_EVTFLG;
    }

    sema_tid = sceKernelCreateThread(
        "Sema Thread",
        (SceKernelThreadEntry)KERNELIFY(sema_thread_main),
        0x11,
        0xFA0,
        0,
        NULL
    );

    if (sema_tid < 0) {
        return -1;
    } else {
        sceKernelStartThread(sema_tid, 0, NULL);
    }

    return MAGPIEDRV_ERROR_OK;
}

int magpiedrv_sema_Wait(SceUInt timeout) {
    timeout *= 1000;

    if (sceKernelWaitEventFlag(magpiedrv_sema_evtflg, MAGPIEDRV_SEMAEVENT_READY, PSP_EVENT_WAITOR, NULL, &timeout) == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
        magpiedrv_Buffer buf;

        memset(buf.bytes, 0, sizeof(buf));

        magpiedrv_msif_ReadReg(0x54, 1, buf.bytes);

        u8 sema = buf.bytes[0];

        while ((sema & 4) == 0) {
            buf.bytes[0] = 4;

            magpiedrv_msif_WriteReg(0x56, 1, buf.bytes);

            if (magpiedrv_msif_WaitPacket() == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
                return MAGPIEDRV_ERROR_TIMEOUT;
            }

            magpiedrv_msif_ReadStatusReg(buf.bytes);

            const u8 status = buf.bytes[0];

            if (status == 0x88) {
                magpiedrv_msif_ReadReg(0x54, 1, buf.bytes);

                sema = buf.bytes[0];
            }
        }

        buf.bytes[0] = ~sema;

        magpiedrv_msif_WriteReg(0x54, 1, buf.bytes);
    }

    sceKernelClearEventFlag(magpiedrv_sema_evtflg, ~MAGPIEDRV_SEMAEVENT_READY);

    return MAGPIEDRV_ERROR_OK;
}

static int WaitHifReady(void) {
    SceUInt timeout = 100000;

    if (sceKernelWaitEventFlag(magpiedrv_sema_evtflg, MAGPIEDRV_SEMAEVENT_HIF_READY, PSP_EVENT_WAITOR, NULL, &timeout) != (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
        return MAGPIEDRV_ERROR_OK;
    }

    magpiedrv_Buffer buf;

    memset(buf.bytes, 0, sizeof(buf));

    buf.bytes[0] = 0x7F;

    magpiedrv_msif_WriteReg(0x54, 1, buf.bytes);

    return -1;
}

int magpiedrv_sema_WaitHif(void) {
    if (hif_ready) {
        return MAGPIEDRV_ERROR_OK;
    }

    if (MAGPIEDRV_RETRY_ON_ERROR(WaitHifReady(), HIF_RETRIES, 0) < 0) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    sceKernelClearEventFlag(magpiedrv_sema_evtflg, ~MAGPIEDRV_SEMAEVENT_HIF_READY);

    return MAGPIEDRV_ERROR_OK;
}
