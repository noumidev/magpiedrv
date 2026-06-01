**magpiedrv** is a reimplementation of the PlayStation Portable `sceWlan` kernel module.

# How to use
1. Call `magpiedrv_Initialize`, flip WLAN switch (needs to be in OFF position before booting the homebrew!)
2. Call `magpiedrv_InitializeInterface` and `magpiedrv_InitializeMagpie`
3. Upload ARM payload with `magpiedrv_UploadFirmware`

# To-dos
- Improve stability
- Figure out all Memory Stick I/F internal registers
- Add documentation, make this user-friendlier
- ...

# Credits

- [PSI](https://github.com/psi-rockin) for `sceWlan` and Magpie boot ROM/firmware reverse engineering efforts
- [`pspsdk`](https://github.com/pspdev/pspsdk)
- [`LibPspExploit`](https://github.com/pspdev/psp-cfw-sdk/tree/main/src/LibPspExploit)
