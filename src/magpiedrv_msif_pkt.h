/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv_msif_pkt.h - Memory Stick I/F packet handlers */

#pragma once

#include <stdbool.h>

#include <pspsdk.h>

int magpiedrv_msif_StartPacket(u8* buf);
int magpiedrv_msif_SyncPacket(const bool no_event);
int magpiedrv_msif_WaitPacket(void);

int magpiedrv_msif_SendPacket(const u16 len, const u32 addr, u8* buf);
int magpiedrv_msif_ReceivePacketCis(const u16 len, const u32 addr, u8* buf);
