/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* example.c - Usage example */

#include <string.h>

#include <pspsdk.h>
#include <pspdebug.h>
#include <pspkernel.h>

#include "magpiedrv.h"
#include "magpiedrv_msif_cmd.h"
#include "magpiedrv_msif_pkt.h"
#include "LibPspExploit/libpspexploit.h"

#include "payload.h"

#define print pspDebugScreenPrintf

PSP_MODULE_INFO("magpiedrv example", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_MAIN_THREAD_STACK_SIZE_KB(32);

static int exit_callback(const int arg1, const int arg2, void* common) {
    (void)arg1;
    (void)arg2;
    (void)common;

    sceKernelExitGame();

    return 0;
}

static int callback_thread_main(const SceSize args, void* argp) {
    (void)args;
    (void)argp;

    const int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);

    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();

    return 0;
}

static int setup_callbacks(void) {
    const int tid = sceKernelCreateThread(
        "Callback Thread",
        callback_thread_main,
        0x11,
        0xFA0,
        THREAD_ATTR_USER,
        NULL
    );

    if (tid >= 0) {
        sceKernelStartThread(tid, 0, NULL);
    }

    return tid;
}

static u32 get_hif_comm(void) {
    magpiedrv_Buffer buf;

    // MS I/F register 0x4A maps to ARM-side register 0x8000000C (check payload.h)
    magpiedrv_msif_ReadReg(0x4A, 4, buf.bytes);

    return MAGPIEDRV_READ_U32_SWAP(buf.bytes);
}

static void kmain(void) {
    const int k1 = pspSdkSetK1(0);
    const int user_level = pspXploitSetUserLevel(8);

    pspXploitRepairKernel();

    print("\n--- magpiedrv test ---\n\n");

    int retval;

    if (retval = magpiedrv_Initialize(), retval < 0) {
        print("Failed to init magpiedrv: %d\n", retval);
        goto KMAIN_END;
    }

    print("magpiedrv init successful\n");

    if (retval = magpiedrv_InitializeInterface(), retval < 0) {
        print("Failed to init MS I/F: %d\n", retval);
        goto KMAIN_END;
    }

    print("MS I/F init successful\n");

    if (retval = magpiedrv_InitializeMagpie(), retval < 0) {
        print("Failed to init Magpie: %d\n", retval);
        goto KMAIN_END;
    }

    print("Magpie init successful\n");

    // Make firmware image
    u8 firm_buf[0x1000];
    u32 firm_len;

    magpiedrv_MakeFirmware(sizeof(PAYLOAD), 0, PAYLOAD, &firm_len, firm_buf);

    MAGPIEDRV_ASSERT_OK(firm_len == magpiedrv_GetFirmwareSize(sizeof(PAYLOAD)));

    if (retval = magpiedrv_UploadFirmware(firm_len, firm_buf), retval < 0) {
        print("Failed to upload firmware: %d\n", retval);
        goto KMAIN_END;
    }

    sceKernelDelayThread(1000);

    // Should print "04030201"
    print("%08lX\n", get_hif_comm());

    char msg[32];

    magpiedrv_msif_ReceivePacketCis(sizeof(msg), 0, (u8*)msg);

    // Should print "Hello, Magpie!"
    print("%s", msg);

KMAIN_END:
    pspXploitSetUserLevel(user_level);
    pspSdkSetK1(k1);
}

int main(void) {
    pspDebugScreenInit();
    pspDebugScreenClear();

    setup_callbacks();

    // Attempt kernel exploit
    if (pspXploitInitKernelExploit() != 0) {
        print("Failed to init kernel exploit\n");
        goto MAIN_END;
    }

    if (pspXploitDoKernelExploit() != 0) {
        print("Failed to exploit kernel\n");
        goto MAIN_END;
    }

    pspXploitExecuteKernel((u32)kmain);

MAIN_END:
    sceKernelSleepThread();

    return 0;
}
