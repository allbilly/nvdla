function top()
    local source = debug.getinfo(2, "S").source:sub(2)
    return source:match("(.*/)") or "./"
end

local root = top() .. "../"
local qbox_build = assert(os.getenv("NVDLA_QBOX_BUILD"), "NVDLA_QBOX_BUILD is not set")
local rootfs = os.getenv("NVDLA_ROOTFS") or (root .. "examples/vp/rootfs.ext4")
local dtb = os.getenv("NVDLA_DTB") or (top() .. "old-nvdla-vp.dtb")
local assets = assert(os.getenv("NVDLA_ASSETS"), "NVDLA_ASSETS is not set")
local kernel = os.getenv("NVDLA_KERNEL") or (root .. "examples/vp/Image")

local RAM_BASE = 0x40000000
local RAM_SIZE = 0x40000000
local NVDLA_MEM_BASE = 0xc0000000
local NVDLA_MEM_SIZE = 0x40000000
local UART_BASE = 0x09000000
local VIRTIO_BASE = 0x0a000000
local GIC_DIST_BASE = 0x08000000
local GIC_CPU_BASE = 0x08010000
local NVDLA_BASE = 0x10200000

_KERNEL64_LOAD_ADDR = RAM_BASE + 0x00080000
_DTB_LOAD_ADDR = RAM_BASE + 0x08000000
dofile(top() .. "arm64_bootloader.lua")

local ARCH_TIMER_VIRT_IRQ = 16 + 11
local ARCH_TIMER_S_EL1_IRQ = 16 + 13
local ARCH_TIMER_NS_EL1_IRQ = 16 + 14
local ARCH_TIMER_NS_EL2_IRQ = 16 + 10

print("NVDLA VP: native macOS arm64 + QBox + HVF")
print("rootfs: " .. rootfs)

platform = {
    moduletype = "Container";
    quantum_ns = 10000000;

    router = {
        moduletype = "router";
        log_level = 0;
    };

    ram_0 = {
        moduletype = "gs_memory";
        target_socket = { address = RAM_BASE; size = RAM_SIZE; bind = "&router.initiator_socket" };
        log_level = 0;
    };

    nvdla_ram_0 = {
        moduletype = "gs_memory";
        target_socket = { address = NVDLA_MEM_BASE; size = NVDLA_MEM_SIZE; bind = "&router.initiator_socket" };
        log_level = 0;
    };

    qemu_inst_mgr = { moduletype = "QemuInstanceManager" };
    qemu_inst = {
        moduletype = "QemuInstance";
        args = { "&platform.qemu_inst_mgr", "AARCH64" };
        accel = "hvf";
        sync_policy = "multithread-unconstrained";
    };

    gic_0 = {
        moduletype = "arm_gicv2";
        args = { "&platform.qemu_inst" };
        dist_iface = { address = GIC_DIST_BASE; size = 0x10000; bind = "&router.initiator_socket" };
        cpu_iface = { address = GIC_CPU_BASE; size = 0x10000; bind = "&router.initiator_socket" };
        v2m_iface = { address = 0x08020000; size = 0x1000; bind = "&router.initiator_socket" };
        num_cpu = 1;
        num_spi = 224;
        revision = 2;
        has_msi_support = true;
        irq_out_0 = { bind = "&cpu_0.irq_in" };
        fiq_out_0 = { bind = "&cpu_0.fiq_in" };
    };

    cpu_0 = {
        moduletype = "cpu_arm_host";
        args = { "&platform.qemu_inst" };
        mem = { bind = "&router.target_socket" };
        psci_conduit = "hvc";
        rvbar = RAM_BASE;
        irq_timer_phys_out = { bind = "&gic_0.ppi_in_cpu_0_" .. ARCH_TIMER_NS_EL1_IRQ };
        irq_timer_virt_out = { bind = "&gic_0.ppi_in_cpu_0_" .. ARCH_TIMER_VIRT_IRQ };
        irq_timer_hyp_out = { bind = "&gic_0.ppi_in_cpu_0_" .. ARCH_TIMER_NS_EL2_IRQ };
        irq_timer_sec_out = { bind = "&gic_0.ppi_in_cpu_0_" .. ARCH_TIMER_S_EL1_IRQ };
    };

    global_peripheral_initiator_arm_0 = {
        moduletype = "global_peripheral_initiator";
        args = { "&platform.qemu_inst", "&platform.cpu_0" };
        global_initiator = { bind = "&router.target_socket" };
    };

    nvdla_0 = {
        moduletype = "Nvdla";
        dylib_path = qbox_build .. "/Nvdla";
        nvdla_host_master_if = { address = NVDLA_BASE; size = 0x20000; bind = "&router.initiator_socket" };
        nvdla_core2dbb_axi4 = { bind = "&router.target_socket" };
        nvdla_core2cvsram_axi4 = { bind = "&router.target_socket" };
        irq = { bind = "&gic_0.spi_in_176" };
    };

    virtioblk_0 = {
        moduletype = "virtio_mmio_blk";
        args = { "&platform.qemu_inst" };
        mem = { address = VIRTIO_BASE; size = 0x200; bind = "&router.initiator_socket" };
        irq_out = { bind = "&gic_0.spi_in_16" };
        blkdev_str = "file=" .. rootfs .. ",format=raw,if=none,readonly=off";
    };

    virtioblk_1 = {
        moduletype = "virtio_mmio_blk";
        args = { "&platform.qemu_inst" };
        mem = { address = VIRTIO_BASE + 0x200; size = 0x200; bind = "&router.initiator_socket" };
        irq_out = { bind = "&gic_0.spi_in_17" };
        blkdev_str = "file=" .. assets .. ",format=raw,if=none,readonly=on";
    };

    virtionet_0 = {
        moduletype = "virtio_mmio_net";
        args = { "&platform.qemu_inst" };
        mem = { address = VIRTIO_BASE + 0x400; size = 0x200; bind = "&router.initiator_socket" };
        irq_out = { bind = "&gic_0.spi_in_18" };
        netdev_str = "type=user,hostfwd=tcp::6667-:22";
    };

    charbackend_stdio_0 = {
        moduletype = "char_backend_stdio";
        read_write = true;
    };

    pl011_uart_0 = {
        moduletype = "Pl011";
        dylib_path = "uart-pl011";
        target_socket = { address = UART_BASE; size = 0x1000; bind = "&router.initiator_socket" };
        irq = { bind = "&gic_0.spi_in_1" };
        backend_socket = { bind = "&charbackend_stdio_0.biflow_socket" };
    };

    rtc_0 = {
        moduletype = "pl031";
        args = { "&platform.qemu_inst" };
        mem = { address = 0x09010000; size = 0x1000; bind = "&router.initiator_socket" };
        irq_out = { bind = "&gic_0.spi_in_2" };
    };

    fallback_0 = {
        moduletype = "gs_memory";
        target_socket = { address = 0x0; size = 0x100000000; bind = "&router.initiator_socket"; priority = 1 };
        dmi_allow = false;
        log_level = 0;
    };

    load = {
        moduletype = "loader";
        initiator_socket = { bind = "&router.target_socket" };
        { bin_file = kernel; address = _KERNEL64_LOAD_ADDR };
        { bin_file = dtb; address = _DTB_LOAD_ADDR };
        { data = _bootloader_aarch64; address = RAM_BASE };
    };
}
