/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv_init.c - Magpie initialization */

#include "magpiedrv_init.h"

#include <string.h>

#include <pspsdk.h>
#include <pspdebug.h>

#include "magpiedrv.h"
#include "magpiedrv_cmd.h"
#include "magpiedrv_ipl.h"
#include "magpiedrv_msif_cmd.h"
#include "magpiedrv_msif_pkt.h"
#include "magpiedrv_sema.h"

#define print pspDebugScreenPrintf

static int UploadIpl(void) {
    u8 buf[0x200];

    u32 ipl_idx = 0;
    u32 len = sizeof(MAGPIEDRV_IPL);

    while (len > 0) {
        u32 block_len = len;

        if (block_len > sizeof(buf)) {
            block_len = sizeof(buf);
        }

        memcpy(buf, &MAGPIEDRV_IPL[ipl_idx], block_len);

        int retval;

        if (retval = magpiedrv_SendIplBlock(block_len, buf), retval < 0) {
            return -1;
        }

        len -= block_len;
        ipl_idx += block_len;

        sceKernelDelayThread(20000);
    }

    memset(buf, 0, 8);

    magpiedrv_msif_WriteReg(0x44, 2, buf);

    return MAGPIEDRV_ERROR_OK;
}

int magpiedrv_InitializeMagpieImpl() {
    magpiedrv_Buffer buf;

    memset(buf.bytes, 0, sizeof(buf));

    buf.bytes[0] = 0xFF;

    MAGPIEDRV_ASSERT_OK(magpiedrv_msif_WriteReg(0x5A, 1, buf.bytes));
    MAGPIEDRV_ASSERT_OK(magpiedrv_msif_WriteReg(0x5C, 1, buf.bytes));
    MAGPIEDRV_ASSERT_OK(magpiedrv_msif_WriteReg(0x5E, 1, buf.bytes));

    if (magpiedrv_sema_Initialize() < 0) {
        return MAGPIEDRV_ERROR_NO_SEMA;
    }

    // Wait for Magpie host I/F handshake
    if (magpiedrv_sema_WaitHif() < 0) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    MAGPIEDRV_ASSERT_OK(magpiedrv_msif_ReadReg(0x22, 1, buf.bytes));

    if (buf.bytes[0] != 0x24) {
        return -1;
    }

    if (UploadIpl() < 0) {
        return MAGPIEDRV_ERROR_NO_IPL;
    }

    if (magpiedrv_sema_Wait(10) < 0) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    return MAGPIEDRV_ERROR_OK;
}

static int GetPayloadLength(u16* len, int retries, const u32 timeout) {
    const bool no_timeout = retries <= 1;

    magpiedrv_Buffer buf;

    for (; retries > 0; retries--) {
        sceKernelDelayThread(timeout);

        if (magpiedrv_msif_ReadReg(0x4C, 2, buf.bytes) < 0) {
            return MAGPIEDRV_ERROR_TIMEOUT;
        }

        *len = MAGPIEDRV_READ_U16_SWAP(buf.bytes);

        if (*len != 0) {
            if ((*len & 1) != 0) {
                return MAGPIEDRV_ERROR_CHECKSUM;
            }

            *len &= ~1;

            break;
        }
    }

    if (!no_timeout && (retries <= 0)) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    return MAGPIEDRV_ERROR_OK;
}

static int SendFirmwareSection(const u32 len, u8* buf) {
    if (magpiedrv_SendIplBlock(len, buf) < 0) {
        return -1;
    }

    u16 block_len;

    int retval;

    if (retval = GetPayloadLength(&block_len, 1, 0x4B0), retval < 0) {
        return retval;
    }

    return block_len;
}

static int UploadFirmwarePayload(const u32 len, u8* buf) {
    if ((len == 0) || (buf == NULL)) {
        return -1;
    }

    u16 block_len;

    if (GetPayloadLength(&block_len, 10000, 20000) < 0) {
        return -1;
    }

    if (block_len != 0x10) {
        return -1;
    }

    u8 block_buf[0x200];
    u32 payload_ptr = 0;

    while (payload_ptr < len) {
        memcpy(block_buf, &buf[payload_ptr], block_len);

        int retval;

        // Possibly retry section transfer upon checksum errors? The last
        // section might always return a checksum error?
        if (retval = SendFirmwareSection(block_len, block_buf), (retval < 0) && (retval != MAGPIEDRV_ERROR_CHECKSUM)) {
            return retval;
        }

        payload_ptr += block_len;
        block_len = retval;
    }

    return MAGPIEDRV_ERROR_OK;
}

int magpiedrv_UploadFirmwareImpl(const u32 len, u8* firm_buf) {
    // sceWlan would use MS I/F DMA to upload the firmware, but
    // this is not necessary for now
    magpiedrv_msif_SetDmaFlags(1);

    // Send empty 16-byte packet
    u8 buf[0x10];

    memset(buf, 0, sizeof(buf));

    if (magpiedrv_msif_SendPacket(0x10, 0x100, buf) < 0) {
        return -1;
    }

    buf[0] = 8;

    magpiedrv_msif_WriteReg(0x56, 1, buf);
    magpiedrv_msif_SetDmaFlags(3);

    return UploadFirmwarePayload(len, firm_buf);
}
