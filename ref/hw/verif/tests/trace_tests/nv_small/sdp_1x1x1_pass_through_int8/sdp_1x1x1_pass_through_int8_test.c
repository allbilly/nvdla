#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>

static const uint32_t crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
    0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
    0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
    0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
    0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
};

static uint32_t calc_crc32(const void* buf, size_t size) {
    const uint8_t* p = (const uint8_t*)buf;
    uint32_t crc = 0xffffffff;
    for (size_t i = 0; i < size; i++)
        crc = crc32_tab[(crc ^ p[i]) & 0xff] ^ (crc >> 8);
    return crc ^ 0xffffffff;
}

struct dla_reg { int32_t offset; uint32_t value; };

static const struct dla_reg program_initial[] = {
    { 0x9004, 0x00000000 }, /* NVDLA_SDP_S_POINTER_0 */
    { 0x90e8, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_OFLOW_0 */
    { 0x90f8, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_LO_HIT_0 */
    { 0x9044, 0x00000000 }, /* NVDLA_SDP_D_DATA_CUBE_CHANNEL_0 */
    { 0x9000, 0x00000000 }, /* NVDLA_SDP_S_STATUS_0 */
    { 0x9088, 0x437d5c5c }, /* NVDLA_SDP_D_DP_EW_ALU_SRC_VALUE_0 */
    { 0x90d8, 0x00000000 }, /* NVDLA_SDP_D_STATUS_NAN_OUTPUT_NUM_0 */
    { 0x90c0, 0x558fbb6e }, /* NVDLA_SDP_D_CVT_OFFSET_0 */
    { 0x9074, 0x000068a5 }, /* NVDLA_SDP_D_DP_BN_ALU_SRC_VALUE_0 */
    { 0x90a4, 0x0000dcda }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_SCALE_VALUE_0 */
    { 0x90dc, 0x0000000e }, /* NVDLA_SDP_D_PERF_ENABLE_0 */
    { 0x904c, 0x00000000 }, /* NVDLA_SDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0x905c, 0x00001900 }, /* NVDLA_SDP_D_DP_BS_ALU_CFG_0 */
    { 0x90d4, 0x00000000 }, /* NVDLA_SDP_D_STATUS_INF_INPUT_NUM_0 */
    { 0x9040, 0x00000000 }, /* NVDLA_SDP_D_DATA_CUBE_HEIGHT_0 */
    { 0x90ac, 0x0000003c }, /* NVDLA_SDP_D_DP_EW_TRUNCATE_VALUE_0 */
    { 0x9064, 0x00003a00 }, /* NVDLA_SDP_D_DP_BS_MUL_CFG_0 */
    { 0x90a0, 0xaffe9286 }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_OFFSET_VALUE_0 */
    { 0x90b0, 0x00000008 }, /* NVDLA_SDP_D_FEATURE_MODE_CFG_0 */
    { 0x9070, 0x00003201 }, /* NVDLA_SDP_D_DP_BN_ALU_CFG_0 */
    { 0x903c, 0x00000000 }, /* NVDLA_SDP_D_DATA_CUBE_WIDTH_0 */
    { 0x9054, 0x00000008 }, /* NVDLA_SDP_D_DST_SURFACE_STRIDE_0 */
    { 0x9048, 0xd0000040 }, /* NVDLA_SDP_D_DST_BASE_ADDR_LOW_0 */
    { 0x9084, 0x00000001 }, /* NVDLA_SDP_D_DP_EW_ALU_CFG_0 */
    { 0x90b4, 0x00000001 }, /* NVDLA_SDP_D_DST_DMA_CFG_0 */
    { 0x90b8, 0xc9a678a0 }, /* NVDLA_SDP_D_DST_BATCH_STRIDE_0 */
    { 0x90bc, 0x00000000 }, /* NVDLA_SDP_D_DATA_FORMAT_0 */
    { 0x90cc, 0x00000000 }, /* NVDLA_SDP_D_STATUS_0 */
    { 0x9068, 0x0000689f }, /* NVDLA_SDP_D_DP_BS_MUL_SRC_VALUE_0 */
    { 0x90c8, 0x0000000a }, /* NVDLA_SDP_D_CVT_SHIFT_0 */
    { 0x909c, 0xbb23747b }, /* NVDLA_SDP_D_DP_EW_MUL_SRC_VALUE_0 */
    { 0x90f0, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_HYBRID_0 */
    { 0x9078, 0x00002400 }, /* NVDLA_SDP_D_DP_BN_MUL_CFG_0 */
    { 0x906c, 0x00000033 }, /* NVDLA_SDP_D_DP_BN_CFG_0 */
    { 0x9050, 0x00000008 }, /* NVDLA_SDP_D_DST_LINE_STRIDE_0 */
    { 0x90f4, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_LE_HIT_0 */
    { 0x90c4, 0x000016cc }, /* NVDLA_SDP_D_CVT_SCALE_0 */
    { 0x90ec, 0x00000000 }, /* NVDLA_SDP_D_PERF_OUT_SATURATION_0 */
    { 0x907c, 0x00006604 }, /* NVDLA_SDP_D_DP_BN_MUL_SRC_VALUE_0 */
    { 0x9090, 0x0000371f }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_SCALE_VALUE_0 */
    { 0x90a8, 0x00000027 }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_TRUNCATE_VALUE_0 */
    { 0x90d0, 0x00000000 }, /* NVDLA_SDP_D_STATUS_NAN_INPUT_NUM_0 */
    { 0x9058, 0x00000027 }, /* NVDLA_SDP_D_DP_BS_CFG_0 */
    { 0x9080, 0x00000067 }, /* NVDLA_SDP_D_DP_EW_CFG_0 */
    { 0x9098, 0x00000001 }, /* NVDLA_SDP_D_DP_EW_MUL_CFG_0 */
    { 0x908c, 0xf3eadfed }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_OFFSET_VALUE_0 */
    { 0x9094, 0x0000002b }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_TRUNCATE_VALUE_0 */
    { 0x90e4, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_UFLOW_0 */
    { 0x9060, 0x00008924 }, /* NVDLA_SDP_D_DP_BS_ALU_SRC_VALUE_0 */
    { 0x90e0, 0x00000000 }, /* NVDLA_SDP_D_PERF_WDMA_WRITE_STALL_0 */
    { 0x8004, 0x00000000 }, /* NVDLA_SDP_RDMA_S_POINTER_0 */
    { 0x8024, 0x00000008 }, /* NVDLA_SDP_RDMA_D_SRC_SURFACE_STRIDE_0 */
    { 0x800c, 0x00000000 }, /* NVDLA_SDP_RDMA_D_DATA_CUBE_WIDTH_0 */
    { 0x8034, 0x00000120 }, /* NVDLA_SDP_RDMA_D_BS_LINE_STRIDE_0 */
    { 0x8064, 0x00000020 }, /* NVDLA_SDP_RDMA_D_EW_LINE_STRIDE_0 */
    { 0x8058, 0x00000003 }, /* NVDLA_SDP_RDMA_D_ERDMA_CFG_0 */
    { 0x808c, 0x00000000 }, /* NVDLA_SDP_RDMA_D_PERF_NRDMA_READ_STALL_0 */
    { 0x8088, 0x00000000 }, /* NVDLA_SDP_RDMA_D_PERF_BRDMA_READ_STALL_0 */
    { 0x8040, 0x0000000d }, /* NVDLA_SDP_RDMA_D_NRDMA_CFG_0 */
    { 0x8000, 0x00000000 }, /* NVDLA_SDP_RDMA_S_STATUS_0 */
    { 0x8048, 0x0000002f }, /* NVDLA_SDP_RDMA_D_BN_BASE_ADDR_HIGH_0 */
    { 0x8050, 0x00000080 }, /* NVDLA_SDP_RDMA_D_BN_SURFACE_STRIDE_0 */
    { 0x8070, 0x00000000 }, /* NVDLA_SDP_RDMA_D_FEATURE_MODE_CFG_0 */
    { 0x8020, 0x00000020 }, /* NVDLA_SDP_RDMA_D_SRC_LINE_STRIDE_0 */
    { 0x8028, 0x00000013 }, /* NVDLA_SDP_RDMA_D_BRDMA_CFG_0 */
    { 0x804c, 0x00000080 }, /* NVDLA_SDP_RDMA_D_BN_LINE_STRIDE_0 */
    { 0x8010, 0x00000000 }, /* NVDLA_SDP_RDMA_D_DATA_CUBE_HEIGHT_0 */
    { 0x8054, 0x3cbe42a0 }, /* NVDLA_SDP_RDMA_D_BN_BATCH_STRIDE_0 */
    { 0x8044, 0xd5077bc0 }, /* NVDLA_SDP_RDMA_D_BN_BASE_ADDR_LOW_0 */
    { 0x8084, 0x00000000 }, /* NVDLA_SDP_RDMA_D_PERF_MRDMA_READ_STALL_0 */
    { 0x8018, 0x80000400 }, /* NVDLA_SDP_RDMA_D_SRC_BASE_ADDR_LOW_0 */
    { 0x807c, 0x00000000 }, /* NVDLA_SDP_RDMA_D_STATUS_INF_INPUT_NUM_0 */
    { 0x801c, 0x00000000 }, /* NVDLA_SDP_RDMA_D_SRC_BASE_ADDR_HIGH_0 */
    { 0x8030, 0x000000a3 }, /* NVDLA_SDP_RDMA_D_BS_BASE_ADDR_HIGH_0 */
    { 0x8068, 0x00000020 }, /* NVDLA_SDP_RDMA_D_EW_SURFACE_STRIDE_0 */
    { 0x8074, 0x00000001 }, /* NVDLA_SDP_RDMA_D_SRC_DMA_CFG_0 */
    { 0x8090, 0x00000000 }, /* NVDLA_SDP_RDMA_D_PERF_ERDMA_READ_STALL_0 */
    { 0x806c, 0x59850460 }, /* NVDLA_SDP_RDMA_D_EW_BATCH_STRIDE_0 */
    { 0x803c, 0x6d6d2800 }, /* NVDLA_SDP_RDMA_D_BS_BATCH_STRIDE_0 */
    { 0x8080, 0x00000002 }, /* NVDLA_SDP_RDMA_D_PERF_ENABLE_0 */
    { 0x805c, 0x31315180 }, /* NVDLA_SDP_RDMA_D_EW_BASE_ADDR_LOW_0 */
    { 0x802c, 0x19135720 }, /* NVDLA_SDP_RDMA_D_BS_BASE_ADDR_LOW_0 */
    { 0x8060, 0x00000097 }, /* NVDLA_SDP_RDMA_D_EW_BASE_ADDR_HIGH_0 */
    { 0x8078, 0x00000000 }, /* NVDLA_SDP_RDMA_D_STATUS_NAN_INPUT_NUM_0 */
    { 0x8014, 0x00000000 }, /* NVDLA_SDP_RDMA_D_DATA_CUBE_CHANNEL_0 */
    { 0x8038, 0x000001e0 }, /* NVDLA_SDP_RDMA_D_BS_SURFACE_STRIDE_0 */
    { 0x9038, 0x00000001 }, /* NVDLA_SDP_D_OP_ENABLE_0 */
    { 0x8008, 0x00000001 }, /* NVDLA_SDP_RDMA_D_OP_ENABLE_0 */
    {-1, 0}
};

static const struct dla_reg program_enable[] = {
    {-1, 0}
};

#define NVDLA_MMIO_BASE  0x10200000
#define NVDLA_MMIO_SIZE (0x10220000 - 0x10200000)
#define INTR_STATUS    0x100c
#define SDP_DONE_SHIFT 0
#define CDMA_DONE_SHIFT 2
#define CSC_DONE_SHIFT  3
#define CMAC_A_DONE_SHIFT 4
#define CMAC_B_DONE_SHIFT 5
#define CACC_DONE_SHIFT 6
#define BDMA_DONE_SHIFT 7
#define PDP_DONE_SHIFT  8
#define CDP_DONE_SHIFT  9
#define RUBIK_DONE_SHIFT 10

int main(int argc, char* argv[])
{
    int failures = 0;
    int fd = open("/dev/mem", O_RDWR);
    if (fd < 0) { perror("open /dev/mem"); return 1; }
    uint8_t* dla_mmio = (uint8_t*)mmap(NULL, NVDLA_MMIO_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, NVDLA_MMIO_BASE);
    if (dla_mmio == MAP_FAILED) { perror("mmap dla"); close(fd); return 1; }

    printf("Programming initial registers...\n");
    const struct dla_reg* p = program_initial;
    while (p->offset != -1) {
        *(volatile uint32_t*)(dla_mmio + p->offset) = p->value;
        p++;
    }
    printf("Done initial registers.\n");

    printf("Firing OP_ENABLE...\n");
    p = program_enable;
    while (p->offset != -1) {
        *(volatile uint32_t*)(dla_mmio + p->offset) = p->value;
        p++;
    }

    printf("Waiting for done interrupt...\n");
    volatile uint32_t* intr = (volatile uint32_t*)(dla_mmio + INTR_STATUS);
    loop = 100000000;
    while (--loop > 0) {
        uint32_t s = *intr;
        if (s & (1 << SDP_DONE_SHIFT)) { printf("SDP_DONE\n"); break; }
        if (s & (1 << CDMA_DONE_SHIFT)) { printf("CDMA_DONE\n"); break; }
        if (s & (1 << CSC_DONE_SHIFT)) { printf("CSC_DONE\n"); break; }
        if (s & (1 << CMAC_A_DONE_SHIFT)) { printf("CMAC_A_DONE\n"); break; }
        if (s & (1 << CMAC_B_DONE_SHIFT)) { printf("CMAC_B_DONE\n"); break; }
        if (s & (1 << CACC_DONE_SHIFT)) { printf("CACC_DONE\n"); break; }
        if (s & (1 << BDMA_DONE_SHIFT)) { printf("BDMA_DONE\n"); break; }
        if (s & (1 << PDP_DONE_SHIFT)) { printf("PDP_DONE\n"); break; }
        if (s & (1 << CDP_DONE_SHIFT)) { printf("CDP_DONE\n"); break; }
        if (s & (1 << RUBIK_DONE_SHIFT)) { printf("RUBIK_DONE\n"); break; }
    }
    if (loop <= 0) { printf("TIMEOUT\n"); munmap(dla_mmio, NVDLA_MMIO_SIZE); close(fd); return 1; }

    /* Read output and verify CRC */
    { /* CRC[0]: addr=0xd0000040, size=0x8, expect=0xcb0ea2db */
        uint8_t* page = (uint8_t*)mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0xd0000000);
        if (page != MAP_FAILED) {
            uint32_t crc = calc_crc32(page + 64, 8);
            printf("CRC[0] = 0x%08x (expect 0xcb0ea2db) %s\n", crc, crc == 0xcb0ea2db ? "PASS" : "FAIL");
            if (crc != 0xcb0ea2db) failures++;
            munmap(page, 4096);
        } else { perror("mmap out"); }
    }

    printf("Results: %d failures\n", failures);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return failures;
}