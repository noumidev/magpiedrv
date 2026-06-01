/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv_cmd.c - Magpie commands */

#include "magpiedrv_cmd.h"

#include <string.h>

#include <pspsdk.h>

#include "magpiedrv.h"
#include "magpiedrv_msif_cmd.h"
#include "magpiedrv_msif_pkt.h"
#include "magpiedrv_sema.h"

int magpiedrv_SendIplBlock(const u16 len, u8* buf) {
    magpiedrv_Buffer cmd_buf;

    memset(cmd_buf.bytes, 0, sizeof(cmd_buf));

    MAGPIEDRV_WRITE_U16_SWAP(cmd_buf.bytes, len);

    int retval;

    if (retval = magpiedrv_msif_WriteReg(0x44, 2, cmd_buf.bytes), retval < 0) {
        return retval;
    }

    magpiedrv_msif_SetDmaFlags(1);

    if (magpiedrv_sema_Wait(200) != MAGPIEDRV_ERROR_OK) {
        magpiedrv_msif_WriteReg(0x44, 2, cmd_buf.bytes);
    }

    retval = magpiedrv_msif_SendPacket(len, 0x100, buf);

    magpiedrv_msif_SetDmaFlags(3);

    return retval;
}
