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
    { 0x9004, 0x00000000 }, /* NVDLA_SDP_S_POINTER_0 */
    { 0x90f0, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_HYBRID_0 */
    { 0x90e8, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_OFLOW_0 */
    { 0x90d0, 0x00000000 }, /* NVDLA_SDP_D_STATUS_NAN_INPUT_NUM_0 */
    { 0x9018, 0x0000007f }, /* NVDLA_SDP_S_LUT_LE_START_0 */
    { 0x9044, 0x00000020 }, /* NVDLA_SDP_D_DATA_CUBE_CHANNEL_0 */
    { 0x90b4, 0x00000001 }, /* NVDLA_SDP_D_DST_DMA_CFG_0 */
    { 0x9054, 0x00000300 }, /* NVDLA_SDP_D_DST_SURFACE_STRIDE_0 */
    { 0x9074, 0xbc470fb3 }, /* NVDLA_SDP_D_DP_BN_ALU_SRC_VALUE_0 */
    { 0x90c0, 0x00000000 }, /* NVDLA_SDP_D_CVT_OFFSET_0 */
    { 0x90e0, 0x00000000 }, /* NVDLA_SDP_D_PERF_WDMA_WRITE_STALL_0 */
    { 0x90b8, 0x00000000 }, /* NVDLA_SDP_D_DST_BATCH_STRIDE_0 */
    { 0x9058, 0x00000019 }, /* NVDLA_SDP_D_DP_BS_CFG_0 */
    { 0x9050, 0x00000100 }, /* NVDLA_SDP_D_DST_LINE_STRIDE_0 */
    { 0x90c8, 0x00000000 }, /* NVDLA_SDP_D_CVT_SHIFT_0 */
    { 0x90a4, 0x0000ef05 }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_SCALE_VALUE_0 */
    { 0x90a8, 0x000001ca }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_TRUNCATE_VALUE_0 */
    { 0x90f4, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_LE_HIT_0 */
    { 0x901c, 0x00000081 }, /* NVDLA_SDP_S_LUT_LE_END_0 */
    { 0x904c, 0x00000000 }, /* NVDLA_SDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0x9040, 0x00000002 }, /* NVDLA_SDP_D_DATA_CUBE_HEIGHT_0 */
    { 0x90b0, 0x00000000 }, /* NVDLA_SDP_D_FEATURE_MODE_CFG_0 */
    { 0x9000, 0x00000000 }, /* NVDLA_SDP_S_STATUS_0 */
    { 0x9064, 0x00002f00 }, /* NVDLA_SDP_D_DP_BS_MUL_CFG_0 */
    { 0x90bc, 0x00000000 }, /* NVDLA_SDP_D_DATA_FORMAT_0 */
    { 0x9048, 0xd00001c0 }, /* NVDLA_SDP_D_DST_BASE_ADDR_LOW_0 */
    { 0x90c4, 0x00000001 }, /* NVDLA_SDP_D_CVT_SCALE_0 */
    { 0x905c, 0x00001801 }, /* NVDLA_SDP_D_DP_BS_ALU_CFG_0 */
    { 0x9078, 0x00003201 }, /* NVDLA_SDP_D_DP_BN_MUL_CFG_0 */
    { 0x90ac, 0x00000000 }, /* NVDLA_SDP_D_DP_EW_TRUNCATE_VALUE_0 */
    { 0x90dc, 0x0000001d }, /* NVDLA_SDP_D_PERF_ENABLE_0 */
    { 0x90cc, 0x00000000 }, /* NVDLA_SDP_D_STATUS_0 */
    { 0x90ec, 0x00000000 }, /* NVDLA_SDP_D_PERF_OUT_SATURATION_0 */
    { 0x9084, 0x00000002 }, /* NVDLA_SDP_D_DP_EW_ALU_CFG_0 */
    { 0x90e4, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_UFLOW_0 */
    { 0x907c, 0x4cf7b232 }, /* NVDLA_SDP_D_DP_BN_MUL_SRC_VALUE_0 */
    { 0x9060, 0x916a1367 }, /* NVDLA_SDP_D_DP_BS_ALU_SRC_VALUE_0 */
    { 0x90d8, 0x00000000 }, /* NVDLA_SDP_D_STATUS_NAN_OUTPUT_NUM_0 */
    { 0x9068, 0x000065e7 }, /* NVDLA_SDP_D_DP_BS_MUL_SRC_VALUE_0 */
    { 0x90a0, 0x718f2f05 }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_OFFSET_VALUE_0 */
    { 0x9094, 0x00000001 }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_TRUNCATE_VALUE_0 */
    { 0x909c, 0x0000a2cf }, /* NVDLA_SDP_D_DP_EW_MUL_SRC_VALUE_0 */
    { 0x906c, 0x0000006b }, /* NVDLA_SDP_D_DP_BN_CFG_0 */
    { 0x9098, 0x00000000 }, /* NVDLA_SDP_D_DP_EW_MUL_CFG_0 */
    { 0x9070, 0x00003601 }, /* NVDLA_SDP_D_DP_BN_ALU_CFG_0 */
    { 0x90f8, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_LO_HIT_0 */
    { 0x908c, 0xce5d7e53 }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_OFFSET_VALUE_0 */
    { 0x90d4, 0x00000000 }, /* NVDLA_SDP_D_STATUS_INF_INPUT_NUM_0 */
    { 0x9090, 0x00000647 }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_SCALE_VALUE_0 */
    { 0x9080, 0x0000001a }, /* NVDLA_SDP_D_DP_EW_CFG_0 */
    { 0x9088, 0x0000e3e4 }, /* NVDLA_SDP_D_DP_EW_ALU_SRC_VALUE_0 */
    { 0x903c, 0x00000002 }, /* NVDLA_SDP_D_DATA_CUBE_WIDTH_0 */
    { 0x9008, 0x00020000 }, /* NVDLA_SDP_S_LUT_ACCESS_CFG_0 */
    { 0x900c, 0x00000000 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000001 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000002 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000003 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000004 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000005 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000006 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000007 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000008 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000009 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000000a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000000b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000000c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000000d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000000e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000000f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000010 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000011 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000012 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000013 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000014 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000015 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000016 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000017 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000018 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000019 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000001a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000001b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000001c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000001d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000001e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000001f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000020 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000021 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000022 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000023 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000024 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000025 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000026 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000027 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000028 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000029 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000002a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000002b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000002c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000002d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000002e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000002f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000030 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000031 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000032 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000033 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000034 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000035 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000036 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000037 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000038 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000039 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000003a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000003b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000003c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000003d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000003e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000003f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000040 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x9008, 0x00030000 }, /* NVDLA_SDP_S_LUT_ACCESS_CFG_0 */
    { 0x900c, 0x00000000 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000001 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000002 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000003 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000004 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000005 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000006 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000007 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000008 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000009 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000000a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000000b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000000c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000000d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000000e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000000f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000010 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000011 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000012 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000013 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000014 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000015 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000016 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000017 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000018 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000019 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000001a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000001b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000001c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000001d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000001e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000001f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000020 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000021 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000022 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000023 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000024 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000025 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000026 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000027 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000028 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000029 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000002a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000002b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000002c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000002d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000002e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000002f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000030 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000031 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000032 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000033 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000034 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000035 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000036 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000037 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000038 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000039 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000003a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000003b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000003c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000003d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000003e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000003f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000040 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000041 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000042 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000043 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000044 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000045 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000046 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000047 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000048 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000049 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000004a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000004b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000004c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000004d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000004e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000004f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000050 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000051 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000052 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000053 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000054 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000055 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000056 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000057 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000058 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000059 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000005a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000005b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000005c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000005d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000005e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000005f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000060 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000061 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000062 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000063 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000064 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000065 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000066 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000067 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000068 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000069 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000006a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000006b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000006c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000006d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000006e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000006f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000070 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000071 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000072 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000073 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000074 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000075 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000076 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000077 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000078 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000079 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000007a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000007b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000007c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000007d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000007e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000007f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000080 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000081 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000082 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000083 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000084 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000085 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000086 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000087 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000088 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000089 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000008a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000008b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000008c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000008d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000008e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000008f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000090 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000091 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000092 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000093 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000094 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000095 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000096 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000097 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000098 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000099 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000009a }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000009b }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000009c }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000009d }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000009e }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x0000009f }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000a0 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000a1 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000a2 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000a3 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000a4 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000a5 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000a6 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000a7 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000a8 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000a9 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000aa }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000ab }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000ac }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000ad }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000ae }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000af }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000b0 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000b1 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000b2 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000b3 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000b4 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000b5 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000b6 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000b7 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000b8 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000b9 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000ba }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000bb }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000bc }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000bd }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000be }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000bf }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000c0 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000c1 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000c2 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000c3 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000c4 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000c5 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000c6 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000c7 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000c8 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000c9 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000ca }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000cb }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000cc }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000cd }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000ce }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000cf }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000d0 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000d1 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000d2 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000d3 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000d4 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000d5 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000d6 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000d7 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000d8 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000d9 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000da }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000db }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000dc }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000dd }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000de }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000df }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000e0 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000e1 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000e2 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000e3 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000e4 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000e5 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000e6 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000e7 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000e8 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000e9 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000ea }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000eb }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000ec }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000ed }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000ee }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000ef }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000f0 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000f1 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000f2 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000f3 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000f4 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000f5 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000f6 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000f7 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000f8 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000f9 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000fa }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000fb }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000fc }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000fd }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000fe }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x000000ff }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x900c, 0x00000100 }, /* NVDLA_SDP_S_LUT_ACCESS_DATA_0 */
    { 0x9010, 0x00000001 }, /* NVDLA_SDP_S_LUT_CFG_0 */
    { 0x9014, 0x0001fb00 }, /* NVDLA_SDP_S_LUT_INFO_0 */
    { 0x9020, 0x53b19eb3 }, /* NVDLA_SDP_S_LUT_LO_START_0 */
    { 0x9024, 0x53b1a0b3 }, /* NVDLA_SDP_S_LUT_LO_END_0 */
    { 0x9028, 0x116c0e83 }, /* NVDLA_SDP_S_LUT_LE_SLOPE_SCALE_0 */
    { 0x902c, 0x00000129 }, /* NVDLA_SDP_S_LUT_LE_SLOPE_SHIFT_0 */
    { 0x9030, 0x7f5c12b3 }, /* NVDLA_SDP_S_LUT_LO_SLOPE_SCALE_0 */
    { 0x9034, 0x00000396 }, /* NVDLA_SDP_S_LUT_LO_SLOPE_SHIFT_0 */
    { 0x8004, 0x00000000 }, /* NVDLA_SDP_RDMA_S_POINTER_0 */
    { 0x8038, 0x00000020 }, /* NVDLA_SDP_RDMA_D_BS_SURFACE_STRIDE_0 */
    { 0x8068, 0x00000080 }, /* NVDLA_SDP_RDMA_D_EW_SURFACE_STRIDE_0 */
    { 0x800c, 0x00000002 }, /* NVDLA_SDP_RDMA_D_DATA_CUBE_WIDTH_0 */
    { 0x802c, 0xdeb64600 }, /* NVDLA_SDP_RDMA_D_BS_BASE_ADDR_LOW_0 */
    { 0x8054, 0x02c870a0 }, /* NVDLA_SDP_RDMA_D_BN_BATCH_STRIDE_0 */
    { 0x801c, 0x00000000 }, /* NVDLA_SDP_RDMA_D_SRC_BASE_ADDR_HIGH_0 */
    { 0x8018, 0xc0000100 }, /* NVDLA_SDP_RDMA_D_SRC_BASE_ADDR_LOW_0 */
    { 0x8044, 0xa98792a0 }, /* NVDLA_SDP_RDMA_D_BN_BASE_ADDR_LOW_0 */
    { 0x8060, 0x000000a8 }, /* NVDLA_SDP_RDMA_D_EW_BASE_ADDR_HIGH_0 */
    { 0x8040, 0x00000001 }, /* NVDLA_SDP_RDMA_D_NRDMA_CFG_0 */
    { 0x8020, 0x00000080 }, /* NVDLA_SDP_RDMA_D_SRC_LINE_STRIDE_0 */
    { 0x8034, 0x00000020 }, /* NVDLA_SDP_RDMA_D_BS_LINE_STRIDE_0 */
    { 0x8050, 0x00000020 }, /* NVDLA_SDP_RDMA_D_BN_SURFACE_STRIDE_0 */
    { 0x8000, 0x00000000 }, /* NVDLA_SDP_RDMA_S_STATUS_0 */
    { 0x8080, 0x00000001 }, /* NVDLA_SDP_RDMA_D_PERF_ENABLE_0 */
    { 0x8024, 0x00000200 }, /* NVDLA_SDP_RDMA_D_SRC_SURFACE_STRIDE_0 */
    { 0x8010, 0x00000002 }, /* NVDLA_SDP_RDMA_D_DATA_CUBE_HEIGHT_0 */
    { 0x805c, 0x9d42a4a0 }, /* NVDLA_SDP_RDMA_D_EW_BASE_ADDR_LOW_0 */
    { 0x8074, 0x00000001 }, /* NVDLA_SDP_RDMA_D_SRC_DMA_CFG_0 */
    { 0x8088, 0x00000000 }, /* NVDLA_SDP_RDMA_D_PERF_BRDMA_READ_STALL_0 */
    { 0x8048, 0x0000001a }, /* NVDLA_SDP_RDMA_D_BN_BASE_ADDR_HIGH_0 */
    { 0x8028, 0x00000001 }, /* NVDLA_SDP_RDMA_D_BRDMA_CFG_0 */
    { 0x808c, 0x00000000 }, /* NVDLA_SDP_RDMA_D_PERF_NRDMA_READ_STALL_0 */
    { 0x8030, 0x0000009e }, /* NVDLA_SDP_RDMA_D_BS_BASE_ADDR_HIGH_0 */
    { 0x8058, 0x0000000d }, /* NVDLA_SDP_RDMA_D_ERDMA_CFG_0 */
    { 0x8084, 0x00000000 }, /* NVDLA_SDP_RDMA_D_PERF_MRDMA_READ_STALL_0 */
    { 0x8014, 0x00000020 }, /* NVDLA_SDP_RDMA_D_DATA_CUBE_CHANNEL_0 */
    { 0x8090, 0x00000000 }, /* NVDLA_SDP_RDMA_D_PERF_ERDMA_READ_STALL_0 */
    { 0x8070, 0x00000000 }, /* NVDLA_SDP_RDMA_D_FEATURE_MODE_CFG_0 */
    { 0x806c, 0xb5237660 }, /* NVDLA_SDP_RDMA_D_EW_BATCH_STRIDE_0 */
    { 0x8064, 0x00000080 }, /* NVDLA_SDP_RDMA_D_EW_LINE_STRIDE_0 */
    { 0x803c, 0xec38dfa0 }, /* NVDLA_SDP_RDMA_D_BS_BATCH_STRIDE_0 */
    { 0x804c, 0x00000020 }, /* NVDLA_SDP_RDMA_D_BN_LINE_STRIDE_0 */
    { 0x9038, 0x00000001 }, /* NVDLA_SDP_D_OP_ENABLE_0 */
    { 0x8008, 0x00000001 }, /* NVDLA_SDP_RDMA_D_OP_ENABLE_0 */
    {-1, 0}
};

