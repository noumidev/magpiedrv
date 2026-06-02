/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv.c - Main driver initialization */

#include "magpiedrv.h"

#include <stdbool.h>
#include <string.h>

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

#define SECTION_SIZE        0x1FC
#define SECTION_HEADER_SIZE 0x10

#define MAKE_SECTION_HEADER(ptr, type, arg0, arg1) do { \
    MAGPIEDRV_WRITE_U32(ptr + 0, type);                 \
    MAGPIEDRV_WRITE_U32(ptr + 4, arg0);                 \
    MAGPIEDRV_WRITE_U32(ptr + 8, arg1);                 \
    MAGPIEDRV_WRITE_U32(ptr + 12, GetCrc(12, ptr));     \
} while(0)

static u32 GetCrc(const u32 len, u8* buf) {
    static const u32 CRC32_TABLE[] = {
        0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
        0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
        0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9, 0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75,
        0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
        0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039, 0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5,
        0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
        0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49, 0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95,
        0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,
        0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431b1C, 0x2E003DC5, 0x2AC12072,
        0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16, 0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,
        0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02,
        0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
        0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692,
        0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6, 0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,
        0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2,
        0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,
        0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637, 0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB,
        0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F, 0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53,
        0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B,
        0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF, 0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,
        0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B,
        0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F, 0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
        0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B,
        0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3,
        0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640, 0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C,
        0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8, 0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,
        0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC,
        0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,
        0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0, 0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C,
        0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4,
        0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C,
        0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668, 0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4
    };

    u32 checksum = 0;

    for (u32 i = 0; i < len; i++) {
        checksum = (checksum << 8) ^ CRC32_TABLE[(checksum >> 24) ^ *buf++];
    }

    return __builtin_bswap32(checksum);
}

/* Returns how large a firmware image constructed from a binary will be */
u32 magpiedrv_GetFirmwareSize(const u32 len) {
    const u32 num_code_sections = len / SECTION_SIZE;

    return len + (num_code_sections * (SECTION_HEADER_SIZE + sizeof(u32))) + (2 * SECTION_HEADER_SIZE) + sizeof(u32);
}

/* Constructs a firmware image from a flat binary. `out` needs to be large enough! */
void magpiedrv_MakeFirmware(const u32 len, const u32 addr, u8* buf, u32* out_len, u8* out) {
    *out_len = 0;

    for (u32 i = 0; i < len; i += SECTION_SIZE) {
        const u32 section_size = (SECTION_SIZE < (len - i)) ? SECTION_SIZE : (len - i);

        // Data header
        MAKE_SECTION_HEADER(&out[*out_len], 1, i + addr, section_size + sizeof(u32));

        *out_len += SECTION_HEADER_SIZE;

        memcpy(&out[*out_len], &buf[i], section_size);

        MAGPIEDRV_WRITE_U32(&out[*out_len + section_size], GetCrc(section_size, &out[*out_len]));

        *out_len += section_size + sizeof(u32);
    }

    // Entrypoint header
    MAKE_SECTION_HEADER(&out[*out_len], 4, addr, addr);

    *out_len += SECTION_HEADER_SIZE;
}

#undef SECTION_SIZE
#undef PAYLOAD_HEADER_SIZE
#undef MAKE_SECTION_HEADER

int magpiedrv_UploadFirmware(const u32 len, u8* buf) {
    return magpiedrv_UploadFirmwareImpl(len, buf);
}

void magpiedrv_DEADLOOP(void) {
    MAGPIEDRV_LOCK_INTERRUPTS {
        while (1) {}
    }
}
