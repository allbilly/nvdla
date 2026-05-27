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
    { 0x1004, 0x003f03fc }, /* NVDLA_GLB_S_INTR_MASK_0 */
    { 0x100c, 0x00000000 }, /* NVDLA_GLB_S_INTR_STATUS_0 */
    { 0x9004, 0x00000000 }, /* NVDLA_SDP_S_POINTER_0 */
    { 0x90a8, 0x00000020 }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_TRUNCATE_VALUE_0 */
    { 0x90b0, 0x00000001 }, /* NVDLA_SDP_D_FEATURE_MODE_CFG_0 */
    { 0x903c, 0x00000006 }, /* NVDLA_SDP_D_DATA_CUBE_WIDTH_0 */
    { 0x90c8, 0x00000000 }, /* NVDLA_SDP_D_CVT_SHIFT_0 */
    { 0x90b8, 0x80f0e8e0 }, /* NVDLA_SDP_D_DST_BATCH_STRIDE_0 */
    { 0x9078, 0x00002701 }, /* NVDLA_SDP_D_DP_BN_MUL_CFG_0 */
    { 0x9098, 0x00000000 }, /* NVDLA_SDP_D_DP_EW_MUL_CFG_0 */
    { 0x9044, 0x0000001f }, /* NVDLA_SDP_D_DATA_CUBE_CHANNEL_0 */
    { 0x90f0, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_HYBRID_0 */
    { 0x9000, 0x00000000 }, /* NVDLA_SDP_S_STATUS_0 */
    { 0x9088, 0xcf151400 }, /* NVDLA_SDP_D_DP_EW_ALU_SRC_VALUE_0 */
    { 0x907c, 0x00000a1a }, /* NVDLA_SDP_D_DP_BN_MUL_SRC_VALUE_0 */
    { 0x90ac, 0x0000003b }, /* NVDLA_SDP_D_DP_EW_TRUNCATE_VALUE_0 */
    { 0x9080, 0x00000011 }, /* NVDLA_SDP_D_DP_EW_CFG_0 */
    { 0x90cc, 0x00000000 }, /* NVDLA_SDP_D_STATUS_0 */
    { 0x90e4, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_UFLOW_0 */
    { 0x90dc, 0x00000003 }, /* NVDLA_SDP_D_PERF_ENABLE_0 */
    { 0x904c, 0x00000000 }, /* NVDLA_SDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0x90d0, 0x00000000 }, /* NVDLA_SDP_D_STATUS_NAN_INPUT_NUM_0 */
    { 0x908c, 0x7f8fad74 }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_OFFSET_VALUE_0 */
    { 0x9060, 0x00000b64 }, /* NVDLA_SDP_D_DP_BS_ALU_SRC_VALUE_0 */
    { 0x909c, 0x00005f15 }, /* NVDLA_SDP_D_DP_EW_MUL_SRC_VALUE_0 */
    { 0x90f8, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_LO_HIT_0 */
    { 0x90ec, 0x00000000 }, /* NVDLA_SDP_D_PERF_OUT_SATURATION_0 */
    { 0x90d8, 0x00000000 }, /* NVDLA_SDP_D_STATUS_NAN_OUTPUT_NUM_0 */
    { 0x9090, 0x000022b9 }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_SCALE_VALUE_0 */
    { 0x9064, 0x00002e01 }, /* NVDLA_SDP_D_DP_BS_MUL_CFG_0 */
    { 0x9084, 0x00000003 }, /* NVDLA_SDP_D_DP_EW_ALU_CFG_0 */
    { 0x9050, 0x00000040 }, /* NVDLA_SDP_D_DST_LINE_STRIDE_0 */
    { 0x9054, 0x00000840 }, /* NVDLA_SDP_D_DST_SURFACE_STRIDE_0 */
    { 0x906c, 0x0000004b }, /* NVDLA_SDP_D_DP_BN_CFG_0 */
    { 0x9094, 0x00000002 }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_TRUNCATE_VALUE_0 */
    { 0x90e0, 0x00000000 }, /* NVDLA_SDP_D_PERF_WDMA_WRITE_STALL_0 */
    { 0x9058, 0x00000057 }, /* NVDLA_SDP_D_DP_BS_CFG_0 */
    { 0x90a4, 0x0000666e }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_SCALE_VALUE_0 */
    { 0x90f4, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_LE_HIT_0 */
    { 0x9074, 0x00004cf9 }, /* NVDLA_SDP_D_DP_BN_ALU_SRC_VALUE_0 */
    { 0x90d4, 0x00000000 }, /* NVDLA_SDP_D_STATUS_INF_INPUT_NUM_0 */
    { 0x90b4, 0x00000001 }, /* NVDLA_SDP_D_DST_DMA_CFG_0 */
    { 0x9070, 0x00003f00 }, /* NVDLA_SDP_D_DP_BN_ALU_CFG_0 */
    { 0x90bc, 0x00000000 }, /* NVDLA_SDP_D_DATA_FORMAT_0 */
    { 0x9040, 0x00000010 }, /* NVDLA_SDP_D_DATA_CUBE_HEIGHT_0 */
    { 0x9048, 0xc0000120 }, /* NVDLA_SDP_D_DST_BASE_ADDR_LOW_0 */
    { 0x905c, 0x00001101 }, /* NVDLA_SDP_D_DP_BS_ALU_CFG_0 */
    { 0x90a0, 0x997ade1a }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_OFFSET_VALUE_0 */
    { 0x90e8, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_OFLOW_0 */
    { 0x90c4, 0x00000001 }, /* NVDLA_SDP_D_CVT_SCALE_0 */
    { 0x9068, 0x00008d68 }, /* NVDLA_SDP_D_DP_BS_MUL_SRC_VALUE_0 */
    { 0x90c0, 0x00000000 }, /* NVDLA_SDP_D_CVT_OFFSET_0 */
    { 0x3004, 0x00000000 }, /* NVDLA_CDMA_S_POINTER_0 */
    { 0x30cc, 0x00000000 }, /* NVDLA_CDMA_D_INF_INPUT_DATA_NUM_0 */
    { 0x301c, 0x000f0007 }, /* NVDLA_CDMA_D_DATAIN_SIZE_0_0 */
    { 0x3044, 0xcb3831a0 }, /* NVDLA_CDMA_D_LINE_UV_STRIDE_0 */
    { 0x3048, 0x00000800 }, /* NVDLA_CDMA_D_SURF_STRIDE_0 */
    { 0x30c4, 0x00000000 }, /* NVDLA_CDMA_D_NAN_INPUT_DATA_NUM_0 */
    { 0x3088, 0x9502b120 }, /* NVDLA_CDMA_D_WGS_ADDR_LOW_0 */
    { 0x30ac, 0x00000001 }, /* NVDLA_CDMA_D_CVT_SCALE_0 */
    { 0x30e8, 0xe067315b }, /* NVDLA_CDMA_D_CYA_0 */
    { 0x303c, 0x5f10b0e0 }, /* NVDLA_CDMA_D_DAIN_ADDR_LOW_1_0 */
    { 0x3030, 0x00000000 }, /* NVDLA_CDMA_D_DAIN_ADDR_HIGH_0_0 */
    { 0x30a4, 0x00000000 }, /* NVDLA_CDMA_D_CVT_CFG_0 */
    { 0x30dc, 0x00000000 }, /* NVDLA_CDMA_D_PERF_WT_READ_STALL_0 */
    { 0x3040, 0x00000080 }, /* NVDLA_CDMA_D_LINE_STRIDE_0 */
    { 0x3068, 0x00000000 }, /* NVDLA_CDMA_D_WEIGHT_FORMAT_0 */
    { 0x3090, 0x14590640 }, /* NVDLA_CDMA_D_WMB_ADDR_LOW_0 */
    { 0x30e0, 0x00000000 }, /* NVDLA_CDMA_D_PERF_DAT_READ_LATENCY_0 */
    { 0x3034, 0xc000b800 }, /* NVDLA_CDMA_D_DAIN_ADDR_LOW_0_0 */
    { 0x302c, 0x00000001 }, /* NVDLA_CDMA_D_DAIN_RAM_TYPE_0 */
    { 0x3084, 0x00000019 }, /* NVDLA_CDMA_D_WGS_ADDR_HIGH_0 */
    { 0x3060, 0x0000007f }, /* NVDLA_CDMA_D_ENTRY_PER_SLICE_0 */
    { 0x30b8, 0x00003b11 }, /* NVDLA_CDMA_D_ZERO_PADDING_VALUE_0 */
    { 0x3054, 0x00130002 }, /* NVDLA_CDMA_D_RESERVED_Y_CFG_0 */
    { 0x3020, 0x0000007f }, /* NVDLA_CDMA_D_DATAIN_SIZE_1_0 */
    { 0x309c, 0x3ef91cb3 }, /* NVDLA_CDMA_D_MEAN_GLOBAL_0_0 */
    { 0x30d8, 0x00000000 }, /* NVDLA_CDMA_D_PERF_DAT_READ_STALL_0 */
    { 0x30b0, 0x00000000 }, /* NVDLA_CDMA_D_CONV_STRIDE_0 */
    { 0x305c, 0x1413eb80 }, /* NVDLA_CDMA_D_BATCH_STRIDE_0 */
    { 0x3038, 0x00000084 }, /* NVDLA_CDMA_D_DAIN_ADDR_HIGH_1_0 */
    { 0x30b4, 0x02010001 }, /* NVDLA_CDMA_D_ZERO_PADDING_0 */
    { 0x304c, 0x00000000 }, /* NVDLA_CDMA_D_DAIN_MAP_0 */
    { 0x3050, 0x00770345 }, /* NVDLA_CDMA_D_RESERVED_X_CFG_0 */
    { 0x3080, 0x00009000 }, /* NVDLA_CDMA_D_WEIGHT_BYTES_0 */
    { 0x306c, 0x0000047f }, /* NVDLA_CDMA_D_WEIGHT_SIZE_0_0 */
    { 0x30d4, 0x00000000 }, /* NVDLA_CDMA_D_PERF_ENABLE_0 */
    { 0x3074, 0x00000001 }, /* NVDLA_CDMA_D_WEIGHT_RAM_TYPE_0 */
    { 0x30c0, 0x00000001 }, /* NVDLA_CDMA_D_NAN_FLUSH_TO_ZERO_0 */
    { 0x3070, 0x0000001f }, /* NVDLA_CDMA_D_WEIGHT_SIZE_1_0 */
    { 0x3028, 0x00000010 }, /* NVDLA_CDMA_D_PIXEL_OFFSET_0 */
    { 0x3024, 0x000f0007 }, /* NVDLA_CDMA_D_DATAIN_SIZE_EXT_0_0 */
    { 0x308c, 0x0000002a }, /* NVDLA_CDMA_D_WMB_ADDR_HIGH_0 */
    { 0x30e4, 0x00000000 }, /* NVDLA_CDMA_D_PERF_WT_READ_LATENCY_0 */
    { 0x3098, 0x00000001 }, /* NVDLA_CDMA_D_MEAN_FORMAT_0 */
    { 0x3000, 0x00000000 }, /* NVDLA_CDMA_S_STATUS_0 */
    { 0x3014, 0x00000000 }, /* NVDLA_CDMA_D_MISC_CFG_0 */
    { 0x30c8, 0x00000000 }, /* NVDLA_CDMA_D_NAN_INPUT_WEIGHT_NUM_0 */
    { 0x3058, 0x00000000 }, /* NVDLA_CDMA_D_BATCH_NUMBER_0 */
    { 0x30a8, 0x00000000 }, /* NVDLA_CDMA_D_CVT_OFFSET_0 */
    { 0x30bc, 0x0004000a }, /* NVDLA_CDMA_D_BANK_0 */
    { 0x30a0, 0xba84f30a }, /* NVDLA_CDMA_D_MEAN_GLOBAL_1_0 */
    { 0x30d0, 0x00000000 }, /* NVDLA_CDMA_D_INF_INPUT_WEIGHT_NUM_0 */
    { 0x3008, 0x00010001 }, /* NVDLA_CDMA_S_ARBITER_0 */
    { 0x3078, 0x00000000 }, /* NVDLA_CDMA_D_WEIGHT_ADDR_HIGH_0 */
    { 0x307c, 0xc0002700 }, /* NVDLA_CDMA_D_WEIGHT_ADDR_LOW_0 */
    { 0x3064, 0x00000000 }, /* NVDLA_CDMA_D_FETCH_GRAIN_0 */
    { 0x3018, 0x00010900 }, /* NVDLA_CDMA_D_DATAIN_FORMAT_0 */
    { 0x300c, 0x00000000 }, /* NVDLA_CDMA_S_CBUF_FLUSH_STATUS_0 */
    { 0x3094, 0x008fa600 }, /* NVDLA_CDMA_D_WMB_BYTES_0 */
    { 0x4004, 0x00000000 }, /* NVDLA_CSC_S_POINTER_0 */
    { 0x401c, 0x00000000 }, /* NVDLA_CSC_D_BATCH_NUMBER_0 */
    { 0x4048, 0x0000000f }, /* NVDLA_CSC_D_RELEASE_0 */
    { 0x4064, 0xe067315b }, /* NVDLA_CSC_D_CYA_0 */
    { 0x4020, 0x00000000 }, /* NVDLA_CSC_D_POST_Y_EXTENSION_0 */
    { 0x402c, 0x00020002 }, /* NVDLA_CSC_D_WEIGHT_SIZE_EXT_0_0 */
    { 0x4010, 0x00000000 }, /* NVDLA_CSC_D_DATAIN_FORMAT_0 */
    { 0x4054, 0x00010001 }, /* NVDLA_CSC_D_ZERO_PADDING_0 */
    { 0x4040, 0x0000001f }, /* NVDLA_CSC_D_DATAOUT_SIZE_1_0 */
    { 0x4034, 0x00009000 }, /* NVDLA_CSC_D_WEIGHT_BYTES_0 */
    { 0x4024, 0x0000007f }, /* NVDLA_CSC_D_ENTRY_PER_SLICE_0 */
    { 0x4028, 0x00000000 }, /* NVDLA_CSC_D_WEIGHT_FORMAT_0 */
    { 0x4018, 0x0000007f }, /* NVDLA_CSC_D_DATAIN_SIZE_EXT_1_0 */
    { 0x4038, 0x008fa600 }, /* NVDLA_CSC_D_WMB_BYTES_0 */
    { 0x403c, 0x00100006 }, /* NVDLA_CSC_D_DATAOUT_SIZE_0_0 */
    { 0x4044, 0x00000076 }, /* NVDLA_CSC_D_ATOMICS_0 */
    { 0x405c, 0x0004000a }, /* NVDLA_CSC_D_BANK_0 */
    { 0x4030, 0x001f007f }, /* NVDLA_CSC_D_WEIGHT_SIZE_EXT_1_0 */
    { 0x404c, 0x00000000 }, /* NVDLA_CSC_D_CONV_STRIDE_EXT_0 */
    { 0x4000, 0x00000000 }, /* NVDLA_CSC_S_STATUS_0 */
    { 0x400c, 0x00000000 }, /* NVDLA_CSC_D_MISC_CFG_0 */
    { 0x4060, 0x00000002 }, /* NVDLA_CSC_D_PRA_CFG_0 */
    { 0x4050, 0x00000000 }, /* NVDLA_CSC_D_DILATION_EXT_0 */
    { 0x4014, 0x000f0007 }, /* NVDLA_CSC_D_DATAIN_SIZE_EXT_0_0 */
    { 0x4058, 0x00003b11 }, /* NVDLA_CSC_D_ZERO_PADDING_VALUE_0 */
    { 0x5004, 0x00000000 }, /* NVDLA_CMAC_A_S_POINTER_0 */
    { 0x500c, 0x00000000 }, /* NVDLA_CMAC_A_D_MISC_CFG_0 */
    { 0x5000, 0x00000000 }, /* NVDLA_CMAC_A_S_STATUS_0 */
    { 0x6004, 0x00000000 }, /* NVDLA_CMAC_B_S_POINTER_0 */
    { 0x6000, 0x00000000 }, /* NVDLA_CMAC_B_S_STATUS_0 */
    { 0x600c, 0x00000000 }, /* NVDLA_CMAC_B_D_MISC_CFG_0 */
    { 0x7004, 0x00000000 }, /* NVDLA_CACC_S_POINTER_0 */
    { 0x7010, 0x00100006 }, /* NVDLA_CACC_D_DATAOUT_SIZE_0_0 */
    { 0x702c, 0x00000000 }, /* NVDLA_CACC_D_CLIP_CFG_0 */
    { 0x7020, 0x00000040 }, /* NVDLA_CACC_D_LINE_STRIDE_0 */
    { 0x701c, 0x00000000 }, /* NVDLA_CACC_D_BATCH_NUMBER_0 */
    { 0x7028, 0x00000000 }, /* NVDLA_CACC_D_DATAOUT_MAP_0 */
    { 0x7030, 0x00000000 }, /* NVDLA_CACC_D_OUT_SATURATION_0 */
    { 0x7014, 0x0000001f }, /* NVDLA_CACC_D_DATAOUT_SIZE_1_0 */
    { 0x7018, 0xd570f320 }, /* NVDLA_CACC_D_DATAOUT_ADDR_0 */
    { 0x7000, 0x00000000 }, /* NVDLA_CACC_S_STATUS_0 */
    { 0x7034, 0xe067315b }, /* NVDLA_CACC_D_CYA_0 */
    { 0x7024, 0x00000840 }, /* NVDLA_CACC_D_SURF_STRIDE_0 */
    { 0x700c, 0x00000000 }, /* NVDLA_CACC_D_MISC_CFG_0 */
    {-1, 0}
};

static const struct dla_reg program_enable[] = {
    { 0x9038, 0x00000001 }, /* NVDLA_SDP_D_OP_ENABLE_0 */
    { 0x7008, 0x00000001 }, /* NVDLA_CACC_D_OP_ENABLE_0 */
    { 0x5008, 0x00000001 }, /* NVDLA_CMAC_A_D_OP_ENABLE_0 */
    { 0x6008, 0x00000001 }, /* NVDLA_CMAC_B_D_OP_ENABLE_0 */
    { 0x4008, 0x00000001 }, /* NVDLA_CSC_D_OP_ENABLE_0 */
    { 0x3010, 0x00000001 }, /* NVDLA_CDMA_D_OP_ENABLE_0 */
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

    printf("Waiting for CBUF flush...\n");
    volatile uint32_t* cbuf_status = (volatile uint32_t*)(dla_mmio + 0x300c);
    int loop = 1000000;
    while (--loop > 0) {
        if (*cbuf_status & 0x1) { printf("CBUF flush done\n"); break; }
    }
    if (loop <= 0) printf("CBUF flush timeout\n");

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
    { /* CRC[0]: addr=0xc0000120, size=0x2100, expect=0xea88655a */
        uint8_t* page = (uint8_t*)mmap(NULL, 12288, PROT_READ, MAP_SHARED, fd, 0xc0000000);
        if (page != MAP_FAILED) {
            uint32_t crc = calc_crc32(page + 288, 8448);
            printf("CRC[0] = 0x%08x (expect 0xea88655a) %s\n", crc, crc == 0xea88655a ? "PASS" : "FAIL");
            if (crc != 0xea88655a) failures++;
            munmap(page, 12288);
        } else { perror("mmap out"); }
    }

    printf("Results: %d failures\n", failures);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return failures;
}