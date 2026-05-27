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
    { 0xb004, 0x00000000 }, /* NVDLA_PDP_S_POINTER_0 */
    { 0xb000, 0x00000000 }, /* NVDLA_PDP_S_STATUS_0 */
    { 0xb060, 0x00000000 }, /* NVDLA_PDP_D_SRC_BASE_ADDR_LOW_0 */
    { 0xb01c, 0x00000005 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_HEIGHT_0 */
    { 0xb07c, 0x000030a0 }, /* NVDLA_PDP_D_DST_SURFACE_STRIDE_0 */
    { 0xb058, 0x0000000c }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_6_CFG_0 */
    { 0xb00c, 0x00000006 }, /* NVDLA_PDP_D_DATA_CUBE_IN_WIDTH_0 */
    { 0xb074, 0x00000000 }, /* NVDLA_PDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0xb05c, 0x0000000e }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_7_CFG_0 */
    { 0xb09c, 0xd76bec80 }, /* NVDLA_PDP_D_CYA_0 */
    { 0xb02c, 0x2eb9f0bb }, /* NVDLA_PDP_D_PARTIAL_WIDTH_IN_0 */
    { 0xb038, 0x00005555 }, /* NVDLA_PDP_D_RECIP_KERNEL_WIDTH_0 */
    { 0xb098, 0x00000000 }, /* NVDLA_PDP_D_PERF_WRITE_STALL_0 */
    { 0xb03c, 0x00005555 }, /* NVDLA_PDP_D_RECIP_KERNEL_HEIGHT_0 */
    { 0xb078, 0x00000800 }, /* NVDLA_PDP_D_DST_LINE_STRIDE_0 */
    { 0xb090, 0x00000000 }, /* NVDLA_PDP_D_NAN_OUTPUT_NUM_0 */
    { 0xb084, 0x00000000 }, /* NVDLA_PDP_D_DATA_FORMAT_0 */
    { 0xb094, 0x00000000 }, /* NVDLA_PDP_D_PERF_ENABLE_0 */
    { 0xb014, 0x00000009 }, /* NVDLA_PDP_D_DATA_CUBE_IN_CHANNEL_0 */
    { 0xb054, 0x0000000a }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_5_CFG_0 */
    { 0xb048, 0x00000004 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_2_CFG_0 */
    { 0xb040, 0x00002121 }, /* NVDLA_PDP_D_POOLING_PADDING_CFG_0 */
    { 0xb064, 0x00000000 }, /* NVDLA_PDP_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xb018, 0x00000003 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_WIDTH_0 */
    { 0xb034, 0x00110202 }, /* NVDLA_PDP_D_POOLING_KERNEL_CFG_0 */
    { 0xb070, 0xc0000000 }, /* NVDLA_PDP_D_DST_BASE_ADDR_LOW_0 */
    { 0xb028, 0x00000001 }, /* NVDLA_PDP_D_NAN_FLUSH_TO_ZERO_0 */
    { 0xb06c, 0x00000880 }, /* NVDLA_PDP_D_SRC_SURFACE_STRIDE_0 */
    { 0xb044, 0x00000002 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_1_CFG_0 */
    { 0xb020, 0x00000009 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_CHANNEL_0 */
    { 0xb010, 0x00000008 }, /* NVDLA_PDP_D_DATA_CUBE_IN_HEIGHT_0 */
    { 0xb04c, 0x00000006 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_3_CFG_0 */
    { 0xb080, 0x00000001 }, /* NVDLA_PDP_D_DST_RAM_CFG_0 */
    { 0xb030, 0x2c5f1256 }, /* NVDLA_PDP_D_PARTIAL_WIDTH_OUT_0 */
    { 0xb024, 0x00000010 }, /* NVDLA_PDP_D_OPERATION_MODE_CFG_0 */
    { 0xb08c, 0x00000000 }, /* NVDLA_PDP_D_NAN_INPUT_NUM_0 */
    { 0xb088, 0x00000000 }, /* NVDLA_PDP_D_INF_INPUT_NUM_0 */
    { 0xb050, 0x00000008 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_4_CFG_0 */
    { 0xb068, 0x000000a0 }, /* NVDLA_PDP_D_SRC_LINE_STRIDE_0 */
    { 0xa004, 0x00000000 }, /* NVDLA_PDP_RDMA_S_POINTER_0 */
    { 0xa014, 0x00000009 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_CHANNEL_0 */
    { 0xa018, 0x00000001 }, /* NVDLA_PDP_RDMA_D_FLYING_MODE_0 */
    { 0xa02c, 0x00000001 }, /* NVDLA_PDP_RDMA_D_SRC_RAM_CFG_0 */
    { 0xa028, 0x00000880 }, /* NVDLA_PDP_RDMA_D_SRC_SURFACE_STRIDE_0 */
    { 0xa01c, 0x80800000 }, /* NVDLA_PDP_RDMA_D_SRC_BASE_ADDR_LOW_0 */
    { 0xa020, 0x00000000 }, /* NVDLA_PDP_RDMA_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xa03c, 0x00000005 }, /* NVDLA_PDP_RDMA_D_POOLING_PADDING_CFG_0 */
    { 0xa024, 0x000000a0 }, /* NVDLA_PDP_RDMA_D_SRC_LINE_STRIDE_0 */
    { 0xa044, 0x00000001 }, /* NVDLA_PDP_RDMA_D_PERF_ENABLE_0 */
    { 0xa048, 0x00000000 }, /* NVDLA_PDP_RDMA_D_PERF_READ_STALL_0 */
    { 0xa038, 0x00000012 }, /* NVDLA_PDP_RDMA_D_POOLING_KERNEL_CFG_0 */
    { 0xa034, 0x00000000 }, /* NVDLA_PDP_RDMA_D_OPERATION_MODE_CFG_0 */
    { 0xa030, 0x00000000 }, /* NVDLA_PDP_RDMA_D_DATA_FORMAT_0 */
    { 0xa00c, 0x00000006 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_WIDTH_0 */
    { 0xa040, 0x2eb9f0bb }, /* NVDLA_PDP_RDMA_D_PARTIAL_WIDTH_IN_0 */
    { 0xa010, 0x00000008 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_HEIGHT_0 */
    { 0xa04c, 0xc09253f8 }, /* NVDLA_PDP_RDMA_D_CYA_0 */
    { 0xa000, 0x00000000 }, /* NVDLA_PDP_RDMA_S_STATUS_0 */
    { 0xb008, 0x00000001 }, /* NVDLA_PDP_D_OP_ENABLE_0 */
    { 0xa008, 0x00000001 }, /* NVDLA_PDP_RDMA_D_OP_ENABLE_0 */
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
    { /* CRC[0]: addr=0xc0000000, size=0x6140, expect=0x3e993ffa */
        uint8_t* page = (uint8_t*)mmap(NULL, 28672, PROT_READ, MAP_SHARED, fd, 0xc0000000);
        if (page != MAP_FAILED) {
            uint32_t crc = calc_crc32(page + 0, 24896);
            printf("CRC[0] = 0x%08x (expect 0x3e993ffa) %s\n", crc, crc == 0x3e993ffa ? "PASS" : "FAIL");
            if (crc != 0x3e993ffa) failures++;
            munmap(page, 28672);
        } else { perror("mmap out"); }
    }

    printf("Results: %d failures\n", failures);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return failures;
}