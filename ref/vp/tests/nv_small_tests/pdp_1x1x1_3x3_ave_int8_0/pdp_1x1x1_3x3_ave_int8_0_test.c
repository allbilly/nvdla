#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static const uint32_t crc32_tab[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
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
    { 0xb040, 0x00000022 }, /* NVDLA_PDP_D_POOLING_PADDING_CFG_0 */
    { 0xb074, 0x00000000 }, /* NVDLA_PDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0xb024, 0x00000010 }, /* NVDLA_PDP_D_OPERATION_MODE_CFG_0 */
    { 0xb044, 0x00000010 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_1_CFG_0 */
    { 0xb034, 0x00110202 }, /* NVDLA_PDP_D_POOLING_KERNEL_CFG_0 */
    { 0xb01c, 0x00000000 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_HEIGHT_0 */
    { 0xb080, 0x00000001 }, /* NVDLA_PDP_D_DST_RAM_CFG_0 */
    { 0xb010, 0x00000000 }, /* NVDLA_PDP_D_DATA_CUBE_IN_HEIGHT_0 */
    { 0xb058, 0x00000060 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_6_CFG_0 */
    { 0xb098, 0x00000000 }, /* NVDLA_PDP_D_PERF_WRITE_STALL_0 */
    { 0xb00c, 0x00000000 }, /* NVDLA_PDP_D_DATA_CUBE_IN_WIDTH_0 */
    { 0xb02c, 0x27c9b90a }, /* NVDLA_PDP_D_PARTIAL_WIDTH_IN_0 */
    { 0xb054, 0x00000050 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_5_CFG_0 */
    { 0xb07c, 0x000026c0 }, /* NVDLA_PDP_D_DST_SURFACE_STRIDE_0 */
    { 0xb03c, 0x00005555 }, /* NVDLA_PDP_D_RECIP_KERNEL_HEIGHT_0 */
    { 0xb06c, 0x00000d00 }, /* NVDLA_PDP_D_SRC_SURFACE_STRIDE_0 */
    { 0xb014, 0x00000000 }, /* NVDLA_PDP_D_DATA_CUBE_IN_CHANNEL_0 */
    { 0xb084, 0x00000000 }, /* NVDLA_PDP_D_DATA_FORMAT_0 */
    { 0xb018, 0x00000000 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_WIDTH_0 */
    { 0xb09c, 0x19d97a33 }, /* NVDLA_PDP_D_CYA_0 */
    { 0xb04c, 0x00000030 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_3_CFG_0 */
    { 0xb070, 0xc0000120 }, /* NVDLA_PDP_D_DST_BASE_ADDR_LOW_0 */
    { 0xb050, 0x00000040 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_4_CFG_0 */
    { 0xb090, 0x00000000 }, /* NVDLA_PDP_D_NAN_OUTPUT_NUM_0 */
    { 0xb020, 0x00000000 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_CHANNEL_0 */
    { 0xb038, 0x00005555 }, /* NVDLA_PDP_D_RECIP_KERNEL_WIDTH_0 */
    { 0xb068, 0x000003a0 }, /* NVDLA_PDP_D_SRC_LINE_STRIDE_0 */
    { 0xb088, 0x00000000 }, /* NVDLA_PDP_D_INF_INPUT_NUM_0 */
    { 0xb078, 0x00002000 }, /* NVDLA_PDP_D_DST_LINE_STRIDE_0 */
    { 0xb028, 0x00000000 }, /* NVDLA_PDP_D_NAN_FLUSH_TO_ZERO_0 */
    { 0xb030, 0x2c571256 }, /* NVDLA_PDP_D_PARTIAL_WIDTH_OUT_0 */
    { 0xb048, 0x00000020 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_2_CFG_0 */
    { 0xb060, 0x00000000 }, /* NVDLA_PDP_D_SRC_BASE_ADDR_LOW_0 */
    { 0xb08c, 0x00000000 }, /* NVDLA_PDP_D_NAN_INPUT_NUM_0 */
    { 0xb064, 0x00000000 }, /* NVDLA_PDP_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xb094, 0x00000001 }, /* NVDLA_PDP_D_PERF_ENABLE_0 */
    { 0xb05c, 0x00000070 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_7_CFG_0 */
    { 0xb000, 0x00000000 }, /* NVDLA_PDP_S_STATUS_0 */
    { 0xa004, 0x00000000 }, /* NVDLA_PDP_RDMA_S_POINTER_0 */
    { 0xa014, 0x00000000 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_CHANNEL_0 */
    { 0xa040, 0x27c9b90a }, /* NVDLA_PDP_RDMA_D_PARTIAL_WIDTH_IN_0 */
    { 0xa01c, 0xc0002b00 }, /* NVDLA_PDP_RDMA_D_SRC_BASE_ADDR_LOW_0 */
    { 0xa030, 0x00000000 }, /* NVDLA_PDP_RDMA_D_DATA_FORMAT_0 */
    { 0xa00c, 0x00000000 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_WIDTH_0 */
    { 0xa03c, 0x00000006 }, /* NVDLA_PDP_RDMA_D_POOLING_PADDING_CFG_0 */
    { 0xa048, 0x00000000 }, /* NVDLA_PDP_RDMA_D_PERF_READ_STALL_0 */
    { 0xa034, 0x00000000 }, /* NVDLA_PDP_RDMA_D_OPERATION_MODE_CFG_0 */
    { 0xa044, 0x00000000 }, /* NVDLA_PDP_RDMA_D_PERF_ENABLE_0 */
    { 0xa000, 0x00000000 }, /* NVDLA_PDP_RDMA_S_STATUS_0 */
    { 0xa018, 0x00000001 }, /* NVDLA_PDP_RDMA_D_FLYING_MODE_0 */
    { 0xa024, 0x000003a0 }, /* NVDLA_PDP_RDMA_D_SRC_LINE_STRIDE_0 */
    { 0xa02c, 0x00000001 }, /* NVDLA_PDP_RDMA_D_SRC_RAM_CFG_0 */
    { 0xa04c, 0x81e7f8f3 }, /* NVDLA_PDP_RDMA_D_CYA_0 */
    { 0xa038, 0x00000012 }, /* NVDLA_PDP_RDMA_D_POOLING_KERNEL_CFG_0 */
    { 0xa020, 0x00000000 }, /* NVDLA_PDP_RDMA_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xa010, 0x00000000 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_HEIGHT_0 */
    { 0xa028, 0x00000d00 }, /* NVDLA_PDP_RDMA_D_SRC_SURFACE_STRIDE_0 */
    { 0xb008, 0x00000001 }, /* NVDLA_PDP_D_OP_ENABLE_0 */
    { 0xa008, 0x00000001 }, /* NVDLA_PDP_RDMA_D_OP_ENABLE_0 */
    {-1, 0}
};

static const struct dla_reg program_enable[] = {
    {-1, 0}
};

/* Embedded data blocks (loaded via mmap instead of devmem scripts) */
static const uint32_t embed_0_data[][2] = {
    { 0x00000000, 0x000000f6 },
    { 0xFFFFFFFF, 0 }
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
    int loop;
    int fd = open("/dev/mem", O_RDWR);
    if (fd < 0) { perror("open /dev/mem"); return 1; }
    uint8_t* dla_mmio = (uint8_t*)mmap(NULL, NVDLA_MMIO_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, NVDLA_MMIO_BASE);
    if (dla_mmio == MAP_FAILED) { perror("mmap dla"); close(fd); return 1; }

    { /* Zero-init region 0xc0002b00 size 0xd00 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0002000);
        if (zpage != MAP_FAILED) { memset(zpage + 2816, 0, 3328); munmap(zpage, 8192); }
    }
    { /* Zero-init region 0xc0000120 size 0x26c0 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 12288, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 288, 0, 9920); munmap(zpage, 12288); }
    }

    /* Load embedded data (replaces devmem load scripts) */
    { /* Data: base=0xc0002b00, 1 writes */
        uint8_t* dpage = (uint8_t*)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0002000);
        if (dpage != MAP_FAILED) {
            const uint32_t (*dp)[2] = embed_0_data;
            uint32_t base_off = 0xb00;
            while ((*dp)[0] != 0xFFFFFFFF) {
                *(volatile uint32_t*)(dpage + base_off + (*dp)[0]) = (*dp)[1];
                dp++;
            }
            munmap(dpage, 4096);
        } else { perror("mmap embed"); }
    }

    printf("Programming initial registers...\n");
    const struct dla_reg* p = program_initial;
    while (p->offset != -1) {
        *(volatile uint32_t*)(dla_mmio + p->offset) = p->value;
        p++;
    }
    printf("Done initial registers.\n");
    /* Zero-init CRC output regions to prevent false positives */
    { /* CRC[0] zero-init: addr=0xc0000120 size=0x26c0 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 12288, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 288, 0, 9920); munmap(zpage, 12288); }
    }

    printf("Firing OP_ENABLE...\n");
    p = program_enable;
    while (p->offset != -1) {
        *(volatile uint32_t*)(dla_mmio + p->offset) = p->value;
        p++;
    }

    printf("Waiting for done interrupt...\n");
    volatile uint32_t* intr = (volatile uint32_t*)(dla_mmio + INTR_STATUS);
    loop = 100000000;
    uint32_t done_mask = 0;
    int extra_wait = 0;
    while (--loop > 0) {
        uint32_t s = *intr;
        uint32_t new_bits = s & ~done_mask;
        done_mask |= s;
        if (new_bits & (1 << SDP_DONE_SHIFT)) { printf("SDP_DONE "); fflush(stdout); extra_wait = 10000; }
        if (new_bits & (1 << CDMA_DONE_SHIFT)) { printf("CDMA_DONE "); fflush(stdout); extra_wait = 10000; }
        if (new_bits & (1 << CSC_DONE_SHIFT)) { printf("CSC_DONE "); fflush(stdout); extra_wait = 10000; }
        if (new_bits & (1 << CMAC_A_DONE_SHIFT)) { printf("CMAC_A_DONE "); fflush(stdout); extra_wait = 10000; }
        if (new_bits & (1 << CMAC_B_DONE_SHIFT)) { printf("CMAC_B_DONE "); fflush(stdout); extra_wait = 10000; }
        if (new_bits & (1 << CACC_DONE_SHIFT)) { printf("CACC_DONE "); fflush(stdout); extra_wait = 10000; }
        if (new_bits & (1 << BDMA_DONE_SHIFT)) { printf("BDMA_DONE "); fflush(stdout); extra_wait = 10000; }
        if (new_bits & (1 << PDP_DONE_SHIFT)) { printf("PDP_DONE "); fflush(stdout); extra_wait = 10000; }
        if (new_bits & (1 << CDP_DONE_SHIFT)) { printf("CDP_DONE "); fflush(stdout); extra_wait = 10000; }
        if (new_bits & (1 << RUBIK_DONE_SHIFT)) { printf("RUBIK_DONE "); fflush(stdout); extra_wait = 10000; }
        if (extra_wait > 0 && --extra_wait <= 0) break;
    }
    printf("\n");
    if (loop <= 0) { printf("TIMEOUT\n"); munmap(dla_mmio, NVDLA_MMIO_SIZE); close(fd); return 1; }

    /* Read output and verify CRC */
    { /* CRC[0]: addr=0xc0000120, size=0x26c0, expect=0xafff270d */
        uint8_t* page = (uint8_t*)mmap(NULL, 12288, PROT_READ, MAP_SHARED, fd, 0xc0000000);
        if (page != MAP_FAILED) {
            uint32_t crc = calc_crc32(page + 288, 9920);
            printf("CRC[0] = 0x%08x (expect 0xafff270d) %s\n", crc, crc == 0xafff270d ? "PASS" : "FAIL");
            if (crc != 0xafff270d) failures++;
            munmap(page, 12288);
        } else { perror("mmap out"); }
    }

    printf("Results: %d failures\n", failures);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return failures;
}