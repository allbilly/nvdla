-- AArch64 reset shim: core 0 enters Linux with x0 pointing at the DTB.
_bootloader_aarch64 = {
    0xd53800a6, -- mrs x6, mpidr_el1
    0x580001e7, -- ldr x7, .aff_mask
    0x8a0600e7, -- and x7, x7, x6
    0xf10000ff, -- cmp x7, #0
    0x54000061, -- b.ne .secondary
    0x580001a4, -- ldr x4, .kernel_entry
    0x14000004, -- b .boot
    0xd503205f, -- .secondary: wfe
    0x580001c4, -- ldr x4, .spintable
    0xb4ffffc4, -- cbz x4, .secondary
    0x58000140, -- .boot: ldr x0, .dtb_ptr
    0xaa1f03e1, -- mov x1, xzr
    0xaa1f03e2, -- mov x2, xzr
    0xaa1f03e3, -- mov x3, xzr
    0xd61f0080, -- br x4
    0x00000000,
    0x00ffffff, 0x00000000, -- .aff_mask
    _KERNEL64_LOAD_ADDR, 0x00000000,
    _DTB_LOAD_ADDR, 0x00000000,
    0x00000000, 0x00000000,
}
