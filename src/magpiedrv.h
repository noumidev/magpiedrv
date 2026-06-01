/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv.h - Main driver initialization */

#pragma once

#include <pspsdk.h>
#include <pspdebug.h>

extern int (*_sceKernelRegisterIntrHandler)(int, int, void*, void*, void*);
extern int (*_sceSysconCtrlWlanPower)(int);
extern int (*_sceSysregMsifClkSelect)(int, int);

extern SceUID magpiedrv_evtflg;

typedef enum {
    MAGPIEDRV_ERROR_OK              = 0,
    MAGPIEDRV_ERROR_ALREADY_INITED  = -10,
    MAGPIEDRV_ERROR_NO_KERNEL_FUNCS = -20,
    MAGPIEDRV_ERROR_NO_EVTFLG       = -30,
    MAGPIEDRV_ERROR_NO_INTR_HANDLER = -40,
    MAGPIEDRV_ERROR_TIMEOUT         = -50,
    MAGPIEDRV_ERROR_NO_WAIT         = -60,
    MAGPIEDRV_ERROR_INVALID_REG     = -70,
    MAGPIEDRV_ERROR_CHECKSUM        = -80,
    MAGPIEDRV_ERROR_NO_SEMA         = -90,
    MAGPIEDRV_ERROR_NO_IPL          = -100,
} magpiedrv_Error;

typedef enum {
    MAGPIEDRV_EVENT_PACKET_COMPLETED0   = 1 << 0,
    MAGPIEDRV_EVENT_COMMAND_COMPLETED   = 1 << 1,
    MAGPIEDRV_EVENT_FIFO_READY          = 1 << 2,
    MAGPIEDRV_EVENT_WLAN_SWITCH_FLIPPED = 1 << 3,
    MAGPIEDRV_EVENT_PACKET_COMPLETED1   = 1 << 4,
} magpiedrv_Event;

typedef struct {
    u8 bytes[8];
} magpiedrv_Buffer;

#define print pspDebugScreenPrintf

#define MAGPIEDRV_READ_U16(ptr) ({      \
    u16 _val;                           \
    memcpy(&_val, (ptr), sizeof(_val)); \
    _val;                               \
})

#define MAGPIEDRV_READ_U16_SWAP(ptr) ({ \
    u16 _val;                           \
    memcpy(&_val, (ptr), sizeof(_val)); \
    __builtin_bswap16(_val);            \
})

#define MAGPIEDRV_READ_U32(ptr) ({      \
    u32 _val;                           \
    memcpy(&_val, (ptr), sizeof(_val)); \
    _val;                               \
})

#define MAGPIEDRV_READ_U32_SWAP(ptr) ({ \
    u32 _val;                           \
    memcpy(&_val, (ptr), sizeof(_val)); \
    __builtin_bswap32(_val);            \
})

#define MAGPIEDRV_WRITE_U16(ptr, val) do { \
    const u16 _val = (val);                \
    memcpy((ptr), &_val, sizeof(_val));    \
} while(0)

#define MAGPIEDRV_WRITE_U16_SWAP(ptr, val) do { \
    const u16 _val = __builtin_bswap16(val);    \
    memcpy((ptr), &_val, sizeof(_val));         \
} while(0)

#define MAGPIEDRV_WRITE_U32(ptr, val) do { \
    const u32 _val = (val);                \
    memcpy((ptr), &_val, sizeof(_val));    \
} while(0)

#define MAGPIEDRV_WRITE_U32_SWAP(ptr, val) do { \
    const u32 _val = __builtin_bswap32(val);    \
    memcpy((ptr), &_val, sizeof(_val));         \
} while(0)

#define MAGPIEDRV_LOCK_INTERRUPTS \
    for (u32 _intr_state = magpiedrv_SuspendInterrupts(), _i = 1; _i; magpiedrv_ResumeInterrupts(_intr_state), _i = 0)

#define MAGPIEDRV_RETRY_ON_ERROR(expr, max_retries, delay) ({                \
    int _retval = -1;                                                        \
    for (int _retries = 0; _retries < (max_retries); _retries++) {           \
        _retval = (expr);                                                    \
        if (_retval >= 0) {                                                  \
            break;                                                           \
        }                                                                    \
        if (((delay) > 0) && (_retries < ((max_retries) - 1))) {             \
            sceKernelDelayThread(delay);                                     \
        }                                                                    \
    }                                                                        \
    _retval;                                                                 \
})

void magpiedrv_DEADLOOP(void);

#define MAGPIEDRV_ASSERT_OK(expr) do {            \
    const int _retval = (expr);                   \
    if (_retval < 0) {                            \
        print("\n!!! ASSERTION FAILED !!!\n");    \
        print("%s\n", #expr);                     \
        print("%d\n", _retval);                   \
        print("%s:%d\n", __FILE__, __LINE__);     \
        magpiedrv_DEADLOOP();                     \
    }                                             \
} while(0)

u32 magpiedrv_SuspendInterrupts(void);
void magpiedrv_ResumeInterrupts(const u32 intr_state);

int magpiedrv_Initialize(void);
int magpiedrv_InitializeInterface(void);
int magpiedrv_InitializeMagpie(void);
int magpiedrv_UploadFirmware(const u32 len, u8* buf);
