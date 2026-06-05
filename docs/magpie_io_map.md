# Magpie (ARM966E-S) I/O map

## I/O map

|       | Start address | End address
|-------|---------------|----------------
| HIF   | `0x8000_0000` | `0x8000_004B`?
| SPI   | `0x8000_C100` | `0x8000_C12B`?
| DMAC  | `0x8000_E800` | `0x8000_E8AF`?
| INTC  | `0x9000_8000` | `0x9000_800F`
| TIMER | `0x9000_9000` | `0x9000_902B`?

## Host interface

| Register       | Address       | MS I/F register | Size
|----------------|---------------|-----------------|--------
| `HIF_COMM`     | `0x8000_000C` | `0x4A`          | 32-bit
| `HIF_INTFLAGS` | `0x8000_0024` | N/A?            | 32-bit?
| `HIF_SEMA`     | `0x8000_0034` | `0x54`          | 8-bit
| `HIF_RX0`      | `0x8000_0038` | N/A?            | 32-bit
| `HIF_TX0`      | `0x8000_003C` | N/A?            | 32-bit
| `HIF_TX1`      | `0x8000_0040` | N/A?            | 32-bit
| `HIF_RX1`      | `0x8000_0044` | N/A?            | 32-bit
| `HIF_CIS`      | `0x8000_0048` | N/A?            | 32-bit

The Magpie-side host interface appears to be CardBus. Some of its registers are accessible via the Memory Stick interface.

`HIF_RX0`/`HIF_TX0`/`HIF_TX1`/`HIF_RX1`/`HIF_CIS` are address registers used for MS I/F transfers. `HIF_CIS` provides the buffer address for MS I/F packet command `0xB2`, the original firmware uses this for the Card Information Structure.

## SPI

| Register       | Address       | Size
|----------------|---------------|--------
| `SPI_CONTROL`? | `0x8000_C100` | 16-bit
| `SPI_DEVICE`   | `0x8000_C104` | 8-bit
| ?              | `0x8000_C108` | 16-bit
| `SPI_STATUS`   | `0x8000_C11C` | 8-bit
| `SPI_DATA`     | `0x8000_C124` | 8-bit
| `SPI_REGADDR`  | `0x8000_C128` | 8-bit

Known SPI devices are:

| Device             | Address
|--------------------|---------
| Baseband Processor | `0x54`
| PHY                | `0x58`
| EEPROM             | `0x82`

`SPI_STATUS` returns 0 when the interface isn't busy. EEPROM commands and data are sent/received via `SPI_DATA`, PHY and BBP registers accessed via `SPI_DATA` and `SPI_REGADDR` (the latter sets the register address).

## Interrupt controller

| Register     | Address       | Size
|--------------|---------------|--------
| `INTC_FLAGS` | `0x9000_8000` | 32-bit
| `INTC_MASK`  | `0x9000_8008` | 32-bit
| `INTC_ACK`   | `0x9000_800C` | 32-bit

There are up to 20 interrupt sources. Known sources are:

|         | Source
|---------|---------------
| `IRQ4`  | Timer 0
| `IRQ8`  | Host I/F
| `IRQ15` | DMA channel 0

Writing `1` to a bit in `INTC_MASK` enables the corresponding interrupt.

Writing `1` to a bit in `INTC_ACK` clears the corresponding bits in `INTC_FLAGS` *and* `INTC_MASK`.

## Timers

| Register           | Address            | Size
|--------------------|--------------------|--------
| `TIMERn_PRESCALER` | `0x9000_9000 + 4n` | 32-bit
| `TIMER_CONTROL`    | `0x9000_9010`      | 32-bit?
| `TIMERn_COUNTER`   | `0x9000_9014 + 4n` | 32-bit
| `TIMER_INTFLAGS`   | `0x9000_9024`      | 32-bit?
| `TIMER_INTMASK`    | `0x9000_9028`      | 32-bit?

There *should* be four timers, but seemingly only one works. The `TIMERn_COUNTER` also don't appear to be working.
