/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv_msif_init.c - Memory Stick I/F initialization */

#include "magpiedrv_msif_init.h"

#include <pspsdk.h>
#include <pspdebug.h>
#include <pspintrman.h>

#include "magpiedrv.h"
#include "magpiedrv_ioreg.h"
#include "magpiedrv_msif_cmd.h"
#include "LibPspExploit/libpspexploit.h"

#define print pspDebugScreenPrintf

static int InterruptHandler(void) {
    MAGPIEDRV_LOCK_INTERRUPTS {
        magpiedrv_msif_SetControl(MSIF1_CONTROL_INT_CLEAR);

        const u32 status = magpiedrv_msif_GetStatus();

        if ((status & MSIF1_STATUS_READY) != 0) {
            sceKernelSetEventFlag(magpiedrv_evtflg, MAGPIEDRV_EVENT_COMMAND_COMPLETED);
        }

        if ((status & MSIF1_STATUS_INTERRUPT) != 0) {
            // Not exactly sure how this works in sceWlan, needs further investigation
            if ((status & (MSIF1_STATUS_CMD_COMPLETED | MSIF1_STATUS_BUF_REQUEST)) != (MSIF1_STATUS_CMD_COMPLETED | MSIF1_STATUS_BUF_REQUEST)) {
                sceKernelSetEventFlag(magpiedrv_evtflg, MAGPIEDRV_EVENT_PACKET_COMPLETED0);
            } else {
                sceKernelSetEventFlag(magpiedrv_evtflg, MAGPIEDRV_EVENT_PACKET_COMPLETED1);
            }
        }

        if ((status & MSIF1_STATUS_DMA_REQUEST) != 0) {
            sceKernelSetEventFlag(magpiedrv_evtflg, MAGPIEDRV_EVENT_FIFO_READY);
        }
    }

    return -1;
}

int magpiedrv_msif_InstallInterruptHandler(void) {
    return _sceKernelRegisterIntrHandler(
        PSP_WLAN_INT,
        1,
        (void*)KERNELIFY(InterruptHandler),
        NULL,
        NULL
    );
}

/* Initializes the Memory Stick I/F and some internal registers */
int magpiedrv_msif_InitializeInterfaceImpl(void) {
    // Big to-do: figure out what all these internal registers are
    magpiedrv_Buffer buf;

    MAGPIEDRV_ASSERT_OK(magpiedrv_msif_ReadReg(0x22, 1, buf.bytes));

    buf.bytes[0] = 0x7F;

    MAGPIEDRV_ASSERT_OK(magpiedrv_msif_WriteReg(0x10, 1, buf.bytes));

    magpiedrv_msif_SelectClock(1);

    if (magpiedrv_msif_ReadReg(0x25, 1, buf.bytes) == MAGPIEDRV_ERROR_TIMEOUT) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    buf.bytes[0] |= 1;

    if (magpiedrv_msif_WriteReg(0x25, 1, buf.bytes) == MAGPIEDRV_ERROR_TIMEOUT) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    MAGPIEDRV_ASSERT_OK(MAGPIEDRV_RETRY_ON_ERROR(magpiedrv_msif_ReadReg(0x25, 1, buf.bytes) && ((buf.bytes[0] & 1) != 0), MAGPIEDRV_MSIF_STATUS_RETRIES, 0));

    buf.bytes[0] = 1;

    MAGPIEDRV_ASSERT_OK(magpiedrv_msif_WriteReg(0x26, 1, buf.bytes));

    if (magpiedrv_msif_ReadReg(0x27, 1, buf.bytes) != MAGPIEDRV_ERROR_TIMEOUT) {
        if ((buf.bytes[0] & 1) != 0) {
            buf.bytes[0] &= ~1;

            MAGPIEDRV_ASSERT_OK(magpiedrv_msif_WriteReg(0x27, 1, buf.bytes));
        }
    }

    if (magpiedrv_msif_ReadReg(0x23, 1, buf.bytes) != MAGPIEDRV_ERROR_TIMEOUT) {
        if ((buf.bytes[0] & 0x80) != 0) {
            buf.bytes[0] = 1;

            MAGPIEDRV_ASSERT_OK(magpiedrv_msif_WriteReg(0x28, 1, buf.bytes));
        }
    }
    
    return MAGPIEDRV_ERROR_OK;
}

void magpiedrv_msif_PowerInterface(void) {
    magpiedrv_msif_ResetInterface();

    _sceSysconCtrlWlanPower(1);
    sceKernelDelayThread(20000);

    magpiedrv_msif_EnableInterface();
}

void magpiedrv_msif_EnableInterface(void) {
    HW_SYSCTRL_RESETEN |= SYSCTRL_RESETEN_MSIF1;

    HW_SYSCTRL_CLKSEL_LO &= ~SYSCTRL_CLKSEL_LO_MSIF1; // sceWlan does this too
    HW_SYSCTRL_BUSCLKEN  |= SYSCTRL_BUSCLKEN_MSIF1;
    HW_SYSCTRL_CLKEN_LO  |= SYSCTRL_CLKEN_LO_MSIF1;
    HW_SYSCTRL_IOEN      |= SYSCTRL_IOEN_MSIF1;

    HW_SYSCTRL_RESETEN &= ~SYSCTRL_RESETEN_MSIF1;
}

void magpiedrv_msif_ResetInterface(void) {
    HW_SYSCTRL_RESETEN |= SYSCTRL_RESETEN_MSIF1;

    HW_SYSCTRL_IOEN      &= ~SYSCTRL_IOEN_MSIF1;
    HW_SYSCTRL_CLKEN_LO  &= ~SYSCTRL_CLKEN_LO_MSIF1;
    HW_SYSCTRL_BUSCLKEN  &= ~SYSCTRL_BUSCLKEN_MSIF1;
    HW_SYSCTRL_CLKSEL_LO &= ~SYSCTRL_CLKSEL_LO_MSIF1;

    HW_SYSCTRL_RESETEN &= ~SYSCTRL_RESETEN_MSIF1;
}

/* Selects whether the Memory Stick I/F operates in 1- or 4-bit mode */
void magpiedrv_msif_SelectClock(const int clock) {
    MAGPIEDRV_LOCK_INTERRUPTS {
        if (clock == 0) {
            magpiedrv_msif_SetControl(MSIF1_CONTROL_4BIT_MODE | MSIF1_CONTROL_REI);

            _sceSysregMsifClkSelect(1, 0);
        } else {
            magpiedrv_msif_ClearControl(MSIF1_CONTROL_4BIT_MODE | MSIF1_CONTROL_REI);

            _sceSysregMsifClkSelect(1, 1);
        }
    }
}
