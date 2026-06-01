/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv.c - Main driver initialization */

#include "magpiedrv.h"

#include <stdbool.h>

#include <pspsdk.h>
#include <pspintrman.h>

#include "magpiedrv_init.h"
#include "magpiedrv_msif_cmd.h"
#include "magpiedrv_msif_init.h"
#include "LibPspExploit/libpspexploit.h"

// Kernel funcs
static int (*_sceKernelDisableIntr)(int);
static int (*_sceKernelEnableIntr)(int);
static int (*_sceKernelCpuResumeIntrWithSync)(u32);
static int (*_sceKernelCpuSuspendIntr)(void);
static int (*_sceSysconSetWlanSwitchCallback)(void (*)(int, void *), void *);
int (*_sceKernelRegisterIntrHandler)(int, int, void*, void*, void*);
int (*_sceSysconCtrlWlanPower)(int);
int (*_sceSysregMsifClkSelect)(int, int);

SceUID magpiedrv_evtflg;

static void WlanSwitchCallback(int wlan_switch, void* argp) {
    (void)argp;

    if (wlan_switch != 0) {
        sceKernelSetEventFlag(magpiedrv_evtflg, MAGPIEDRV_EVENT_WLAN_SWITCH_FLIPPED);
    }
}

static int WaitWlanSwitch(void) {
    const int retval = sceKernelWaitEventFlag(magpiedrv_evtflg, MAGPIEDRV_EVENT_WLAN_SWITCH_FLIPPED, PSP_EVENT_WAITOR, NULL, NULL);

    sceKernelClearEventFlag(magpiedrv_evtflg, ~MAGPIEDRV_EVENT_WLAN_SWITCH_FLIPPED);

    return retval;
}

u32 magpiedrv_SuspendInterrupts(void) {
    return _sceKernelCpuSuspendIntr();
}

void magpiedrv_ResumeInterrupts(const u32 intr_state) {
    _sceKernelCpuResumeIntrWithSync(intr_state);
}

/* Initializes the most important driver functions */
int magpiedrv_Initialize(void) {
    // ONLY CALL THIS AFTER PERFORMING THE KERNEL EXPLOIT!

    static bool IS_INITIALIZED = false;

    if (IS_INITIALIZED) {
        return MAGPIEDRV_ERROR_ALREADY_INITED;
    }

    const char* mod_sysreg;

    if (pspXploitFindTextAddrByName("sceLowIO_Driver") == 0) {
        mod_sysreg = "sceSYSREG_Driver";
    } else {
        mod_sysreg = "sceLowIO_Driver";
    }

    // Get kernel functions
    _sceKernelDisableIntr           = (int (*)(int))pspXploitFindFunction("sceInterruptManager", "InterruptManagerForKernel", 0xD774BA45);
    _sceKernelEnableIntr            = (int (*)(int))pspXploitFindFunction("sceInterruptManager", "InterruptManagerForKernel", 0x4D6E7305);
    _sceKernelRegisterIntrHandler   = (int (*)(int, int, void*, void*, void*))pspXploitFindFunction("sceInterruptManager", "InterruptManagerForKernel", 0x58DD8978);
    _sceKernelCpuResumeIntrWithSync = (int (*)(u32))pspXploitFindFunction("sceInterruptManager", "InterruptManagerForKernel", 0x3B84732D);
    _sceKernelCpuSuspendIntr        = (int (*)(void))pspXploitFindFunction("sceInterruptManager", "InterruptManagerForKernel", 0x092968F4);
    _sceSysconCtrlWlanPower         = (int (*)(int))pspXploitFindFunction("sceSYSCON_Driver", "sceSyscon_driver", 0x48448373);
    _sceSysconSetWlanSwitchCallback = (int (*)(void (*)(int, void*), void*))pspXploitFindFunction("sceSYSCON_Driver", "sceSyscon_driver", 0x50446BE5);
    _sceSysregMsifClkSelect         = (int (*)(int, int))pspXploitFindFunction(mod_sysreg, "sceSysreg_driver", 0x48124AFE);

    if (
        (_sceKernelDisableIntr           == NULL) ||
        (_sceKernelEnableIntr            == NULL) ||
        (_sceKernelRegisterIntrHandler   == NULL) ||
        (_sceKernelCpuResumeIntrWithSync == NULL) ||
        (_sceKernelCpuSuspendIntr        == NULL) ||
        (_sceSysconCtrlWlanPower         == NULL) ||
        (_sceSysconSetWlanSwitchCallback == NULL) ||
        (_sceSysregMsifClkSelect         == NULL)
    ) {
        return MAGPIEDRV_ERROR_NO_KERNEL_FUNCS;
    }

    _sceKernelDisableIntr(PSP_WLAN_INT);

    if (magpiedrv_evtflg = sceKernelCreateEventFlag("magpiedrv_evtflg", 0x201, 0, NULL), magpiedrv_evtflg < 0) {
        return MAGPIEDRV_ERROR_NO_EVTFLG;
    }

    // Install WLAN switch callback and wait for flip
    if (_sceSysconSetWlanSwitchCallback(WlanSwitchCallback, NULL) < 0) {
        return -1;
    }

    if (WaitWlanSwitch() == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
        return MAGPIEDRV_ERROR_TIMEOUT;
    }

    magpiedrv_msif_PowerInterface();

    if (magpiedrv_msif_InstallInterruptHandler() < 0) {
        return MAGPIEDRV_ERROR_NO_INTR_HANDLER;
    }

    _sceKernelEnableIntr(PSP_WLAN_INT);

    magpiedrv_msif_InitializeControl();

    IS_INITIALIZED = true;

    return MAGPIEDRV_ERROR_OK;
}

int magpiedrv_InitializeInterface(void) {
    return magpiedrv_msif_InitializeInterfaceImpl();
}

int magpiedrv_InitializeMagpie(void) {
    return magpiedrv_InitializeMagpieImpl();
}

int magpiedrv_UploadFirmware(const u32 len, u8* buf) {
    return magpiedrv_UploadFirmwareImpl(len, buf);
}

void magpiedrv_DEADLOOP(void) {
    MAGPIEDRV_LOCK_INTERRUPTS {
        while (1) {}
    }
}
