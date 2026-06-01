/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv_msif_cmd.c - Memory Stick I/F commands */

#include "magpiedrv_msif_cmd.h"

#include <stdbool.h>
#include <string.h>

#include <pspsdk.h>
#include <pspdebug.h>
#include <pspkernel.h>

#include "magpiedrv.h"
#include "magpiedrv_ioreg.h"

#define BLOCK_SIZE 8

#define print pspDebugScreenPrintf

void magpiedrv_msif_InitializeControl(void) {
    MAGPIEDRV_LOCK_INTERRUPTS {
        magpiedrv_msif_SetControl(MSIF1_CONTROL_INT_ENABLE | MSIF1_CONTROL_DRQ_SL);

        HW_MSIF1_DMACTRL |= 0x10000;
        HW_MSIF1_DMACTRL |= 0x20;
        HW_MSIF1_DMACTRL |= 0x400;
    }
}

u32 magpiedrv_msif_GetControl(void) {
    u32 control;

    MAGPIEDRV_LOCK_INTERRUPTS {
        control = HW_MSIF1_CONTROL;
    }

    return control;
}

void magpiedrv_msif_SetControl(const u32 val) {
    MAGPIEDRV_LOCK_INTERRUPTS {
        HW_MSIF1_CONTROL |= val;
    }
}

void magpiedrv_msif_ClearControl(const u32 val) {
    MAGPIEDRV_LOCK_INTERRUPTS {
        HW_MSIF1_CONTROL &= ~val;
    }
}

int magpiedrv_msif_WaitControl(void) {
    int retries = 0;

    for (; retries < MAGPIEDRV_MSIF_STATUS_RETRIES; retries++) {
        const u32 control = magpiedrv_msif_GetControl();

        // Check if MS I/F is ready to receive data
        if ((control & MSIF1_CONTROL_FIFO_CLEAR) == 0) {
            break;
        }

        sceKernelDelayThread(10000);
    }

    if (retries == MAGPIEDRV_MSIF_STATUS_RETRIES) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    return MAGPIEDRV_ERROR_OK;
}

void magpiedrv_msif_SetDmaFlags(const u32 val) {
    MAGPIEDRV_LOCK_INTERRUPTS {
        if ((val & 4) == 0) {
            if ((val & 1) == 0) {
                HW_MSIF1_DMACTRL |= 0x10000;
                HW_MSIF1_DMACTRL &= ~0x20;
            } else {
                HW_MSIF1_DMACTRL |= 0x10020;
            }

            if ((val & 2) == 0) {
                HW_MSIF1_DMACTRL |= 0x10000;
                HW_MSIF1_DMACTRL &= ~0x400;
            } else {
                HW_MSIF1_DMACTRL |= 0x10400;
            }
        }
    }
}

u32 magpiedrv_msif_GetStatus(void) {
    u32 status;

    MAGPIEDRV_LOCK_INTERRUPTS {
        status = HW_MSIF1_STATUS;
    }

    return status;
}

void magpiedrv_msif_WaitFifo(void) {
    const u32 status = magpiedrv_msif_GetStatus();

    SceUInt timeout = 1000;

    if ((status & MSIF1_STATUS_DMA_REQUEST) == 0) {
        sceKernelWaitEventFlag(magpiedrv_evtflg, MAGPIEDRV_EVENT_FIFO_READY, PSP_EVENT_WAITOR, NULL, &timeout);
        sceKernelClearEventFlag(magpiedrv_evtflg, ~MAGPIEDRV_EVENT_FIFO_READY);
    }
}

void magpiedrv_msif_SetCommand(const u32 command, const u32 len, const bool is_read) {
    if (is_read) {
        magpiedrv_msif_ClearControl(MSIF1_CONTROL_FIFO_WRITE);
    } else {
        magpiedrv_msif_SetControl(MSIF1_CONTROL_FIFO_WRITE);
    }

    MAGPIEDRV_LOCK_INTERRUPTS {
        HW_MSIF1_COMMAND = (command << 12) | len;
    }
}

int magpiedrv_msif_WaitCommand(const bool no_event) {
    int retval;

    if (no_event) {
        // Wait for READY flag without event flag
        for (int retries = 0; retries < MAGPIEDRV_MSIF_STATUS_RETRIES; retries++) {
            const u32 status = magpiedrv_msif_GetStatus();

            if ((status & MSIF1_STATUS_READY) != 0) {
                return MAGPIEDRV_ERROR_OK;
            }

            sceKernelDelayThread(10000);
        }

        retval = MAGPIEDRV_ERROR_TIMEOUT;
    } else {
        const u32 status = magpiedrv_msif_GetStatus();

        SceUInt timeout = 1000;

        retval = MAGPIEDRV_ERROR_NO_WAIT;

        if ((status & MSIF1_STATUS_READY) == 0) {
            retval = sceKernelWaitEventFlag(magpiedrv_evtflg, MAGPIEDRV_EVENT_COMMAND_COMPLETED, PSP_EVENT_WAITOR, NULL, &timeout);

            sceKernelClearEventFlag(magpiedrv_evtflg, ~MAGPIEDRV_EVENT_COMMAND_COMPLETED);
        }

        if (retval == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
            retval = MAGPIEDRV_ERROR_TIMEOUT;
        }
    }

    return retval;
}

