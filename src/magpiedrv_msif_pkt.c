/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv_msif_pkt.c - Memory Stick I/F packet handlers */

#include "magpiedrv_msif_pkt.h"

#include <string.h>

#include <pspsdk.h>
#include <pspkernel.h>

#include "magpiedrv.h"
#include "magpiedrv_ioreg.h"
#include "magpiedrv_msif_cmd.h"

typedef enum {
    MAGPIEDRV_PACKETSTATE_UNIMPLEMENTED  = -1,
    MAGPIEDRV_PACKETSTATE_SYNC_PACKET    = 1,
    MAGPIEDRV_PACKETSTATE_CHECK_STATUS   = 2,
    MAGPIEDRV_PACKETSTATE_TRANSFER_BLOCK = 3,
    MAGPIEDRV_PACKETSTATE_END            = 5,
} magpiedrv_PacketState;

int magpiedrv_msif_StartPacket(u8* buf) {
    magpiedrv_msif_SetControl(MSIF1_CONTROL_INT_CLEAR | MSIF1_CONTROL_FIFO_CLEAR);

    if (magpiedrv_msif_WaitControl() == MAGPIEDRV_ERROR_TIMEOUT) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    if (magpiedrv_msif_WaitCommand(true) == MAGPIEDRV_ERROR_TIMEOUT) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    magpiedrv_msif_SetCommand(MAGPIEDRV_MSIF_COMMAND_START_PKT, 7, false);

    magpiedrv_msif_SetData(MAGPIEDRV_READ_U32(&buf[0]));
    magpiedrv_msif_SetData(MAGPIEDRV_READ_U32(&buf[4]));

    return MAGPIEDRV_ERROR_OK;
}

/* `no_event` returns a state, not an error code */
int magpiedrv_msif_SyncPacket(const bool no_event) {
    if (no_event) {
        if (magpiedrv_msif_WaitPacket() == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
            return MAGPIEDRV_PACKETSTATE_END;
        }

        const u32 status = magpiedrv_msif_GetStatus();

        if ((status & (MSIF1_STATUS_TIMEOUT | MSIF1_STATUS_BAD_CMD)) != 0) {
            return MAGPIEDRV_PACKETSTATE_END;
        }

        if ((status & (MSIF1_STATUS_CMD_COMPLETED | MSIF1_STATUS_BUF_REQUEST)) == MSIF1_STATUS_BUF_REQUEST) {
            return MAGPIEDRV_PACKETSTATE_TRANSFER_BLOCK;
        }

        return MAGPIEDRV_PACKETSTATE_CHECK_STATUS;
    } else {
        int retval;

        const u32 status = magpiedrv_msif_GetStatus();

        if ((status & (MSIF1_STATUS_CMD_COMPLETED | MSIF1_STATUS_BUF_REQUEST)) != (MSIF1_STATUS_CMD_COMPLETED | MSIF1_STATUS_BUF_REQUEST)) {
            SceUInt timeout = 50000;

            retval = sceKernelWaitEventFlag(magpiedrv_evtflg, MAGPIEDRV_EVENT_PACKET_COMPLETED1, PSP_EVENT_WAITOR, NULL, &timeout);

            sceKernelClearEventFlag(magpiedrv_evtflg, ~MAGPIEDRV_EVENT_PACKET_COMPLETED1);

            if (retval != MAGPIEDRV_ERROR_TIMEOUT) {
                retval = MAGPIEDRV_ERROR_OK;
            }
        }

        return retval;
    }
}

int magpiedrv_msif_WaitPacket(void) {
    SceUInt timeout = 1000;

    int retval = MAGPIEDRV_ERROR_NO_WAIT;

    if ((magpiedrv_msif_GetStatus() & MSIF1_STATUS_INTERRUPT) == 0) {
        retval = sceKernelWaitEventFlag(magpiedrv_evtflg, MAGPIEDRV_EVENT_PACKET_COMPLETED0, PSP_EVENT_WAITOR, NULL, &timeout);

        sceKernelClearEventFlag(magpiedrv_evtflg, ~MAGPIEDRV_EVENT_PACKET_COMPLETED0);
    }

    return retval;
}