static const struct dla_reg program_enable[] = {
    {-1, 0}
};

/* Embedded data blocks (loaded via mmap instead of devmem scripts) */
static const uint32_t embed_0_data[][2] = {
    { 0x00000000, 0xd519eba7 },
    { 0x00000004, 0xac42b1e8 },
    { 0x00000008, 0x1bc4eb07 },
    { 0x0000000c, 0x0d5d7f23 },
    { 0x00000010, 0x251aee94 },
    { 0x00000014, 0xe8d6c224 },
    { 0x00000080, 0x2baeee55 },
    { 0x00000084, 0xfcb0a17d },
    { 0x00000088, 0x4a4ec3e0 },
    { 0x0000008c, 0xc1b55e96 },
    { 0x00000090, 0x33735bfe },
    { 0x00000094, 0x2c2b2256 },
    { 0x00000100, 0x668fff47 },
    { 0x00000104, 0xa1fae8da },
    { 0x00000108, 0x81bba948 },
    { 0x0000010c, 0x7c6e5c96 },
    { 0x00000110, 0x80f7efe9 },
    { 0x00000114, 0x81852950 },
    { 0x00000200, 0xd70d4846 },
    { 0x00000204, 0xfb8d5efe },
    { 0x00000208, 0x9a64e1bf },
    { 0x0000020c, 0x489aad8b },
    { 0x00000210, 0x2ad8ef8f },
    { 0x00000214, 0x50d3a822 },
    { 0x00000280, 0x5d0a23b0 },
    { 0x00000284, 0x5cc3c661 },
    { 0x00000288, 0x52860b8e },
    { 0x0000028c, 0xe3745eae },
    { 0x00000290, 0x5b02b9a8 },
    { 0x00000294, 0x496dcb7a },
    { 0x00000300, 0x5abdf423 },
    { 0x00000304, 0xbcf1b38b },
    { 0x00000308, 0x4c16b48e },
    { 0x0000030c, 0xd770e6e1 },
    { 0x00000310, 0x42623a81 },
    { 0x00000314, 0xad4b0af8 },
    { 0x00000400, 0xaa992488 },
    { 0x00000404, 0x4ad3c790 },
    { 0x00000408, 0x99735b45 },
    { 0x0000040c, 0x970ff38f },
    { 0x00000410, 0x932feea1 },
    { 0x00000414, 0x095ac6d9 },
    { 0x00000480, 0xe77a62b2 },
    { 0x00000484, 0x664c53bb },
    { 0x00000488, 0xcc23972e },
    { 0x0000048c, 0x9ba569a6 },
    { 0x00000490, 0x76b46a07 },
    { 0x00000494, 0xc7950c62 },
    { 0x00000500, 0xeee143da },
    { 0x00000504, 0xc9e4f544 },
    { 0x00000508, 0x5af71193 },
    { 0x0000050c, 0xeaa7e46c },
    { 0x00000510, 0xdd8101e6 },
    { 0x00000514, 0xf1c05418 },
    { 0x00000600, 0xb0684f74 },
    { 0x00000604, 0x78e3e093 },
    { 0x00000608, 0x5df86d02 },
    { 0x0000060c, 0x1df5b299 },
    { 0x00000610, 0x30a19b8d },
    { 0x00000614, 0x9b0a2e29 },
    { 0x00000680, 0xc91723d0 },
    { 0x00000684, 0xb848c66f },
    { 0x00000688, 0xf62041e3 },
    { 0x0000068c, 0xb5f47b76 },
    { 0x00000690, 0xfe1d9612 },
    { 0x00000694, 0x391e2d12 },
    { 0x00000700, 0x4e118e1d },
    { 0x00000704, 0x0321ca71 },
    { 0x00000708, 0x2df0eef1 },
    { 0x0000070c, 0xd9267761 },
    { 0x00000710, 0x79ef2dd8 },
    { 0x00000714, 0x82eb06cc },
    { 0x00000800, 0x0000005b },
    { 0x00000808, 0x0000008b },
    { 0x00000810, 0x00000035 },
    { 0x00000880, 0x0000002b },
    { 0x00000888, 0x00000011 },
    { 0x00000890, 0x0000004b },
    { 0x00000900, 0x000000b9 },
    { 0x00000908, 0x00000026 },
    { 0x00000910, 0x0000003e },
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

    { /* Zero-init region 0xc0000100 size 0xf00 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 256, 0, 3840); munmap(zpage, 4096); }
    }
    { /* Zero-init region 0xd00001c0 size 0xf00 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xd0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 448, 0, 3840); munmap(zpage, 8192); }
    }

    /* Load embedded data (replaces devmem load scripts) */
    { /* Data: base=0xc0000100, 81 writes */
        uint8_t* dpage = (uint8_t*)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (dpage != MAP_FAILED) {
            const uint32_t (*dp)[2] = embed_0_data;
            uint32_t base_off = 0x100;
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
    { /* CRC[0] zero-init: addr=0xd00001c0 size=0xf00 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xd0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 448, 0, 3840); munmap(zpage, 8192); }
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
    { /* CRC[0]: addr=0xd00001c0, size=0xf00, expect=0x4dd84f2e */
        uint8_t* page = (uint8_t*)mmap(NULL, 8192, PROT_READ, MAP_SHARED, fd, 0xd0000000);
        if (page != MAP_FAILED) {
            uint32_t crc = calc_crc32(page + 448, 3840);
            printf("CRC[0] = 0x%08x (expect 0x4dd84f2e) %s\n", crc, crc == 0x4dd84f2e ? "PASS" : "FAIL");
            if (crc != 0x4dd84f2e) failures++;
            munmap(page, 8192);
        } else { perror("mmap out"); }
    }

    printf("Results: %d failures\n", failures);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return failures;
}