void magpiedrv_msif_SetData(const u32 data) {
    magpiedrv_msif_WaitFifo();

    MAGPIEDRV_LOCK_INTERRUPTS {
        HW_MSIF1_DATA = data;
    }
}

u32 magpiedrv_msif_GetData(void) {
    magpiedrv_msif_WaitFifo();

    u32 data;

    MAGPIEDRV_LOCK_INTERRUPTS {
        data = HW_MSIF1_DATA;
    }

    return data;
}

/* Selects an internal Memory Stick I/F register */
int magpiedrv_msif_SelectReg(const u32 reg, const u32 len, const bool is_read) {
    magpiedrv_msif_SetControl(MSIF1_CONTROL_INT_CLEAR | MSIF1_CONTROL_FIFO_CLEAR);

    if (magpiedrv_msif_WaitControl() < 0) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    magpiedrv_msif_SetCommand(MAGPIEDRV_MSIF_COMMAND_SELECT_REG, 4, false);

    magpiedrv_Buffer buf;

    memset(buf.bytes, 0, sizeof(buf));

    if (is_read) {
        buf.bytes[0] = reg;
        buf.bytes[1] = len;
    } else {
        buf.bytes[2] = reg;
        buf.bytes[3] = len;
    }

    magpiedrv_msif_SetData(MAGPIEDRV_READ_U32(buf.bytes));
    magpiedrv_msif_SetData(0);

    if (magpiedrv_msif_WaitCommand(false) == MAGPIEDRV_ERROR_TIMEOUT) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    if ((magpiedrv_msif_GetStatus() & (MSIF1_STATUS_CRC_ERROR | MSIF1_STATUS_TIMEOUT)) == 0) {
        return MAGPIEDRV_ERROR_OK;
    }

    return -1;
}

static int ReadRegImpl(const u32 len, u8* buf) {
    magpiedrv_msif_SetControl(MSIF1_CONTROL_INT_CLEAR | MSIF1_CONTROL_FIFO_CLEAR);

    if (magpiedrv_msif_WaitControl() < 0) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    magpiedrv_msif_SetCommand(MAGPIEDRV_MSIF_COMMAND_READ_REG, len, true);

    MAGPIEDRV_WRITE_U32(&buf[0], magpiedrv_msif_GetData());
    MAGPIEDRV_WRITE_U32(&buf[4], magpiedrv_msif_GetData());

    magpiedrv_msif_SetControl(MSIF1_CONTROL_INT_CLEAR);

    if (magpiedrv_msif_WaitCommand(false) == MAGPIEDRV_ERROR_TIMEOUT) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    if ((magpiedrv_msif_GetStatus() & (MSIF1_STATUS_CRC_ERROR | MSIF1_STATUS_TIMEOUT)) == 0) {
        return MAGPIEDRV_ERROR_OK;
    }

    return -1;
}

/* Selects and reads an internal Memory Stick I/F register */
int magpiedrv_msif_ReadReg(const u32 reg, const u32 len, u8* buf) {
    if (reg >= 0x100) {
        return MAGPIEDRV_ERROR_INVALID_REG;
    }

    if (MAGPIEDRV_RETRY_ON_ERROR(magpiedrv_msif_SelectReg(reg, len, true), MAGPIEDRV_MSIF_REG_RETRIES, 0)) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    if (MAGPIEDRV_RETRY_ON_ERROR(ReadRegImpl(len, buf), MAGPIEDRV_MSIF_REG_RETRIES, 0)) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    return MAGPIEDRV_ERROR_OK;
}

int magpiedrv_msif_ReadStatusReg(u8* buf) {
    for (int retries = 0; retries < MAGPIEDRV_MSIF_REG_RETRIES; retries++) {
        magpiedrv_msif_SetControl(MSIF1_CONTROL_INT_CLEAR | MSIF1_CONTROL_FIFO_CLEAR);

        if (magpiedrv_msif_WaitControl() == MAGPIEDRV_ERROR_TIMEOUT) {
            return MAGPIEDRV_ERROR_TIMEOUT;
        }

        magpiedrv_msif_SetCommand(MAGPIEDRV_MSIF_COMMAND_READ_STATUS, 1, true);

        u32 data = magpiedrv_msif_GetData();

        memcpy(&buf[0], &data, sizeof(data));

        data = magpiedrv_msif_GetData();

        memcpy(&buf[4], &data, sizeof(data));

        if (magpiedrv_msif_WaitCommand(false) == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
            return MAGPIEDRV_ERROR_TIMEOUT;
        }

        if ((magpiedrv_msif_GetStatus() & (MSIF1_STATUS_CRC_ERROR | MSIF1_STATUS_TIMEOUT)) == 0) {
            return MAGPIEDRV_ERROR_OK;
        }
    }

    return -1;
}

