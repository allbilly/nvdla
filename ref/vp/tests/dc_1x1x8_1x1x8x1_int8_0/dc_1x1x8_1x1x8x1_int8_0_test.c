#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>

#define DATA_BASE        0xc0000000
#define WEIGHT_BASE      0xd0000000
#define OUT_BASE         0xf0000000
#define NVDLA_MMIO_BASE  0x10200000
#define NVDLA_MMIO_SIZE  (0x10220000 - 0x10200000)

struct dla_reg {
    int32_t  offset;
    uint32_t value;
};

static const struct dla_reg program_initial[] = {
    { 0x1004, 0x003f03fc }, /* NVDLA_GLB_S_INTR_MASK_0 */
    { 0x100c, 0x00000000 }, /* NVDLA_GLB_S_INTR_STATUS_0 */
    { 0x9004, 0x00000000 }, /* NVDLA_SDP_S_POINTER_0 */
    { 0x904c, 0x00000000 }, /* NVDLA_SDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0x90e0, 0x00000000 }, /* NVDLA_SDP_D_PERF_WDMA_WRITE_STALL_0 */
    { 0x90e4, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_UFLOW_0 */
    { 0x9054, 0x00000008 }, /* NVDLA_SDP_D_DST_SURFACE_STRIDE_0 */
    { 0x90f0, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_HYBRID_0 */
    { 0x90ec, 0x00000000 }, /* NVDLA_SDP_D_PERF_OUT_SATURATION_0 */
    { 0x9000, 0x00000000 }, /* NVDLA_SDP_S_STATUS_0 */
    { 0x90f4, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_LE_HIT_0 */
    { 0x90a0, 0x8eb59288 }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_OFFSET_VALUE_0 */
    { 0x9084, 0x00000002 }, /* NVDLA_SDP_D_DP_EW_ALU_CFG_0 */
    { 0x9090, 0x0000b016 }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_SCALE_VALUE_0 */
    { 0x90ac, 0x00000033 }, /* NVDLA_SDP_D_DP_EW_TRUNCATE_VALUE_0 */
    { 0x9060, 0x0000ab7d }, /* NVDLA_SDP_D_DP_BS_ALU_SRC_VALUE_0 */
    { 0x90d0, 0x00000000 }, /* NVDLA_SDP_D_STATUS_NAN_INPUT_NUM_0 */
    { 0x906c, 0x0000006b }, /* NVDLA_SDP_D_DP_BN_CFG_0 */
    { 0x9064, 0x00002101 }, /* NVDLA_SDP_D_DP_BS_MUL_CFG_0 */
    { 0x90a8, 0x00000018 }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_TRUNCATE_VALUE_0 */
    { 0x90d8, 0x00000000 }, /* NVDLA_SDP_D_STATUS_NAN_OUTPUT_NUM_0 */
    { 0x90bc, 0x00000000 }, /* NVDLA_SDP_D_DATA_FORMAT_0 */
    { 0x9058, 0x0000000b }, /* NVDLA_SDP_D_DP_BS_CFG_0 */
    { 0x908c, 0x42139b55 }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_OFFSET_VALUE_0 */
    { 0x907c, 0x00007e67 }, /* NVDLA_SDP_D_DP_BN_MUL_SRC_VALUE_0 */
    { 0x90d4, 0x00000000 }, /* NVDLA_SDP_D_STATUS_INF_INPUT_NUM_0 */
    { 0x903c, 0x00000000 }, /* NVDLA_SDP_D_DATA_CUBE_WIDTH_0 */
    { 0x90e8, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_OFLOW_0 */
    { 0x90b8, 0xdf4cdbe0 }, /* NVDLA_SDP_D_DST_BATCH_STRIDE_0 */
    { 0x90c8, 0x00000021 }, /* NVDLA_SDP_D_CVT_SHIFT_0 */
    { 0x9088, 0x000097e0 }, /* NVDLA_SDP_D_DP_EW_ALU_SRC_VALUE_0 */
    { 0x9094, 0x00000035 }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_TRUNCATE_VALUE_0 */
    { 0x9050, 0x00000008 }, /* NVDLA_SDP_D_DST_LINE_STRIDE_0 */
    { 0x9098, 0x00000001 }, /* NVDLA_SDP_D_DP_EW_MUL_CFG_0 */
    { 0x909c, 0xabc443a9 }, /* NVDLA_SDP_D_DP_EW_MUL_SRC_VALUE_0 */
    { 0x905c, 0x00000e01 }, /* NVDLA_SDP_D_DP_BS_ALU_CFG_0 */
    { 0x90c0, 0x3dc324eb }, /* NVDLA_SDP_D_CVT_OFFSET_0 */
    { 0x90b4, 0x00000001 }, /* NVDLA_SDP_D_DST_DMA_CFG_0 */
    { 0x90c4, 0x0000a433 }, /* NVDLA_SDP_D_CVT_SCALE_0 */
    { 0x9070, 0x00001e00 }, /* NVDLA_SDP_D_DP_BN_ALU_CFG_0 */
    { 0x9048, 0xf0000000 }, /* NVDLA_SDP_D_DST_BASE_ADDR_LOW_0 */
    { 0x9080, 0x00000001 }, /* NVDLA_SDP_D_DP_EW_CFG_0 */
    { 0x9078, 0x00000401 }, /* NVDLA_SDP_D_DP_BN_MUL_CFG_0 */
    { 0x9044, 0x00000000 }, /* NVDLA_SDP_D_DATA_CUBE_CHANNEL_0 */
    { 0x90f8, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_LO_HIT_0 */
    { 0x90cc, 0x00000000 }, /* NVDLA_SDP_D_STATUS_0 */
    { 0x9068, 0x000059c1 }, /* NVDLA_SDP_D_DP_BS_MUL_SRC_VALUE_0 */
    { 0x90dc, 0x00000007 }, /* NVDLA_SDP_D_PERF_ENABLE_0 */
    { 0x9074, 0x00009430 }, /* NVDLA_SDP_D_DP_BN_ALU_SRC_VALUE_0 */
    { 0x9040, 0x00000000 }, /* NVDLA_SDP_D_DATA_CUBE_HEIGHT_0 */
    { 0x90a4, 0x000090b8 }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_SCALE_VALUE_0 */
    { 0x90b0, 0x00000009 }, /* NVDLA_SDP_D_FEATURE_MODE_CFG_0 */
    { 0x3004, 0x00000000 }, /* NVDLA_CDMA_S_POINTER_0 */
    { 0x30a4, 0x000000b0 }, /* NVDLA_CDMA_D_CVT_CFG_0 */
    { 0x30c4, 0x00000000 }, /* NVDLA_CDMA_D_NAN_INPUT_DATA_NUM_0 */
    { 0x30d0, 0x00000000 }, /* NVDLA_CDMA_D_INF_INPUT_WEIGHT_NUM_0 */
    { 0x30a8, 0x000036e4 }, /* NVDLA_CDMA_D_CVT_OFFSET_0 */
    { 0x3058, 0x00000000 }, /* NVDLA_CDMA_D_BATCH_NUMBER_0 */
    { 0x30c8, 0x00000000 }, /* NVDLA_CDMA_D_NAN_INPUT_WEIGHT_NUM_0 */
    { 0x30a0, 0x97392cf4 }, /* NVDLA_CDMA_D_MEAN_GLOBAL_1_0 */
    { 0x301c, 0x00000000 }, /* NVDLA_CDMA_D_DATAIN_SIZE_0_0 */
    { 0x304c, 0x00000000 }, /* NVDLA_CDMA_D_DAIN_MAP_0 */
    { 0x30dc, 0x00000000 }, /* NVDLA_CDMA_D_PERF_WT_READ_STALL_0 */
    { 0x3064, 0x00000000 }, /* NVDLA_CDMA_D_FETCH_GRAIN_0 */
    { 0x30cc, 0x00000000 }, /* NVDLA_CDMA_D_INF_INPUT_DATA_NUM_0 */
    { 0x3000, 0x00000000 }, /* NVDLA_CDMA_S_STATUS_0 */
    { 0x3098, 0x00000000 }, /* NVDLA_CDMA_D_MEAN_FORMAT_0 */
    { 0x3014, 0x10000000 }, /* NVDLA_CDMA_D_MISC_CFG_0 */
    { 0x303c, 0x62189ce0 }, /* NVDLA_CDMA_D_DAIN_ADDR_LOW_1_0 */
    { 0x3084, 0x00000079 }, /* NVDLA_CDMA_D_WGS_ADDR_HIGH_0 */
    { 0x30e8, 0xaaaadb06 }, /* NVDLA_CDMA_D_CYA_0 */
    { 0x30e4, 0x00000000 }, /* NVDLA_CDMA_D_PERF_WT_READ_LATENCY_0 */
    { 0x30e0, 0x00000000 }, /* NVDLA_CDMA_D_PERF_DAT_READ_LATENCY_0 */
    { 0x300c, 0x00000000 }, /* NVDLA_CDMA_S_CBUF_FLUSH_STATUS_0 */
    { 0x30d4, 0x00000000 }, /* NVDLA_CDMA_D_PERF_ENABLE_0 */
    { 0x3060, 0x00000000 }, /* NVDLA_CDMA_D_ENTRY_PER_SLICE_0 */
    { 0x309c, 0xfec724c9 }, /* NVDLA_CDMA_D_MEAN_GLOBAL_0_0 */
    { 0x3080, 0x00000080 }, /* NVDLA_CDMA_D_WEIGHT_BYTES_0 */
    { 0x3034, 0xc0000000 }, /* NVDLA_CDMA_D_DAIN_ADDR_LOW_0_0 */
    { 0x30b8, 0x0000d0c0 }, /* NVDLA_CDMA_D_ZERO_PADDING_VALUE_0 */
    { 0x3040, 0x000001c0 }, /* NVDLA_CDMA_D_LINE_STRIDE_0 */
    { 0x3028, 0x00020003 }, /* NVDLA_CDMA_D_PIXEL_OFFSET_0 */
    { 0x3094, 0x02399f80 }, /* NVDLA_CDMA_D_WMB_BYTES_0 */
    { 0x3074, 0x00000001 }, /* NVDLA_CDMA_D_WEIGHT_RAM_TYPE_0 */
    { 0x30ac, 0x0000f9fa }, /* NVDLA_CDMA_D_CVT_SCALE_0 */
    { 0x3048, 0x00002100 }, /* NVDLA_CDMA_D_SURF_STRIDE_0 */
    { 0x308c, 0x000000cc }, /* NVDLA_CDMA_D_WMB_ADDR_HIGH_0 */
    { 0x307c, 0xd0000000 }, /* NVDLA_CDMA_D_WEIGHT_ADDR_LOW_0 */
    { 0x3020, 0x00000007 }, /* NVDLA_CDMA_D_DATAIN_SIZE_1_0 */
    { 0x3090, 0x50b37f00 }, /* NVDLA_CDMA_D_WMB_ADDR_LOW_0 */
    { 0x3078, 0x00000000 }, /* NVDLA_CDMA_D_WEIGHT_ADDR_HIGH_0 */
    { 0x3008, 0x000b0007 }, /* NVDLA_CDMA_S_ARBITER_0 */
    { 0x30c0, 0x00000001 }, /* NVDLA_CDMA_D_NAN_FLUSH_TO_ZERO_0 */
    { 0x3038, 0x0000007c }, /* NVDLA_CDMA_D_DAIN_ADDR_HIGH_1_0 */
    { 0x3068, 0x00000000 }, /* NVDLA_CDMA_D_WEIGHT_FORMAT_0 */
    { 0x3024, 0x00000000 }, /* NVDLA_CDMA_D_DATAIN_SIZE_EXT_0_0 */
    { 0x302c, 0x00000001 }, /* NVDLA_CDMA_D_DAIN_RAM_TYPE_0 */
    { 0x306c, 0x00000007 }, /* NVDLA_CDMA_D_WEIGHT_SIZE_0_0 */
    { 0x3030, 0x00000000 }, /* NVDLA_CDMA_D_DAIN_ADDR_HIGH_0_0 */
    { 0x30d8, 0x00000000 }, /* NVDLA_CDMA_D_PERF_DAT_READ_STALL_0 */
    { 0x3018, 0x00000400 }, /* NVDLA_CDMA_D_DATAIN_FORMAT_0 */
    { 0x3088, 0x94654220 }, /* NVDLA_CDMA_D_WGS_ADDR_LOW_0 */
    { 0x30b0, 0x00010000 }, /* NVDLA_CDMA_D_CONV_STRIDE_0 */
    { 0x30bc, 0x00070006 }, /* NVDLA_CDMA_D_BANK_0 */
    { 0x3044, 0xf0e1fb40 }, /* NVDLA_CDMA_D_LINE_UV_STRIDE_0 */
    { 0x305c, 0x00000000 }, /* NVDLA_CDMA_D_BATCH_STRIDE_0 */
    { 0x3070, 0x00000000 }, /* NVDLA_CDMA_D_WEIGHT_SIZE_1_0 */
    { 0x30b4, 0x00000000 }, /* NVDLA_CDMA_D_ZERO_PADDING_0 */
    { 0x4004, 0x00000000 }, /* NVDLA_CSC_S_POINTER_0 */
    { 0x4048, 0x00000000 }, /* NVDLA_CSC_D_RELEASE_0 */
    { 0x403c, 0x00000000 }, /* NVDLA_CSC_D_DATAOUT_SIZE_0_0 */
    { 0x4020, 0x00000000 }, /* NVDLA_CSC_D_POST_Y_EXTENSION_0 */
    { 0x401c, 0x00000000 }, /* NVDLA_CSC_D_BATCH_NUMBER_0 */
    { 0x4034, 0x00000080 }, /* NVDLA_CSC_D_WEIGHT_BYTES_0 */
    { 0x402c, 0x00000000 }, /* NVDLA_CSC_D_WEIGHT_SIZE_EXT_0_0 */
    { 0x4040, 0x00000000 }, /* NVDLA_CSC_D_DATAOUT_SIZE_1_0 */
    { 0x4060, 0x00000001 }, /* NVDLA_CSC_D_PRA_CFG_0 */
    { 0x4014, 0x00000000 }, /* NVDLA_CSC_D_DATAIN_SIZE_EXT_0_0 */
    { 0x4000, 0x00000000 }, /* NVDLA_CSC_S_STATUS_0 */
    { 0x4064, 0xaaaadb06 }, /* NVDLA_CSC_D_CYA_0 */
    { 0x4024, 0x00000000 }, /* NVDLA_CSC_D_ENTRY_PER_SLICE_0 */
    { 0x4028, 0x00000000 }, /* NVDLA_CSC_D_WEIGHT_FORMAT_0 */
    { 0x405c, 0x00070006 }, /* NVDLA_CSC_D_BANK_0 */
    { 0x4044, 0x00000000 }, /* NVDLA_CSC_D_ATOMICS_0 */
    { 0x404c, 0x00010000 }, /* NVDLA_CSC_D_CONV_STRIDE_EXT_0 */
    { 0x4050, 0x000d0003 }, /* NVDLA_CSC_D_DILATION_EXT_0 */
    { 0x4054, 0x00000000 }, /* NVDLA_CSC_D_ZERO_PADDING_0 */
    { 0x4038, 0x02399f80 }, /* NVDLA_CSC_D_WMB_BYTES_0 */
    { 0x4058, 0x0000d0c0 }, /* NVDLA_CSC_D_ZERO_PADDING_VALUE_0 */
    { 0x4018, 0x00000007 }, /* NVDLA_CSC_D_DATAIN_SIZE_EXT_1_0 */
    { 0x4030, 0x00000007 }, /* NVDLA_CSC_D_WEIGHT_SIZE_EXT_1_0 */
    { 0x4010, 0x00000000 }, /* NVDLA_CSC_D_DATAIN_FORMAT_0 */
    { 0x400c, 0x10000000 }, /* NVDLA_CSC_D_MISC_CFG_0 */
    { 0x5004, 0x00000000 }, /* NVDLA_CMAC_A_S_POINTER_0 */
    { 0x500c, 0x00000000 }, /* NVDLA_CMAC_A_D_MISC_CFG_0 */
    { 0x5000, 0x00000000 }, /* NVDLA_CMAC_A_S_STATUS_0 */
    { 0x6004, 0x00000000 }, /* NVDLA_CMAC_B_S_POINTER_0 */
    { 0x6000, 0x00000000 }, /* NVDLA_CMAC_B_S_STATUS_0 */
    { 0x600c, 0x00000000 }, /* NVDLA_CMAC_B_D_MISC_CFG_0 */
    { 0x7004, 0x00000000 }, /* NVDLA_CACC_S_POINTER_0 */
    { 0x702c, 0x00000006 }, /* NVDLA_CACC_D_CLIP_CFG_0 */
    { 0x7018, 0x0f5ac2e0 }, /* NVDLA_CACC_D_DATAOUT_ADDR_0 */
    { 0x700c, 0x00000000 }, /* NVDLA_CACC_D_MISC_CFG_0 */
    { 0x7000, 0x00000000 }, /* NVDLA_CACC_S_STATUS_0 */
    { 0x7010, 0x00000000 }, /* NVDLA_CACC_D_DATAOUT_SIZE_0_0 */
    { 0x7034, 0xaaaadb06 }, /* NVDLA_CACC_D_CYA_0 */
    { 0x7020, 0x00000020 }, /* NVDLA_CACC_D_LINE_STRIDE_0 */
    { 0x7028, 0x00010001 }, /* NVDLA_CACC_D_DATAOUT_MAP_0 */
    { 0x7024, 0x00000020 }, /* NVDLA_CACC_D_SURF_STRIDE_0 */
    { 0x7030, 0x00000000 }, /* NVDLA_CACC_D_OUT_SATURATION_0 */
    { 0x701c, 0x00000000 }, /* NVDLA_CACC_D_BATCH_NUMBER_0 */
    { 0x7014, 0x00000000 }, /* NVDLA_CACC_D_DATAOUT_SIZE_1_0 */
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

#define CBUF_FLUSH_STATUS_OFFSET 0x300c
#define INTR_STATUS_OFFSET       0x100c
#define SDP_DONE_SHIFT            0

int main(int argc, char* argv[])
{
    int fd = open("/dev/mem", O_RDWR);
    if (fd < 0) { perror("open /dev/mem"); return 1; }
    uint8_t* dla_mmio = (uint8_t*)mmap(NULL, NVDLA_MMIO_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, NVDLA_MMIO_BASE);
    if (dla_mmio == MAP_FAILED) { perror("mmap dla"); return 1; }

    printf("Programming registers...\n");
    const struct dla_reg* p = program_initial;
    while (p->offset != -1) {
        *(volatile uint32_t*)(dla_mmio + p->offset) = p->value;
        p++;
    }
    printf("Done programming initial registers.\n");

    volatile uint32_t* cbuf_status = (volatile uint32_t*)(dla_mmio + CBUF_FLUSH_STATUS_OFFSET);
    int loop = 1000000;
    while (--loop > 0) {
        if (*cbuf_status & 0x1) {
            printf("CDMA -> CBUF flush done\n");
            break;
        }
    }
    if (loop <= 0) { printf("CBUF flush timeout\n"); }

    printf("Firing OP_ENABLE...\n");
    p = program_enable;
    while (p->offset != -1) {
        *(volatile uint32_t*)(dla_mmio + p->offset) = p->value;
        p++;
    }

    volatile uint32_t* intr_status = (volatile uint32_t*)(dla_mmio + INTR_STATUS_OFFSET);
    loop = 100000000;
    while (--loop > 0) {
        if ((*intr_status >> SDP_DONE_SHIFT) & 1) {
            printf("SDP done interrupt received!\n");
            break;
        }
    }
    if (loop <= 0) { printf("SDP timeout\n"); return 1; }

    /* Read and dump output from VP DRAM via mmap */
    printf("Reading output at OUT_BASE=0x%x...\n", OUT_BASE);
    uint32_t* out_mem = (uint32_t*)mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, OUT_BASE);
    if (out_mem == MAP_FAILED) { perror("mmap output"); return 1; }
    FILE* fp = fopen("sdp2mcif_output.dat", "w");
    if (!fp) { perror("fopen"); return 1; }
    for (int i = 0; i < 2; i++) {
        fprintf(fp, "%08x\n", out_mem[i]);
    }
    fclose(fp);
    printf("Output written to sdp2mcif_output.dat\n");

    munmap(out_mem, 4096);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return 0;
}