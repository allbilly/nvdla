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
    { 0xb058, 0x000001fe }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_6_CFG_0 */
    { 0xb070, 0xc0000000 }, /* NVDLA_PDP_D_DST_BASE_ADDR_LOW_0 */
    { 0xb078, 0x00002020 }, /* NVDLA_PDP_D_DST_LINE_STRIDE_0 */
    { 0xb054, 0x000001a9 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_5_CFG_0 */
    { 0xb040, 0x00007665 }, /* NVDLA_PDP_D_POOLING_PADDING_CFG_0 */
    { 0xb01c, 0x00000007 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_HEIGHT_0 */
    { 0xb08c, 0x00000000 }, /* NVDLA_PDP_D_NAN_INPUT_NUM_0 */
    { 0xb064, 0x00000000 }, /* NVDLA_PDP_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xb014, 0x00000000 }, /* NVDLA_PDP_D_DATA_CUBE_IN_CHANNEL_0 */
    { 0xb038, 0x00002000 }, /* NVDLA_PDP_D_RECIP_KERNEL_WIDTH_0 */
    { 0xb090, 0x00000000 }, /* NVDLA_PDP_D_NAN_OUTPUT_NUM_0 */
    { 0xb000, 0x00000000 }, /* NVDLA_PDP_S_STATUS_0 */
    { 0xb080, 0x00000001 }, /* NVDLA_PDP_D_DST_RAM_CFG_0 */
    { 0xb010, 0x0000000f }, /* NVDLA_PDP_D_DATA_CUBE_IN_HEIGHT_0 */
    { 0xb044, 0x00000055 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_1_CFG_0 */
    { 0xb02c, 0x2eb9f0bb }, /* NVDLA_PDP_D_PARTIAL_WIDTH_IN_0 */
    { 0xb05c, 0x00000253 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_7_CFG_0 */
    { 0xb050, 0x00000154 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_4_CFG_0 */
    { 0xb084, 0x00000000 }, /* NVDLA_PDP_D_DATA_FORMAT_0 */
    { 0xb06c, 0x000048c0 }, /* NVDLA_PDP_D_SRC_SURFACE_STRIDE_0 */
    { 0xb00c, 0x00000017 }, /* NVDLA_PDP_D_DATA_CUBE_IN_WIDTH_0 */
    { 0xb018, 0x0000001b }, /* NVDLA_PDP_D_DATA_CUBE_OUT_WIDTH_0 */
    { 0xb034, 0x00200707 }, /* NVDLA_PDP_D_POOLING_KERNEL_CFG_0 */
    { 0xb07c, 0x00010340 }, /* NVDLA_PDP_D_DST_SURFACE_STRIDE_0 */
    { 0xb098, 0x00000000 }, /* NVDLA_PDP_D_PERF_WRITE_STALL_0 */
    { 0xb088, 0x00000000 }, /* NVDLA_PDP_D_INF_INPUT_NUM_0 */
    { 0xb04c, 0x000000ff }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_3_CFG_0 */
    { 0xb068, 0x00000420 }, /* NVDLA_PDP_D_SRC_LINE_STRIDE_0 */
    { 0xb094, 0x00000000 }, /* NVDLA_PDP_D_PERF_ENABLE_0 */
    { 0xb020, 0x00000000 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_CHANNEL_0 */
    { 0xb048, 0x000000aa }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_2_CFG_0 */
    { 0xb060, 0x00000000 }, /* NVDLA_PDP_D_SRC_BASE_ADDR_LOW_0 */
    { 0xb024, 0x00000010 }, /* NVDLA_PDP_D_OPERATION_MODE_CFG_0 */
    { 0xb028, 0x00000001 }, /* NVDLA_PDP_D_NAN_FLUSH_TO_ZERO_0 */
    { 0xb074, 0x00000000 }, /* NVDLA_PDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0xb03c, 0x00002000 }, /* NVDLA_PDP_D_RECIP_KERNEL_HEIGHT_0 */
    { 0xb030, 0x2c5f1256 }, /* NVDLA_PDP_D_PARTIAL_WIDTH_OUT_0 */
    { 0xb09c, 0xd76bec80 }, /* NVDLA_PDP_D_CYA_0 */
    { 0xa004, 0x00000000 }, /* NVDLA_PDP_RDMA_S_POINTER_0 */
    { 0xa014, 0x00000000 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_CHANNEL_0 */
    { 0xa048, 0x00000000 }, /* NVDLA_PDP_RDMA_D_PERF_READ_STALL_0 */
    { 0xa01c, 0xc0800000 }, /* NVDLA_PDP_RDMA_D_SRC_BASE_ADDR_LOW_0 */
    { 0xa04c, 0xc09253f8 }, /* NVDLA_PDP_RDMA_D_CYA_0 */
    { 0xa028, 0x000048c0 }, /* NVDLA_PDP_RDMA_D_SRC_SURFACE_STRIDE_0 */
    { 0xa02c, 0x00000001 }, /* NVDLA_PDP_RDMA_D_SRC_RAM_CFG_0 */
    { 0xa03c, 0x00000005 }, /* NVDLA_PDP_RDMA_D_POOLING_PADDING_CFG_0 */
    { 0xa038, 0x00000007 }, /* NVDLA_PDP_RDMA_D_POOLING_KERNEL_CFG_0 */
    { 0xa020, 0x00000000 }, /* NVDLA_PDP_RDMA_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xa010, 0x0000000f }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_HEIGHT_0 */
    { 0xa018, 0x00000001 }, /* NVDLA_PDP_RDMA_D_FLYING_MODE_0 */
    { 0xa00c, 0x00000017 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_WIDTH_0 */
    { 0xa024, 0x00000420 }, /* NVDLA_PDP_RDMA_D_SRC_LINE_STRIDE_0 */
    { 0xa030, 0x00000000 }, /* NVDLA_PDP_RDMA_D_DATA_FORMAT_0 */
    { 0xa044, 0x00000001 }, /* NVDLA_PDP_RDMA_D_PERF_ENABLE_0 */
    { 0xa034, 0x00000000 }, /* NVDLA_PDP_RDMA_D_OPERATION_MODE_CFG_0 */
    { 0xa000, 0x00000000 }, /* NVDLA_PDP_RDMA_S_STATUS_0 */
    { 0xa040, 0x2eb9f0bb }, /* NVDLA_PDP_RDMA_D_PARTIAL_WIDTH_IN_0 */
    { 0xb008, 0x00000001 }, /* NVDLA_PDP_D_OP_ENABLE_0 */
    { 0xa008, 0x00000001 }, /* NVDLA_PDP_RDMA_D_OP_ENABLE_0 */
    {-1, 0}
};

static const struct dla_reg program_enable[] = {
    {-1, 0}
};

/* Embedded data blocks (loaded via mmap instead of devmem scripts) */
static const uint32_t embed_0_data[][2] = {
    { 0x00000000, 0x000000b6 },
    { 0x00000008, 0x000000aa },
    { 0x00000010, 0x000000be },
    { 0x00000018, 0x0000008e },
    { 0x00000020, 0x000000d4 },
    { 0x00000028, 0x00000001 },
    { 0x00000030, 0x000000a4 },
    { 0x00000038, 0x00000051 },
    { 0x00000040, 0x00000088 },
    { 0x00000048, 0x000000e7 },
    { 0x00000050, 0x00000088 },
    { 0x00000058, 0x00000054 },
    { 0x00000060, 0x0000009a },
    { 0x00000068, 0x00000049 },
    { 0x00000070, 0x00000008 },
    { 0x00000078, 0x000000ac },
    { 0x00000080, 0x00000044 },
    { 0x00000088, 0x000000b6 },
    { 0x00000090, 0x00000010 },
    { 0x00000098, 0x000000d7 },
    { 0x000000a0, 0x000000ba },
    { 0x000000a8, 0x00000052 },
    { 0x000000b0, 0x000000a0 },
    { 0x000000b8, 0x00000085 },
    { 0x00000420, 0x0000000a },
    { 0x00000428, 0x000000cb },
    { 0x00000430, 0x000000dc },
    { 0x00000438, 0x000000f6 },
    { 0x00000440, 0x000000ab },
    { 0x00000448, 0x000000bc },
    { 0x00000450, 0x00000019 },
    { 0x00000458, 0x00000036 },
    { 0x00000460, 0x00000044 },
    { 0x00000468, 0x0000001e },
    { 0x00000470, 0x000000e7 },
    { 0x00000478, 0x000000fb },
    { 0x00000480, 0x00000004 },
    { 0x00000488, 0x000000f4 },
    { 0x00000490, 0x000000d1 },
    { 0x00000498, 0x0000002f },
    { 0x000004a0, 0x00000088 },
    { 0x000004a8, 0x000000d5 },
    { 0x000004b0, 0x000000d5 },
    { 0x000004b8, 0x00000058 },
    { 0x000004c0, 0x000000ae },
    { 0x000004c8, 0x000000fb },
    { 0x000004d0, 0x00000040 },
    { 0x000004d8, 0x000000c0 },
    { 0x00000840, 0x00000072 },
    { 0x00000848, 0x000000c0 },
    { 0x00000850, 0x0000002f },
    { 0x00000858, 0x0000009f },
    { 0x00000860, 0x000000f3 },
    { 0x00000868, 0x000000ca },
    { 0x00000870, 0x00000084 },
    { 0x00000878, 0x0000008b },
    { 0x00000880, 0x00000017 },
    { 0x00000888, 0x000000d0 },
    { 0x00000890, 0x000000f2 },
    { 0x00000898, 0x00000097 },
    { 0x000008a0, 0x00000022 },
    { 0x000008a8, 0x0000002c },
    { 0x000008b0, 0x00000088 },
    { 0x000008b8, 0x000000da },
    { 0x000008c0, 0x00000031 },
    { 0x000008c8, 0x0000006c },
    { 0x000008d0, 0x00000026 },
    { 0x000008d8, 0x00000038 },
    { 0x000008e0, 0x000000c6 },
    { 0x000008e8, 0x000000bd },
    { 0x000008f0, 0x0000008c },
    { 0x000008f8, 0x00000060 },
    { 0x00000c60, 0x000000ce },
    { 0x00000c68, 0x00000027 },
    { 0x00000c70, 0x0000006f },
    { 0x00000c78, 0x0000001e },
    { 0x00000c80, 0x000000c1 },
    { 0x00000c88, 0x000000d5 },
    { 0x00000c90, 0x000000a5 },
    { 0x00000c98, 0x00000013 },
    { 0x00000ca0, 0x000000b5 },
    { 0x00000ca8, 0x000000a4 },
    { 0x00000cb0, 0x00000090 },
    { 0x00000cb8, 0x000000ca },
    { 0x00000cc0, 0x00000004 },
    { 0x00000cc8, 0x00000073 },
    { 0x00000cd0, 0x00000097 },
    { 0x00000cd8, 0x00000043 },
    { 0x00000ce0, 0x000000a9 },
    { 0x00000ce8, 0x0000000a },
    { 0x00000cf0, 0x000000f7 },
    { 0x00000cf8, 0x000000aa },
    { 0x00000d00, 0x000000b8 },
    { 0x00000d08, 0x00000063 },
    { 0x00000d10, 0x00000067 },
    { 0x00000d18, 0x00000026 },
    { 0x00001080, 0x000000de },
    { 0x00001088, 0x00000077 },
    { 0x00001090, 0x00000017 },
    { 0x00001098, 0x0000008f },
    { 0x000010a0, 0x0000008b },
    { 0x000010a8, 0x0000004b },
    { 0x000010b0, 0x0000005a },
    { 0x000010b8, 0x000000aa },
    { 0x000010c0, 0x0000002a },
    { 0x000010c8, 0x000000ea },
    { 0x000010d0, 0x00000070 },
    { 0x000010d8, 0x0000008f },
    { 0x000010e0, 0x00000075 },
    { 0x000010e8, 0x000000bb },
    { 0x000010f0, 0x000000c4 },
    { 0x000010f8, 0x000000c2 },
    { 0x00001100, 0x00000060 },
    { 0x00001108, 0x00000039 },
    { 0x00001110, 0x00000098 },
    { 0x00001118, 0x00000074 },
    { 0x00001120, 0x000000d6 },
    { 0x00001128, 0x000000f7 },
    { 0x00001130, 0x00000014 },
    { 0x00001138, 0x0000007d },
    { 0x000014a0, 0x0000001d },
    { 0x000014a8, 0x000000f8 },
    { 0x000014b0, 0x00000002 },
    { 0x000014b8, 0x00000091 },
    { 0x000014c0, 0x00000040 },
    { 0x000014c8, 0x00000073 },
    { 0x000014d0, 0x000000fa },
    { 0x000014d8, 0x000000db },
    { 0x000014e0, 0x0000008e },
    { 0x000014e8, 0x000000d1 },
    { 0x000014f0, 0x00000077 },
    { 0x000014f8, 0x00000040 },
    { 0x00001500, 0x000000db },
    { 0x00001508, 0x00000080 },
    { 0x00001510, 0x000000e7 },
    { 0x00001518, 0x000000b6 },
    { 0x00001520, 0x0000007c },
    { 0x00001528, 0x0000006b },
    { 0x00001530, 0x000000aa },
    { 0x00001538, 0x00000052 },
    { 0x00001540, 0x0000002d },
    { 0x00001548, 0x00000033 },
    { 0x00001550, 0x0000000a },
    { 0x00001558, 0x00000038 },
    { 0x000018c0, 0x00000071 },
    { 0x000018c8, 0x00000075 },
    { 0x000018d0, 0x00000055 },
    { 0x000018d8, 0x00000043 },
    { 0x000018e0, 0x00000088 },
    { 0x000018e8, 0x000000d1 },
    { 0x000018f0, 0x00000001 },
    { 0x000018f8, 0x00000068 },
    { 0x00001900, 0x0000001a },
    { 0x00001908, 0x000000bb },
    { 0x00001910, 0x0000008a },
    { 0x00001918, 0x00000052 },
    { 0x00001920, 0x00000076 },
    { 0x00001928, 0x000000aa },
    { 0x00001930, 0x00000097 },
    { 0x00001938, 0x000000ff },
    { 0x00001940, 0x00000017 },
    { 0x00001948, 0x000000b0 },
    { 0x00001950, 0x000000a1 },
    { 0x00001958, 0x000000e0 },
    { 0x00001960, 0x00000016 },
    { 0x00001968, 0x000000e5 },
    { 0x00001970, 0x0000003c },
    { 0x00001978, 0x000000ae },
    { 0x00001ce0, 0x000000a7 },
    { 0x00001ce8, 0x0000002c },
    { 0x00001cf0, 0x00000062 },
    { 0x00001cf8, 0x00000055 },
    { 0x00001d00, 0x0000000e },
    { 0x00001d08, 0x0000009c },
    { 0x00001d10, 0x00000004 },
    { 0x00001d18, 0x00000095 },
    { 0x00001d20, 0x00000090 },
    { 0x00001d28, 0x000000ca },
    { 0x00001d30, 0x00000018 },
    { 0x00001d38, 0x000000bb },
    { 0x00001d40, 0x00000076 },
    { 0x00001d48, 0x000000a3 },
    { 0x00001d50, 0x00000082 },
    { 0x00001d58, 0x000000bb },
    { 0x00001d60, 0x000000f2 },
    { 0x00001d68, 0x00000084 },
    { 0x00001d70, 0x0000008a },
    { 0x00001d78, 0x00000019 },
    { 0x00001d80, 0x00000037 },
    { 0x00001d88, 0x0000003d },
    { 0x00001d90, 0x0000003a },
    { 0x00001d98, 0x00000037 },
    { 0x00002100, 0x00000017 },
    { 0x00002108, 0x00000036 },
    { 0x00002110, 0x000000d4 },
    { 0x00002118, 0x00000045 },
    { 0x00002120, 0x00000038 },
    { 0x00002128, 0x0000002b },
    { 0x00002130, 0x00000047 },
    { 0x00002138, 0x0000003e },
    { 0x00002140, 0x00000020 },
    { 0x00002148, 0x000000cc },
    { 0x00002150, 0x0000008e },
    { 0x00002158, 0x0000009e },
    { 0x00002160, 0x0000002b },
    { 0x00002168, 0x0000007e },
    { 0x00002170, 0x000000fb },
    { 0x00002178, 0x0000001a },
    { 0x00002180, 0x000000b5 },
    { 0x00002188, 0x00000052 },
    { 0x00002190, 0x00000079 },
    { 0x00002198, 0x000000e0 },
    { 0x000021a0, 0x000000e9 },
    { 0x000021a8, 0x0000005f },
    { 0x000021b0, 0x000000a6 },
    { 0x000021b8, 0x0000000c },
    { 0x00002520, 0x00000057 },
    { 0x00002528, 0x00000016 },
    { 0x00002530, 0x0000009f },
    { 0x00002538, 0x000000bd },
    { 0x00002540, 0x00000001 },
    { 0x00002548, 0x000000b4 },
    { 0x00002550, 0x0000007a },
    { 0x00002558, 0x00000098 },
    { 0x00002560, 0x00000009 },
    { 0x00002568, 0x000000f2 },
    { 0x00002570, 0x000000b7 },
    { 0x00002578, 0x0000007f },
    { 0x00002580, 0x00000055 },
    { 0x00002588, 0x00000062 },
    { 0x00002590, 0x00000081 },
    { 0x00002598, 0x000000e4 },
    { 0x000025a0, 0x000000ad },
    { 0x000025a8, 0x000000cb },
    { 0x000025b0, 0x00000036 },
    { 0x000025b8, 0x000000ba },
    { 0x000025c0, 0x00000091 },
    { 0x000025c8, 0x00000072 },
    { 0x000025d0, 0x000000b4 },
    { 0x000025d8, 0x000000c4 },
    { 0x00002940, 0x00000048 },
    { 0x00002948, 0x0000006d },
    { 0x00002950, 0x00000060 },
    { 0x00002958, 0x000000b9 },
    { 0x00002960, 0x00000087 },
    { 0x00002968, 0x00000095 },
    { 0x00002970, 0x00000081 },
    { 0x00002978, 0x0000004b },
    { 0x00002980, 0x00000061 },
    { 0x00002988, 0x000000c4 },
    { 0x00002990, 0x0000002d },
    { 0x00002998, 0x00000054 },
    { 0x000029a0, 0x000000a8 },
    { 0x000029a8, 0x000000aa },
    { 0x000029b0, 0x000000c2 },
    { 0x000029b8, 0x00000026 },
    { 0x000029c0, 0x0000006d },
    { 0x000029c8, 0x00000019 },
    { 0x000029d0, 0x000000a5 },
    { 0x000029d8, 0x000000a6 },
    { 0x000029e0, 0x000000b1 },
    { 0x000029e8, 0x0000008c },
    { 0x000029f0, 0x00000014 },
    { 0x000029f8, 0x000000a4 },
    { 0x00002d60, 0x00000019 },
    { 0x00002d68, 0x0000009e },
    { 0x00002d70, 0x000000b8 },
    { 0x00002d78, 0x000000e0 },
    { 0x00002d80, 0x00000087 },
    { 0x00002d88, 0x00000034 },
    { 0x00002d90, 0x000000f1 },
    { 0x00002d98, 0x00000084 },
    { 0x00002da0, 0x00000016 },
    { 0x00002da8, 0x00000049 },
    { 0x00002db0, 0x00000038 },
    { 0x00002db8, 0x0000009e },
    { 0x00002dc0, 0x000000de },
    { 0x00002dc8, 0x0000004e },
    { 0x00002dd0, 0x000000bf },
    { 0x00002dd8, 0x00000023 },
    { 0x00002de0, 0x000000cc },
    { 0x00002de8, 0x00000053 },
    { 0x00002df0, 0x00000099 },
    { 0x00002df8, 0x000000a0 },
    { 0x00002e00, 0x00000048 },
    { 0x00002e08, 0x00000050 },
    { 0x00002e10, 0x00000090 },
    { 0x00002e18, 0x00000071 },
    { 0x00003180, 0x000000b5 },
    { 0x00003188, 0x0000007b },
    { 0x00003190, 0x000000bf },
    { 0x00003198, 0x00000060 },
    { 0x000031a0, 0x00000056 },
    { 0x000031a8, 0x00000057 },
    { 0x000031b0, 0x00000045 },
    { 0x000031b8, 0x000000a1 },
    { 0x000031c0, 0x00000022 },
    { 0x000031c8, 0x00000037 },
    { 0x000031d0, 0x00000010 },
    { 0x000031d8, 0x00000082 },
    { 0x000031e0, 0x00000052 },
    { 0x000031e8, 0x00000035 },
    { 0x000031f0, 0x000000f6 },
    { 0x000031f8, 0x00000082 },
    { 0x00003200, 0x0000003c },
    { 0x00003208, 0x000000f8 },
    { 0x00003210, 0x0000000b },
    { 0x00003218, 0x000000e7 },
    { 0x00003220, 0x00000021 },
    { 0x00003228, 0x000000e2 },
    { 0x00003230, 0x0000006e },
    { 0x00003238, 0x000000e2 },
    { 0x000035a0, 0x000000f2 },
    { 0x000035a8, 0x0000008a },
    { 0x000035b0, 0x000000ff },
    { 0x000035b8, 0x00000070 },
    { 0x000035c0, 0x00000004 },
    { 0x000035c8, 0x0000001c },
    { 0x000035d0, 0x0000008f },
    { 0x000035d8, 0x00000070 },
    { 0x000035e0, 0x0000000f },
    { 0x000035e8, 0x000000a2 },
    { 0x000035f0, 0x000000e3 },
    { 0x000035f8, 0x000000d1 },
    { 0x00003600, 0x000000a1 },
    { 0x00003608, 0x00000032 },
    { 0x00003610, 0x00000058 },
    { 0x00003618, 0x0000000e },
    { 0x00003620, 0x0000009c },
    { 0x00003628, 0x000000dc },
    { 0x00003630, 0x0000005f },
    { 0x00003638, 0x000000a0 },
    { 0x00003640, 0x00000085 },
    { 0x00003648, 0x000000b5 },
    { 0x00003650, 0x0000003d },
    { 0x00003658, 0x00000073 },
    { 0x000039c0, 0x0000006e },
    { 0x000039c8, 0x0000007f },
    { 0x000039d0, 0x00000059 },
    { 0x000039d8, 0x0000005e },
    { 0x000039e0, 0x00000073 },
    { 0x000039e8, 0x000000e8 },
    { 0x000039f0, 0x0000002a },
    { 0x000039f8, 0x0000009a },
    { 0x00003a00, 0x0000002d },
    { 0x00003a08, 0x000000c8 },
    { 0x00003a10, 0x000000ee },
    { 0x00003a18, 0x00000016 },
    { 0x00003a20, 0x0000009e },
    { 0x00003a28, 0x000000fe },
    { 0x00003a30, 0x000000dd },
    { 0x00003a38, 0x00000024 },
    { 0x00003a40, 0x000000d9 },
    { 0x00003a48, 0x00000086 },
    { 0x00003a50, 0x00000047 },
    { 0x00003a58, 0x00000055 },
    { 0x00003a60, 0x0000008b },
    { 0x00003a68, 0x00000098 },
    { 0x00003a70, 0x00000005 },
    { 0x00003a78, 0x000000f1 },
    { 0x00003de0, 0x000000fe },
    { 0x00003de8, 0x000000e6 },
    { 0x00003df0, 0x000000be },
    { 0x00003df8, 0x0000000d },
    { 0x00003e00, 0x000000a1 },
    { 0x00003e08, 0x00000013 },
    { 0x00003e10, 0x000000f7 },
    { 0x00003e18, 0x00000078 },
    { 0x00003e20, 0x00000052 },
    { 0x00003e28, 0x0000008b },
    { 0x00003e30, 0x000000a5 },
    { 0x00003e38, 0x0000006d },
    { 0x00003e40, 0x000000b0 },
    { 0x00003e48, 0x00000065 },
    { 0x00003e50, 0x0000002d },
    { 0x00003e58, 0x0000007d },
    { 0x00003e60, 0x00000073 },
    { 0x00003e68, 0x0000008f },
    { 0x00003e70, 0x000000ba },
    { 0x00003e78, 0x0000004a },
    { 0x00003e80, 0x000000eb },
    { 0x00003e88, 0x000000f4 },
    { 0x00003e90, 0x000000f2 },
    { 0x00003e98, 0x0000000b },
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

    { /* Zero-init region 0xc0800000 size 0x48c0 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 20480, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0800000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 18624); munmap(zpage, 20480); }
    }
    { /* Zero-init region 0xc0000000 size 0x10340 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 69632, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 66368); munmap(zpage, 69632); }
    }

    /* Load embedded data (replaces devmem load scripts) */
    { /* Data: base=0xc0800000, 384 writes */
        uint8_t* dpage = (uint8_t*)mmap(NULL, 16384, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0800000);
        if (dpage != MAP_FAILED) {
            const uint32_t (*dp)[2] = embed_0_data;
            uint32_t base_off = 0x0;
            while ((*dp)[0] != 0xFFFFFFFF) {
                *(volatile uint32_t*)(dpage + base_off + (*dp)[0]) = (*dp)[1];
                dp++;
            }
            munmap(dpage, 16384);
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
    { /* CRC[0] zero-init: addr=0xc0000000 size=0x10340 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 69632, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 66368); munmap(zpage, 69632); }
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
    { /* CRC[0]: addr=0xc0000000, size=0x10340, expect=0x0f7aee8d */
        uint8_t* page = (uint8_t*)mmap(NULL, 69632, PROT_READ, MAP_SHARED, fd, 0xc0000000);
        if (page != MAP_FAILED) {
            uint32_t crc = calc_crc32(page + 0, 66368);
            printf("CRC[0] = 0x%08x (expect 0x0f7aee8d) %s\n", crc, crc == 0x0f7aee8d ? "PASS" : "FAIL");
            if (crc != 0x0f7aee8d) failures++;
            munmap(page, 69632);
        } else { perror("mmap out"); }
    }

    printf("Results: %d failures\n", failures);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return failures;
}