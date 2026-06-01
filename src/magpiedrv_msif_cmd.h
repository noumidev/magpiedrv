/*
 * magpiedrv is an sceWlan reimplementation for the PlayStation Portable.
 * Copyright (c) 2025-2026 noumidev
 */

/* magpiedrv_msif_cmd.h - Memory Stick I/F commands */

#pragma once

#include <stdbool.h>

#include <pspsdk.h>

#define MAGPIEDRV_MSIF_STATUS_RETRIES 100
#define MAGPIEDRV_MSIF_REG_RETRIES    6

typedef enum {
    MAGPIEDRV_MSIF_COMMAND_READ_REG        = 4,
    MAGPIEDRV_MSIF_COMMAND_READ_BLOCK_CPU  = 5,
    MAGPIEDRV_MSIF_COMMAND_READ_STATUS     = 7,
    MAGPIEDRV_MSIF_COMMAND_SELECT_REG      = 8,
    MAGPIEDRV_MSIF_COMMAND_START_PKT       = 9,
    MAGPIEDRV_MSIF_COMMAND_WRITE_BLOCK_CPU = 10,
    MAGPIEDRV_MSIF_COMMAND_WRITE_REG       = 11,
} magpiedrv_msif_Command;

void magpiedrv_msif_InitializeControl(void);
u32 magpiedrv_msif_GetControl(void);
void magpiedrv_msif_SetControl(const u32 val);
void magpiedrv_msif_ClearControl(const u32 val);
int magpiedrv_msif_WaitControl(void);

void magpiedrv_msif_SetDmaFlags(const u32 val);

u32 magpiedrv_msif_GetStatus(void);

void magpiedrv_msif_WaitFifo(void);

void magpiedrv_msif_SetCommand(const u32 command, const u32 len, const bool is_read);
int magpiedrv_msif_WaitCommand(const bool no_event);

void magpiedrv_msif_SetData(const u32 data);
u32 magpiedrv_msif_GetData(void);

int magpiedrv_msif_SelectReg(const u32 reg, const u32 len, const bool is_read);
int magpiedrv_msif_ReadReg(const u32 reg, const u32 len, u8* buf);
int magpiedrv_msif_ReadStatusReg(u8* buf);
int magpiedrv_msif_WriteReg(const u32 reg, const u32 len, u8* buf);

int magpiedrv_msif_ReadBlock(const u32 len, u8* buf);
int magpiedrv_msif_WriteBlock(const u32 len, u8* buf);
