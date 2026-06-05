# Magpie (ARM966E-S) memory map

## Memory map

|          | Start address | End address    | Size (physical)
|----------|---------------|----------------|-----------------
| ITCM     | `0x0000_0000` | `0x03FF_FFFF`  | 64 MB (64 KB?)
| DTCM     | `0x0400_0000` | `0x07FF_FFFF`  | 64 MB (16 KB?)
| I/O      | `0x8000_0000` | `0x9FFF_FFFF`? | 512 MB?
| RAM      | `0xC000_0000` | `0xC001_FFFF`  | 128 KB
| Boot ROM | `0xFFF0_0000` | `0xFFFF_FFFF`  | 1 MB (32 KB)