static int WriteRegImpl(const u32 len, u8* buf) {
    magpiedrv_msif_SetControl(MSIF1_CONTROL_INT_CLEAR | MSIF1_CONTROL_FIFO_CLEAR);

    if (magpiedrv_msif_WaitControl() < 0) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    magpiedrv_msif_SetCommand(MAGPIEDRV_MSIF_COMMAND_WRITE_REG, len, false);

    magpiedrv_msif_SetData(MAGPIEDRV_READ_U32(&buf[0]));
    magpiedrv_msif_SetData(MAGPIEDRV_READ_U32(&buf[4]));

    magpiedrv_msif_SetControl(MSIF1_CONTROL_INT_CLEAR);

    if (magpiedrv_msif_WaitCommand(false) == MAGPIEDRV_ERROR_TIMEOUT) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    if ((magpiedrv_msif_GetStatus() & (MSIF1_STATUS_CRC_ERROR | MSIF1_STATUS_TIMEOUT)) == 0) {
        return MAGPIEDRV_ERROR_OK;
    }

    return -1;
}

/* Selects and writes an internal Memory Stick I/F register */
int magpiedrv_msif_WriteReg(const u32 reg, const u32 len, u8* buf) {
    if (reg >= 0x100) {
        return MAGPIEDRV_ERROR_INVALID_REG;
    }

    if (MAGPIEDRV_RETRY_ON_ERROR(magpiedrv_msif_SelectReg(reg, len, false), MAGPIEDRV_MSIF_REG_RETRIES, 0)) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    if (MAGPIEDRV_RETRY_ON_ERROR(WriteRegImpl(len, buf), MAGPIEDRV_MSIF_REG_RETRIES, 0)) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    return MAGPIEDRV_ERROR_OK;
}

static int ReadBlockImplCpu(const u32 len, u8* buf) {
    magpiedrv_msif_SetControl(MSIF1_CONTROL_INT_CLEAR | MSIF1_CONTROL_FIFO_CLEAR);

    if (magpiedrv_msif_WaitControl() < 0) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    magpiedrv_msif_SetCommand(MAGPIEDRV_MSIF_COMMAND_READ_BLOCK_CPU, len, false);

    // MS I/F blocks are always transferred 8 bytes at a time
    u32 num_blocks = len >> 3;

    if ((len & 7) != 0) {
        num_blocks++;
    }

    for (; num_blocks > 0; num_blocks--) {
        MAGPIEDRV_WRITE_U32(&buf[0], magpiedrv_msif_GetData());
        MAGPIEDRV_WRITE_U32(&buf[4], magpiedrv_msif_GetData());

        buf += BLOCK_SIZE;
    }

    return MAGPIEDRV_ERROR_OK;
}

int magpiedrv_msif_ReadBlock(const u32 len, u8* buf) {
    if ((len == 0) || ((len & 8) != 0)) {
        return -1;
    }

    // sceWlan sometimes uses an MS I/F DMA feature which magpiedrv
    // doesn't implement yet

    if (ReadBlockImplCpu(len, buf) == MAGPIEDRV_ERROR_TIMEOUT) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    // Byteswap the whole buffer
    for (u32 i = 0; i < len; i += sizeof(u32)) {
        MAGPIEDRV_WRITE_U32_SWAP(&buf[i], MAGPIEDRV_READ_U32(&buf[i]));
    }

    return len;
}

static int WriteBlockImplCpu(const u32 len, u8* buf) {
    magpiedrv_msif_SetControl(MSIF1_CONTROL_INT_CLEAR | MSIF1_CONTROL_FIFO_CLEAR);

    if (magpiedrv_msif_WaitControl() < 0) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    magpiedrv_msif_SetCommand(MAGPIEDRV_MSIF_COMMAND_WRITE_BLOCK_CPU, len, false);

    // MS I/F blocks are always transferred 8 bytes at a time.
    // sceWlan always transfers an empty block at the end which is absolutely
    // necessary for the transfer to work
    u32 num_blocks = (len >> 3) + 1;

    for (; num_blocks > 0; num_blocks--) {
        magpiedrv_msif_SetData(MAGPIEDRV_READ_U32(&buf[0]));
        magpiedrv_msif_SetData(MAGPIEDRV_READ_U32(&buf[4]));

        buf += BLOCK_SIZE;
    }

    return MAGPIEDRV_ERROR_OK;
}

int magpiedrv_msif_WriteBlock(const u32 len, u8* buf) {
    if ((len == 0) || (len > 0x200)) {
        return -1;
    }

    u8 block_buf[0x208];

    memset(block_buf, 0, sizeof(block_buf));
    memcpy(block_buf, buf, len);

    // Byteswap the whole buffer
    for (u32 i = 0; i < len; i += sizeof(u32)) {
        MAGPIEDRV_WRITE_U32_SWAP(&block_buf[i], MAGPIEDRV_READ_U32(&block_buf[i]));
    }

    // sceWlan sometimes uses an MS I/F DMA feature which magpiedrv
    // doesn't implement yet

    if (WriteBlockImplCpu(len, block_buf) == MAGPIEDRV_ERROR_TIMEOUT) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    return len;
}
