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
    { 0x9054, 0x00000400 }, /* NVDLA_SDP_D_DST_SURFACE_STRIDE_0 */
    { 0x90f4, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_LE_HIT_0 */
    { 0x9070, 0x00000101 }, /* NVDLA_SDP_D_DP_BN_ALU_CFG_0 */
    { 0x904c, 0x00000000 }, /* NVDLA_SDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0x90d0, 0x00000000 }, /* NVDLA_SDP_D_STATUS_NAN_INPUT_NUM_0 */
    { 0x90f8, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_LO_HIT_0 */
    { 0x9064, 0x00002200 }, /* NVDLA_SDP_D_DP_BS_MUL_CFG_0 */
    { 0x90f0, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_HYBRID_0 */
    { 0x90d4, 0x00000000 }, /* NVDLA_SDP_D_STATUS_INF_INPUT_NUM_0 */
    { 0x9080, 0x0000002f }, /* NVDLA_SDP_D_DP_EW_CFG_0 */
    { 0x9060, 0x0000292a }, /* NVDLA_SDP_D_DP_BS_ALU_SRC_VALUE_0 */
    { 0x9044, 0x00000020 }, /* NVDLA_SDP_D_DATA_CUBE_CHANNEL_0 */
    { 0x9058, 0x0000006b }, /* NVDLA_SDP_D_DP_BS_CFG_0 */
    { 0x90e4, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_UFLOW_0 */
    { 0x90b8, 0x00000000 }, /* NVDLA_SDP_D_DST_BATCH_STRIDE_0 */
    { 0x90a4, 0x0000f46b }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_SCALE_VALUE_0 */
    { 0x90ac, 0x0000002f }, /* NVDLA_SDP_D_DP_EW_TRUNCATE_VALUE_0 */
    { 0x90c4, 0x00000001 }, /* NVDLA_SDP_D_CVT_SCALE_0 */
    { 0x909c, 0x184675f8 }, /* NVDLA_SDP_D_DP_EW_MUL_SRC_VALUE_0 */
    { 0x90e8, 0x00000000 }, /* NVDLA_SDP_D_PERF_LUT_OFLOW_0 */
    { 0x9068, 0x000023d6 }, /* NVDLA_SDP_D_DP_BS_MUL_SRC_VALUE_0 */
    { 0x90c0, 0x00000000 }, /* NVDLA_SDP_D_CVT_OFFSET_0 */
    { 0x90d8, 0x00000000 }, /* NVDLA_SDP_D_STATUS_NAN_OUTPUT_NUM_0 */
    { 0x907c, 0x00000001 }, /* NVDLA_SDP_D_DP_BN_MUL_SRC_VALUE_0 */
    { 0x9048, 0xf0000040 }, /* NVDLA_SDP_D_DST_BASE_ADDR_LOW_0 */
    { 0x90ec, 0x00000000 }, /* NVDLA_SDP_D_PERF_OUT_SATURATION_0 */
    { 0x9094, 0x00000023 }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_TRUNCATE_VALUE_0 */
    { 0x90bc, 0x00000000 }, /* NVDLA_SDP_D_DATA_FORMAT_0 */
    { 0x90a0, 0xf35aa3e8 }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_OFFSET_VALUE_0 */
    { 0x905c, 0x00001801 }, /* NVDLA_SDP_D_DP_BS_ALU_CFG_0 */
    { 0x9040, 0x00000002 }, /* NVDLA_SDP_D_DATA_CUBE_HEIGHT_0 */
    { 0x9090, 0x0000d1c8 }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_SCALE_VALUE_0 */
    { 0x9078, 0x00000101 }, /* NVDLA_SDP_D_DP_BN_MUL_CFG_0 */
    { 0x9050, 0x00000100 }, /* NVDLA_SDP_D_DST_LINE_STRIDE_0 */
    { 0x903c, 0x00000002 }, /* NVDLA_SDP_D_DATA_CUBE_WIDTH_0 */
    { 0x90c8, 0x00000000 }, /* NVDLA_SDP_D_CVT_SHIFT_0 */
    { 0x90a8, 0x00000012 }, /* NVDLA_SDP_D_DP_EW_MUL_CVT_TRUNCATE_VALUE_0 */
    { 0x90dc, 0x00000005 }, /* NVDLA_SDP_D_PERF_ENABLE_0 */
    { 0x90b0, 0x00000000 }, /* NVDLA_SDP_D_FEATURE_MODE_CFG_0 */
    { 0x9084, 0x00000003 }, /* NVDLA_SDP_D_DP_EW_ALU_CFG_0 */
    { 0x90cc, 0x00000000 }, /* NVDLA_SDP_D_STATUS_0 */
    { 0x9000, 0x00000000 }, /* NVDLA_SDP_S_STATUS_0 */
    { 0x9098, 0x00000001 }, /* NVDLA_SDP_D_DP_EW_MUL_CFG_0 */
    { 0x90b4, 0x00000001 }, /* NVDLA_SDP_D_DST_DMA_CFG_0 */
    { 0x9074, 0x00000001 }, /* NVDLA_SDP_D_DP_BN_ALU_SRC_VALUE_0 */
    { 0x906c, 0x00000018 }, /* NVDLA_SDP_D_DP_BN_CFG_0 */
    { 0x908c, 0x4bdddaa4 }, /* NVDLA_SDP_D_DP_EW_ALU_CVT_OFFSET_VALUE_0 */
    { 0x9088, 0x3c51a9ad }, /* NVDLA_SDP_D_DP_EW_ALU_SRC_VALUE_0 */
    { 0x90e0, 0x00000000 }, /* NVDLA_SDP_D_PERF_WDMA_WRITE_STALL_0 */
    { 0x8004, 0x00000000 }, /* NVDLA_SDP_RDMA_S_POINTER_0 */
    { 0x8068, 0x00000a00 }, /* NVDLA_SDP_RDMA_D_EW_SURFACE_STRIDE_0 */
    { 0x8050, 0x00000200 }, /* NVDLA_SDP_RDMA_D_BN_SURFACE_STRIDE_0 */
    { 0x803c, 0x1b0a3600 }, /* NVDLA_SDP_RDMA_D_BS_BATCH_STRIDE_0 */
    { 0x8044, 0xd0000c20 }, /* NVDLA_SDP_RDMA_D_BN_BASE_ADDR_LOW_0 */
    { 0x8048, 0x00000000 }, /* NVDLA_SDP_RDMA_D_BN_BASE_ADDR_HIGH_0 */
    { 0x8078, 0x00000000 }, /* NVDLA_SDP_RDMA_D_STATUS_NAN_INPUT_NUM_0 */
    { 0x8080, 0x00000000 }, /* NVDLA_SDP_RDMA_D_PERF_ENABLE_0 */
    { 0x8018, 0xc0002000 }, /* NVDLA_SDP_RDMA_D_SRC_BASE_ADDR_LOW_0 */
    { 0x8058, 0x0000001d }, /* NVDLA_SDP_RDMA_D_ERDMA_CFG_0 */
    { 0x804c, 0x00000080 }, /* NVDLA_SDP_RDMA_D_BN_LINE_STRIDE_0 */
    { 0x8040, 0x00000032 }, /* NVDLA_SDP_RDMA_D_NRDMA_CFG_0 */
    { 0x805c, 0xfa70ff60 }, /* NVDLA_SDP_RDMA_D_EW_BASE_ADDR_LOW_0 */
    { 0x8064, 0x00000300 }, /* NVDLA_SDP_RDMA_D_EW_LINE_STRIDE_0 */
    { 0x8014, 0x00000020 }, /* NVDLA_SDP_RDMA_D_DATA_CUBE_CHANNEL_0 */
    { 0x8088, 0x00000000 }, /* NVDLA_SDP_RDMA_D_PERF_BRDMA_READ_STALL_0 */
    { 0x8038, 0x000007e0 }, /* NVDLA_SDP_RDMA_D_BS_SURFACE_STRIDE_0 */
    { 0x8060, 0x00000071 }, /* NVDLA_SDP_RDMA_D_EW_BASE_ADDR_HIGH_0 */
    { 0x8084, 0x00000000 }, /* NVDLA_SDP_RDMA_D_PERF_MRDMA_READ_STALL_0 */
    { 0x8024, 0x00000200 }, /* NVDLA_SDP_RDMA_D_SRC_SURFACE_STRIDE_0 */
    { 0x802c, 0x72c83760 }, /* NVDLA_SDP_RDMA_D_BS_BASE_ADDR_LOW_0 */
    { 0x8054, 0x2186d8a0 }, /* NVDLA_SDP_RDMA_D_BN_BATCH_STRIDE_0 */
    { 0x8028, 0x0000003b }, /* NVDLA_SDP_RDMA_D_BRDMA_CFG_0 */
    { 0x801c, 0x00000000 }, /* NVDLA_SDP_RDMA_D_SRC_BASE_ADDR_HIGH_0 */
    { 0x8010, 0x00000002 }, /* NVDLA_SDP_RDMA_D_DATA_CUBE_HEIGHT_0 */
    { 0x806c, 0x6b8ec500 }, /* NVDLA_SDP_RDMA_D_EW_BATCH_STRIDE_0 */
    { 0x8034, 0x00000240 }, /* NVDLA_SDP_RDMA_D_BS_LINE_STRIDE_0 */
    { 0x8020, 0x00000080 }, /* NVDLA_SDP_RDMA_D_SRC_LINE_STRIDE_0 */
    { 0x800c, 0x00000002 }, /* NVDLA_SDP_RDMA_D_DATA_CUBE_WIDTH_0 */
    { 0x807c, 0x00000000 }, /* NVDLA_SDP_RDMA_D_STATUS_INF_INPUT_NUM_0 */
    { 0x8000, 0x00000000 }, /* NVDLA_SDP_RDMA_S_STATUS_0 */
    { 0x8070, 0x00000000 }, /* NVDLA_SDP_RDMA_D_FEATURE_MODE_CFG_0 */
    { 0x8030, 0x00000082 }, /* NVDLA_SDP_RDMA_D_BS_BASE_ADDR_HIGH_0 */
    { 0x8090, 0x00000000 }, /* NVDLA_SDP_RDMA_D_PERF_ERDMA_READ_STALL_0 */
    { 0x8074, 0x00000001 }, /* NVDLA_SDP_RDMA_D_SRC_DMA_CFG_0 */
    { 0x808c, 0x00000000 }, /* NVDLA_SDP_RDMA_D_PERF_NRDMA_READ_STALL_0 */
    { 0x9038, 0x00000001 }, /* NVDLA_SDP_D_OP_ENABLE_0 */
    { 0x8008, 0x00000001 }, /* NVDLA_SDP_RDMA_D_OP_ENABLE_0 */
    {-1, 0}
};

static const struct dla_reg program_enable[] = {
    {-1, 0}
};

/* Embedded data blocks (loaded via mmap instead of devmem scripts) */
static const uint32_t embed_0_data[][2] = {
    { 0x00000000, 0xb1200062 },
    { 0x00000004, 0x748abbe9 },
    { 0x00000008, 0xf8602ef0 },
    { 0x0000000c, 0xf46ca8e8 },
    { 0x00000010, 0xcff5509f },
    { 0x00000014, 0x4bac2f97 },
    { 0x00000080, 0xdc16d8b8 },
    { 0x00000084, 0x3ec2677d },
    { 0x00000088, 0x1a133934 },
    { 0x0000008c, 0x11c6c523 },
    { 0x00000090, 0x320a8b64 },
    { 0x00000094, 0x2416b948 },
    { 0x00000100, 0xbca0b279 },
    { 0x00000104, 0xe6b110cd },
    { 0x00000108, 0xa5b9424b },
    { 0x0000010c, 0x5db7e1bb },
    { 0x00000110, 0x7e72c5fc },
    { 0x00000114, 0xd7f844af },
    { 0x00000200, 0x1747832e },
    { 0x00000204, 0xc690128d },
    { 0x00000208, 0xe6841ccd },
    { 0x0000020c, 0x68cd09d1 },
    { 0x00000210, 0x835da206 },
    { 0x00000214, 0x94a12e29 },
    { 0x00000280, 0x8511580a },
    { 0x00000284, 0xc6878a8d },
    { 0x00000288, 0x1691a32d },
    { 0x0000028c, 0x18e87131 },
    { 0x00000290, 0xc544d88a },
    { 0x00000294, 0x4dfe879b },
    { 0x00000300, 0xc9662c92 },
    { 0x00000304, 0xa2ce0045 },
    { 0x00000308, 0x408a2981 },
    { 0x0000030c, 0xd932d8b2 },
    { 0x00000310, 0xd0770ea0 },
    { 0x00000314, 0x076adf0e },
    { 0x00000400, 0x632f07c6 },
    { 0x00000404, 0xd51cb4aa },
    { 0x00000408, 0x5d8363bb },
    { 0x0000040c, 0xeff692df },
    { 0x00000410, 0x624b63c4 },
    { 0x00000414, 0xebb0f6d7 },
    { 0x00000480, 0x64eb39c9 },
    { 0x00000484, 0xf8d3d8f1 },
    { 0x00000488, 0x2f36c55e },
    { 0x0000048c, 0x5a0f27b4 },
    { 0x00000490, 0xe6fbf4cd },
    { 0x00000494, 0xb5f8fa6b },
    { 0x00000500, 0xd4ea6aa9 },
    { 0x00000504, 0xcf31fc9e },
    { 0x00000508, 0x598d27b3 },
    { 0x0000050c, 0x05aeb90b },
    { 0x00000510, 0x3aaf8775 },
    { 0x00000514, 0x6ca7ffd9 },
    { 0x00000600, 0xd01df0a0 },
    { 0x00000604, 0x1be8cf37 },
    { 0x00000608, 0x3e8ed57d },
    { 0x0000060c, 0x2c402158 },
    { 0x00000610, 0xaea6d45d },
    { 0x00000614, 0x26c7134d },
    { 0x00000680, 0xda12deb4 },
    { 0x00000684, 0x729a7fe5 },
    { 0x00000688, 0x073a730e },
    { 0x0000068c, 0x322cc164 },
    { 0x00000690, 0x55ff23ab },
    { 0x00000694, 0xf1eca2e3 },
    { 0x00000700, 0x6ec2cbdd },
    { 0x00000704, 0x600b2dc0 },
    { 0x00000708, 0x24011148 },
    { 0x0000070c, 0xf0b860e1 },
    { 0x00000710, 0x86d26e0d },
    { 0x00000714, 0x13913127 },
    { 0x00000800, 0x0000006c },
    { 0x00000808, 0x00000025 },
    { 0x00000810, 0x000000dc },
    { 0x00000880, 0x00000037 },
    { 0x00000888, 0x00000080 },
    { 0x00000890, 0x000000bb },
    { 0x00000900, 0x0000009d },
    { 0x00000908, 0x000000df },
    { 0x00000910, 0x0000009e },
    { 0xFFFFFFFF, 0 }
};

static const uint32_t embed_1_data[][2] = {
    { 0x00000000, 0x3d19e345 },
    { 0x00000004, 0x808b0a19 },
    { 0x00000008, 0xa0b66037 },
    { 0x0000000c, 0x04ca7962 },
    { 0x00000010, 0x7f3ac0e0 },
    { 0x00000014, 0x84e74416 },
    { 0x00000020, 0x243c1e00 },
    { 0x00000024, 0x000a2b16 },
    { 0x00000028, 0x69000000 },
    { 0x0000002c, 0x00000046 },
    { 0x00000030, 0x2b4d0018 },
    { 0x00000034, 0x452b0000 },
    { 0x00000080, 0xaa54ee17 },
    { 0x00000084, 0xea4490f4 },
    { 0x00000088, 0x1388d1fe },
    { 0x0000008c, 0x97ff52f3 },
    { 0x00000090, 0x73d5d1b6 },
    { 0x00000094, 0x3936c12a },
    { 0x00000100, 0x7ebf17aa },
    { 0x00000104, 0x492a912d },
    { 0x00000108, 0xa9c4e6f7 },
    { 0x0000010c, 0x40356040 },
    { 0x00000110, 0x26a0b1a7 },
    { 0x00000114, 0x17fbe738 },
    { 0x00000120, 0x60000000 },
    { 0x00000124, 0x00007f00 },
    { 0x00000128, 0x3c596b00 },
    { 0x0000012c, 0x72000000 },
    { 0x00000130, 0x26005900 },
    { 0x00000134, 0x4b000000 },
    { 0x00000200, 0x88de7560 },
    { 0x00000204, 0xbebebb0b },
    { 0x00000208, 0x2277bdca },
    { 0x0000020c, 0x70145fc8 },
    { 0x00000210, 0xb5f6aac2 },
    { 0x00000214, 0xb5d66d0b },
    { 0x00000220, 0x0000586e },
    { 0x00000224, 0x64002d00 },
    { 0x00000228, 0x32000000 },
    { 0x0000022c, 0x6c002200 },
    { 0x00000234, 0x00117f40 },
    { 0x00000280, 0xa1687133 },
    { 0x00000284, 0x3ee4608e },
    { 0x00000288, 0x9cd74888 },
    { 0x0000028c, 0x0ebf73b4 },
    { 0x00000290, 0x09f2d126 },
    { 0x00000294, 0x6e50f20a },
    { 0x00000300, 0xdda80437 },
    { 0x00000304, 0x95f2aae5 },
    { 0x00000308, 0x99c66758 },
    { 0x0000030c, 0x058184c0 },
    { 0x00000310, 0x1a850e19 },
    { 0x00000314, 0xeb7ddab5 },
    { 0x00000400, 0xcbbb1d37 },
    { 0x00000404, 0x76a4b627 },
    { 0x00000408, 0x2c5519a1 },
    { 0x0000040c, 0x4b73728e },
    { 0x00000410, 0xbb149a04 },
    { 0x00000414, 0x2de0ff8c },
    { 0x00000420, 0x0000007f },
    { 0x00000480, 0x98386f9f },
    { 0x00000484, 0x775eb991 },
    { 0x00000488, 0x8c8a5265 },
    { 0x0000048c, 0x43d98123 },
    { 0x00000490, 0x971c2f95 },
    { 0x00000494, 0x55a249cc },
    { 0x00000500, 0x575f4ed4 },
    { 0x00000504, 0x714018a5 },
    { 0x00000508, 0xa45ade04 },
    { 0x0000050c, 0x846ca001 },
    { 0x00000510, 0x3882aadb },
    { 0x00000514, 0xf1ba9ef4 },
    { 0x00000600, 0x3c2d269f },
    { 0x00000604, 0xcb1643fa },
    { 0x00000608, 0x4a82e8bb },
    { 0x0000060c, 0xa6b4a31a },
    { 0x00000610, 0x547abae9 },
    { 0x00000614, 0x32478b88 },
    { 0x00000628, 0x0000001b },
    { 0x00000680, 0x7398d58f },
    { 0x00000684, 0x8f164bc4 },
    { 0x00000688, 0x383c3190 },
    { 0x0000068c, 0x5987a1b0 },
    { 0x00000690, 0xfb9e47aa },
    { 0x00000694, 0x5202abca },
    { 0x00000700, 0xbde2727f },
    { 0x00000704, 0x348a16fb },
    { 0x00000708, 0x208edc9e },
    { 0x0000070c, 0x74e0f2a5 },
    { 0x00000710, 0x87b09098 },
    { 0x00000714, 0xb848762c },
    { 0x00000800, 0x00000052 },
    { 0x00000808, 0x00000082 },
    { 0x00000810, 0x0000000c },
    { 0x00000880, 0x000000dc },
    { 0x00000888, 0x000000cc },
    { 0x00000890, 0x0000008b },
    { 0x00000900, 0x00000021 },
    { 0x00000908, 0x0000002b },
    { 0x00000910, 0x000000f7 },
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

    { /* Zero-init region 0xc0002000 size 0x1400 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0002000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 5120); munmap(zpage, 8192); }
    }
    { /* Zero-init region 0xd0000c20 size 0x1400 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 12288, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xd0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 3104, 0, 5120); munmap(zpage, 12288); }
    }
    { /* Zero-init region 0xf0000040 size 0x1400 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xf0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 64, 0, 5120); munmap(zpage, 8192); }
    }

    /* Load embedded data (replaces devmem load scripts) */
    { /* Data: base=0xc0002000, 81 writes */
        uint8_t* dpage = (uint8_t*)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0002000);
        if (dpage != MAP_FAILED) {
            const uint32_t (*dp)[2] = embed_0_data;
            uint32_t base_off = 0x0;
            while ((*dp)[0] != 0xFFFFFFFF) {
                *(volatile uint32_t*)(dpage + base_off + (*dp)[0]) = (*dp)[1];
                dp++;
            }
            munmap(dpage, 4096);
        } else { perror("mmap embed"); }
    }

    { /* Data: base=0xd0000c20, 100 writes */
        uint8_t* dpage = (uint8_t*)mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xd0000000);
        if (dpage != MAP_FAILED) {
            const uint32_t (*dp)[2] = embed_1_data;
            uint32_t base_off = 0xc20;
            while ((*dp)[0] != 0xFFFFFFFF) {
                *(volatile uint32_t*)(dpage + base_off + (*dp)[0]) = (*dp)[1];
                dp++;
            }
            munmap(dpage, 8192);
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
    { /* CRC[0] zero-init: addr=0xf0000040 size=0x1400 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xf0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 64, 0, 5120); munmap(zpage, 8192); }
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
    { /* CRC[0]: addr=0xf0000040, size=0x1400, expect=0xf480b323 */
        uint8_t* page = (uint8_t*)mmap(NULL, 8192, PROT_READ, MAP_SHARED, fd, 0xf0000000);
        if (page != MAP_FAILED) {
            uint32_t crc = calc_crc32(page + 64, 5120);
            printf("CRC[0] = 0x%08x (expect 0xf480b323) %s\n", crc, crc == 0xf480b323 ? "PASS" : "FAIL");
            if (crc != 0xf480b323) failures++;
            munmap(page, 8192);
        } else { perror("mmap out"); }
    }

    printf("Results: %d failures\n", failures);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return failures;
}