int magpiedrv_msif_SendPacket(const u16 len, const u32 addr, u8* buf) {
    magpiedrv_Buffer packet;

    memset(packet.bytes, 0, sizeof(packet));

    packet.bytes[0] = 0xB4;

    MAGPIEDRV_WRITE_U16_SWAP(&packet.bytes[1], len);
    MAGPIEDRV_WRITE_U32_SWAP(&packet.bytes[3], addr);

    if (magpiedrv_msif_StartPacket(packet.bytes) == MAGPIEDRV_ERROR_TIMEOUT) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    u32 buf_idx = 0;
    u32 block_len = len;

    // 0x200 is the maximum size per MS I/F block transfer
    if (block_len > 0x200) {
        block_len = 0x200;
    }

    magpiedrv_PacketState state = MAGPIEDRV_PACKETSTATE_SYNC_PACKET;

    int retval = MAGPIEDRV_ERROR_OK;

    while (state != MAGPIEDRV_PACKETSTATE_END) {
        switch (state) {
            case MAGPIEDRV_PACKETSTATE_SYNC_PACKET:
                state = magpiedrv_msif_SyncPacket(true);
                break;
            case MAGPIEDRV_PACKETSTATE_CHECK_STATUS:
                {
                    magpiedrv_Buffer status_buf;

                    if (magpiedrv_msif_ReadStatusReg(status_buf.bytes) < 0) {
                        state = MAGPIEDRV_PACKETSTATE_END;
                        break;
                    }

                    const u8 status = status_buf.bytes[0];

                    if ((status & 1) != 0) {
                        state = MAGPIEDRV_PACKETSTATE_END;
                    } else if ((status & 0x10) != 0) {
                        state = MAGPIEDRV_PACKETSTATE_TRANSFER_BLOCK;
                    } else if ((status & 0x60) != 0x60) {
                        state = MAGPIEDRV_PACKETSTATE_END;
                    }

                    break;
                }
            case MAGPIEDRV_PACKETSTATE_TRANSFER_BLOCK:
                if (magpiedrv_msif_WriteBlock(block_len, &buf[buf_idx]) < 0) {
                    state  = MAGPIEDRV_PACKETSTATE_END;
                    retval = -1;
                } else {
                    state = MAGPIEDRV_PACKETSTATE_SYNC_PACKET;

                    buf_idx += block_len;

                    if (((int)len - (int)buf_idx) < (int)block_len) {
                        block_len = len - buf_idx;
                    }
                }
                break;
            default:
                state  = MAGPIEDRV_PACKETSTATE_END;
                retval = -1;
                break;
        }
    }

    return retval;
}

/* Reads a block of memory from the CIS base (ARM-side register 0x80000048) + addr */
int magpiedrv_msif_ReceivePacketCis(const u16 len, const u32 addr, u8* buf) {
    // Note: addresses 0x100-0x1FF are mirrors of 0x000-0x0FF!!
    magpiedrv_Buffer packet;

    memset(packet.bytes, 0, sizeof(packet));

    packet.bytes[0] = 0xB2;

    MAGPIEDRV_WRITE_U16_SWAP(&packet.bytes[1], len);
    MAGPIEDRV_WRITE_U32_SWAP(&packet.bytes[3], addr);

    if (magpiedrv_msif_StartPacket(packet.bytes) == MAGPIEDRV_ERROR_TIMEOUT) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    magpiedrv_PacketState state = MAGPIEDRV_PACKETSTATE_SYNC_PACKET;

    int retval = MAGPIEDRV_ERROR_OK;

    while (state != MAGPIEDRV_PACKETSTATE_END) {
        switch (state) {
            case MAGPIEDRV_PACKETSTATE_SYNC_PACKET:
                state = magpiedrv_msif_SyncPacket(true);
                break;
            case MAGPIEDRV_PACKETSTATE_CHECK_STATUS:
                {
                    magpiedrv_Buffer status_buf;

                    if (magpiedrv_msif_ReadStatusReg(status_buf.bytes) < 0) {
                        state = MAGPIEDRV_PACKETSTATE_END;
                        break;
                    }

                    const u8 status = status_buf.bytes[0];

                    if ((status & 1) != 0) {
                        state = MAGPIEDRV_PACKETSTATE_END;
                    } else if ((status & 0x10) != 0) {
                        state = MAGPIEDRV_PACKETSTATE_TRANSFER_BLOCK;
                    } else if ((status & 0x60) != 0x60) {
                        state = MAGPIEDRV_PACKETSTATE_END;
                    }

                    break;
                }
            case MAGPIEDRV_PACKETSTATE_TRANSFER_BLOCK:
                if (magpiedrv_msif_ReadBlock(len, buf) < 0) {
                    state  = MAGPIEDRV_PACKETSTATE_END;
                    retval = -1;
                } else {
                    state = MAGPIEDRV_PACKETSTATE_SYNC_PACKET;
                }
                break;
            default:
                state  = MAGPIEDRV_PACKETSTATE_END;
                retval = -1;
                break;
        }
    }

    return retval;
}
