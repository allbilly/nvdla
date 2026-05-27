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
    { 0xb03c, 0x00005555 }, /* NVDLA_PDP_D_RECIP_KERNEL_HEIGHT_0 */
    { 0xb01c, 0x00000004 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_HEIGHT_0 */
    { 0xb08c, 0x00000000 }, /* NVDLA_PDP_D_NAN_INPUT_NUM_0 */
    { 0xb070, 0xc0000000 }, /* NVDLA_PDP_D_DST_BASE_ADDR_LOW_0 */
    { 0xb098, 0x00000000 }, /* NVDLA_PDP_D_PERF_WRITE_STALL_0 */
    { 0xb030, 0x2c5f1256 }, /* NVDLA_PDP_D_PARTIAL_WIDTH_OUT_0 */
    { 0xb010, 0x00000008 }, /* NVDLA_PDP_D_DATA_CUBE_IN_HEIGHT_0 */
    { 0xb020, 0x00000012 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_CHANNEL_0 */
    { 0xb04c, 0x000000ff }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_3_CFG_0 */
    { 0xb038, 0x00002000 }, /* NVDLA_PDP_D_RECIP_KERNEL_WIDTH_0 */
    { 0xb094, 0x00000000 }, /* NVDLA_PDP_D_PERF_ENABLE_0 */
    { 0xb064, 0x00000000 }, /* NVDLA_PDP_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xb044, 0x00000055 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_1_CFG_0 */
    { 0xb018, 0x00000007 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_WIDTH_0 */
    { 0xb02c, 0x2eb9f0bb }, /* NVDLA_PDP_D_PARTIAL_WIDTH_IN_0 */
    { 0xb06c, 0x00002300 }, /* NVDLA_PDP_D_SRC_SURFACE_STRIDE_0 */
    { 0xb040, 0x00000426 }, /* NVDLA_PDP_D_POOLING_PADDING_CFG_0 */
    { 0xb088, 0x00000000 }, /* NVDLA_PDP_D_INF_INPUT_NUM_0 */
    { 0xb084, 0x00000000 }, /* NVDLA_PDP_D_DATA_FORMAT_0 */
    { 0xb00c, 0x0000000b }, /* NVDLA_PDP_D_DATA_CUBE_IN_WIDTH_0 */
    { 0xb028, 0x00000001 }, /* NVDLA_PDP_D_NAN_FLUSH_TO_ZERO_0 */
    { 0xb050, 0x00000154 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_4_CFG_0 */
    { 0xb05c, 0x00000253 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_7_CFG_0 */
    { 0xb034, 0x00110207 }, /* NVDLA_PDP_D_POOLING_KERNEL_CFG_0 */
    { 0xb060, 0x00000000 }, /* NVDLA_PDP_D_SRC_BASE_ADDR_LOW_0 */
    { 0xb09c, 0xd76bec80 }, /* NVDLA_PDP_D_CYA_0 */
    { 0xb074, 0x00000000 }, /* NVDLA_PDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0xb000, 0x00000000 }, /* NVDLA_PDP_S_STATUS_0 */
    { 0xb054, 0x000001a9 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_5_CFG_0 */
    { 0xb024, 0x00000010 }, /* NVDLA_PDP_D_OPERATION_MODE_CFG_0 */
    { 0xb090, 0x00000000 }, /* NVDLA_PDP_D_NAN_OUTPUT_NUM_0 */
    { 0xb07c, 0x0000a240 }, /* NVDLA_PDP_D_DST_SURFACE_STRIDE_0 */
    { 0xb080, 0x00000001 }, /* NVDLA_PDP_D_DST_RAM_CFG_0 */
    { 0xb068, 0x000002c0 }, /* NVDLA_PDP_D_SRC_LINE_STRIDE_0 */
    { 0xb058, 0x000001fe }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_6_CFG_0 */
    { 0xb014, 0x00000012 }, /* NVDLA_PDP_D_DATA_CUBE_IN_CHANNEL_0 */
    { 0xb078, 0x00002000 }, /* NVDLA_PDP_D_DST_LINE_STRIDE_0 */
    { 0xb048, 0x000000aa }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_2_CFG_0 */
    { 0xa004, 0x00000000 }, /* NVDLA_PDP_RDMA_S_POINTER_0 */
    { 0xa020, 0x00000000 }, /* NVDLA_PDP_RDMA_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xa03c, 0x00000005 }, /* NVDLA_PDP_RDMA_D_POOLING_PADDING_CFG_0 */
    { 0xa010, 0x00000008 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_HEIGHT_0 */
    { 0xa00c, 0x0000000b }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_WIDTH_0 */
    { 0xa018, 0x00000001 }, /* NVDLA_PDP_RDMA_D_FLYING_MODE_0 */
    { 0xa01c, 0xc0800000 }, /* NVDLA_PDP_RDMA_D_SRC_BASE_ADDR_LOW_0 */
    { 0xa040, 0x2eb9f0bb }, /* NVDLA_PDP_RDMA_D_PARTIAL_WIDTH_IN_0 */
    { 0xa014, 0x00000012 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_CHANNEL_0 */
    { 0xa024, 0x000002c0 }, /* NVDLA_PDP_RDMA_D_SRC_LINE_STRIDE_0 */
    { 0xa038, 0x00000017 }, /* NVDLA_PDP_RDMA_D_POOLING_KERNEL_CFG_0 */
    { 0xa000, 0x00000000 }, /* NVDLA_PDP_RDMA_S_STATUS_0 */
    { 0xa030, 0x00000000 }, /* NVDLA_PDP_RDMA_D_DATA_FORMAT_0 */
    { 0xa034, 0x00000000 }, /* NVDLA_PDP_RDMA_D_OPERATION_MODE_CFG_0 */
    { 0xa02c, 0x00000001 }, /* NVDLA_PDP_RDMA_D_SRC_RAM_CFG_0 */
    { 0xa044, 0x00000001 }, /* NVDLA_PDP_RDMA_D_PERF_ENABLE_0 */
    { 0xa048, 0x00000000 }, /* NVDLA_PDP_RDMA_D_PERF_READ_STALL_0 */
    { 0xa028, 0x00002300 }, /* NVDLA_PDP_RDMA_D_SRC_SURFACE_STRIDE_0 */
    { 0xa04c, 0xc09253f8 }, /* NVDLA_PDP_RDMA_D_CYA_0 */
    { 0xb008, 0x00000001 }, /* NVDLA_PDP_D_OP_ENABLE_0 */
    { 0xa008, 0x00000001 }, /* NVDLA_PDP_RDMA_D_OP_ENABLE_0 */
    {-1, 0}
};

static const struct dla_reg program_enable[] = {
    {-1, 0}
};

/* Embedded data blocks (loaded via mmap instead of devmem scripts) */
static const uint32_t embed_0_data[][2] = {
    { 0x00000000, 0x4e9f44cc },
    { 0x00000004, 0xa6e35a9c },
    { 0x00000008, 0x721e7f2b },
    { 0x0000000c, 0x05acf2f3 },
    { 0x00000010, 0x275c201b },
    { 0x00000014, 0xc630a085 },
    { 0x00000018, 0x04525250 },
    { 0x0000001c, 0x80e80bb2 },
    { 0x00000020, 0x1665497e },
    { 0x00000024, 0xa910ff8a },
    { 0x00000028, 0xe7515f44 },
    { 0x0000002c, 0x383c98b4 },
    { 0x00000030, 0xe9ca5317 },
    { 0x00000034, 0x5fa86614 },
    { 0x00000038, 0x728de5b1 },
    { 0x0000003c, 0x7af7afe8 },
    { 0x00000040, 0xc8f5e9a0 },
    { 0x00000044, 0x286bc196 },
    { 0x00000048, 0x7dd2b0eb },
    { 0x0000004c, 0x9de8b943 },
    { 0x00000050, 0xfc559829 },
    { 0x00000054, 0xf9ffc619 },
    { 0x00000058, 0x3b750455 },
    { 0x0000005c, 0xeabff39c },
    { 0x000002c0, 0x0d976cf3 },
    { 0x000002c4, 0xb61015ae },
    { 0x000002c8, 0x886cf5f3 },
    { 0x000002cc, 0x55456146 },
    { 0x000002d0, 0x3f826030 },
    { 0x000002d4, 0xf56264c4 },
    { 0x000002d8, 0x53d45236 },
    { 0x000002dc, 0x99261ac5 },
    { 0x000002e0, 0x6ecc2b6d },
    { 0x000002e4, 0xc74ebc3c },
    { 0x000002e8, 0x6a9f413c },
    { 0x000002ec, 0x4adeb6f1 },
    { 0x000002f0, 0xd2e21ab9 },
    { 0x000002f4, 0xccebaeec },
    { 0x000002f8, 0x83743a47 },
    { 0x000002fc, 0xbe3be165 },
    { 0x00000300, 0xdefbb049 },
    { 0x00000304, 0xfaeceb2f },
    { 0x00000308, 0x1e459104 },
    { 0x0000030c, 0x0a048b8b },
    { 0x00000310, 0xb45ce180 },
    { 0x00000314, 0xa02a3a61 },
    { 0x00000318, 0x59b61011 },
    { 0x0000031c, 0x30d9fe2c },
    { 0x00000580, 0x605a75ab },
    { 0x00000584, 0xc956ab01 },
    { 0x00000588, 0x5fb9078e },
    { 0x0000058c, 0xc12f6525 },
    { 0x00000590, 0x04216da7 },
    { 0x00000594, 0x6ffac67d },
    { 0x00000598, 0xdc6f9dec },
    { 0x0000059c, 0x7f7893c0 },
    { 0x000005a0, 0x25ca5719 },
    { 0x000005a4, 0xa92698c0 },
    { 0x000005a8, 0xb67a88c4 },
    { 0x000005ac, 0x769e1715 },
    { 0x000005b0, 0xa6d039e2 },
    { 0x000005b4, 0x235bab4c },
    { 0x000005b8, 0x7936e7b0 },
    { 0x000005bc, 0x8bcdd548 },
    { 0x000005c0, 0xbdc54b97 },
    { 0x000005c4, 0x0cb7ff8b },
    { 0x000005c8, 0x5decdfb7 },
    { 0x000005cc, 0x20633a22 },
    { 0x000005d0, 0xb502cbf3 },
    { 0x000005d4, 0xdea4f0e2 },
    { 0x000005d8, 0x76d2507d },
    { 0x000005dc, 0x62a6cd19 },
    { 0x00000840, 0x4a375611 },
    { 0x00000844, 0x41198cf7 },
    { 0x00000848, 0x6c33aae6 },
    { 0x0000084c, 0x73101ad3 },
    { 0x00000850, 0x178a6279 },
    { 0x00000854, 0x363ba9d1 },
    { 0x00000858, 0xc2574fad },
    { 0x0000085c, 0xd4daa56d },
    { 0x00000860, 0xc91c4e08 },
    { 0x00000864, 0x54d22ac1 },
    { 0x00000868, 0x33e527ab },
    { 0x0000086c, 0x87afe62f },
    { 0x00000870, 0x03c29ced },
    { 0x00000874, 0xc59aca90 },
    { 0x00000878, 0x25b3775a },
    { 0x0000087c, 0xa612bef0 },
    { 0x00000880, 0x484e186f },
    { 0x00000884, 0x2d879c16 },
    { 0x00000888, 0x0f9362dc },
    { 0x0000088c, 0x11318246 },
    { 0x00000890, 0x211cc40f },
    { 0x00000894, 0xff8fc2b6 },
    { 0x00000898, 0xa77e38c0 },
    { 0x0000089c, 0x1688b23d },
    { 0x00000b00, 0x564e7b6f },
    { 0x00000b04, 0x325204fe },
    { 0x00000b08, 0x0a13e3ea },
    { 0x00000b0c, 0x76feba6c },
    { 0x00000b10, 0x4f47a860 },
    { 0x00000b14, 0x6a306602 },
    { 0x00000b18, 0x18cbba69 },
    { 0x00000b1c, 0x45f26cbd },
    { 0x00000b20, 0x8fbe9a17 },
    { 0x00000b24, 0x99b98282 },
    { 0x00000b28, 0xb82c30ee },
    { 0x00000b2c, 0xc15224cd },
    { 0x00000b30, 0x1e592570 },
    { 0x00000b34, 0x42092c6e },
    { 0x00000b38, 0x0a3fc1e9 },
    { 0x00000b3c, 0x5430c6ff },
    { 0x00000b40, 0x0426619b },
    { 0x00000b44, 0x4a7f8e83 },
    { 0x00000b48, 0x36950921 },
    { 0x00000b4c, 0xb9378afe },
    { 0x00000b50, 0xad8496b9 },
    { 0x00000b54, 0xaa465c59 },
    { 0x00000b58, 0x0e237001 },
    { 0x00000b5c, 0x58bd12bb },
    { 0x00000dc0, 0x3c5438d0 },
    { 0x00000dc4, 0x5571616a },
    { 0x00000dc8, 0x5e501fce },
    { 0x00000dcc, 0x76ce2a44 },
    { 0x00000dd0, 0x2dd3dbae },
    { 0x00000dd4, 0x78c9d1b0 },
    { 0x00000dd8, 0x1a73a31d },
    { 0x00000ddc, 0xf54a39dd },
    { 0x00000de0, 0x122fc2c8 },
    { 0x00000de4, 0xf1d7bb59 },
    { 0x00000de8, 0x68fc3456 },
    { 0x00000dec, 0x253dba54 },
    { 0x00000df0, 0x1640a406 },
    { 0x00000df4, 0x38cb7c16 },
    { 0x00000df8, 0x238df8fc },
    { 0x00000dfc, 0x23387d71 },
    { 0x00000e00, 0x3c610851 },
    { 0x00000e04, 0x8b1c5629 },
    { 0x00000e08, 0x2a82f224 },
    { 0x00000e0c, 0x1687b8b4 },
    { 0x00000e10, 0x91e0e1a4 },
    { 0x00000e14, 0xd1796855 },
    { 0x00000e18, 0x513bae10 },
    { 0x00000e1c, 0x81b0c0b6 },
    { 0x00001080, 0xd45eeedb },
    { 0x00001084, 0xb01c618c },
    { 0x00001088, 0xcd87fe0c },
    { 0x0000108c, 0x1ed108a3 },
    { 0x00001090, 0x53b0ba82 },
    { 0x00001094, 0xce229d91 },
    { 0x00001098, 0x222e8008 },
    { 0x0000109c, 0x7bd8a176 },
    { 0x000010a0, 0x1cf8e591 },
    { 0x000010a4, 0xf30dbb71 },
    { 0x000010a8, 0x3d55ef5d },
    { 0x000010ac, 0x1499595a },
    { 0x000010b0, 0xc36681d6 },
    { 0x000010b4, 0x6373977e },
    { 0x000010b8, 0x79691ece },
    { 0x000010bc, 0x31f32a21 },
    { 0x000010c0, 0xe0124182 },
    { 0x000010c4, 0xd95e71e1 },
    { 0x000010c8, 0xac404baa },
    { 0x000010cc, 0xa46fae2b },
    { 0x000010d0, 0xfdd62532 },
    { 0x000010d4, 0x7f6653e9 },
    { 0x000010d8, 0x58083497 },
    { 0x000010dc, 0xf8140478 },
    { 0x00001340, 0x3cef6235 },
    { 0x00001344, 0x593244e4 },
    { 0x00001348, 0xbf37aaf4 },
    { 0x0000134c, 0xda30fb5e },
    { 0x00001350, 0x6550c03d },
    { 0x00001354, 0x521e36b8 },
    { 0x00001358, 0x014db802 },
    { 0x0000135c, 0xb3775581 },
    { 0x00001360, 0xeda8a722 },
    { 0x00001364, 0x8b8a9f17 },
    { 0x00001368, 0x2f2610a8 },
    { 0x0000136c, 0x1ea0456d },
    { 0x00001370, 0x7c209e21 },
    { 0x00001374, 0x4a8c2146 },
    { 0x00001378, 0x701904eb },
    { 0x0000137c, 0x7d7697df },
    { 0x00001380, 0x75926aea },
    { 0x00001384, 0x849406d4 },
    { 0x00001388, 0x8573deec },
    { 0x0000138c, 0xe2b5b6de },
    { 0x00001390, 0x80bb32f7 },
    { 0x00001394, 0xac76895e },
    { 0x00001398, 0x690bc9e8 },
    { 0x0000139c, 0x67ecd61e },
    { 0x00001600, 0xba1640a1 },
    { 0x00001604, 0xc13edae4 },
    { 0x00001608, 0xaa8342d0 },
    { 0x0000160c, 0xbfdad019 },
    { 0x00001610, 0xdf78ba5e },
    { 0x00001614, 0xa4dde96e },
    { 0x00001618, 0x5b171c2a },
    { 0x0000161c, 0x5c271239 },
    { 0x00001620, 0x0d7b77fa },
    { 0x00001624, 0xd0af2276 },
    { 0x00001628, 0x76b6c127 },
    { 0x0000162c, 0xea776d77 },
    { 0x00001630, 0xe939a8f7 },
    { 0x00001634, 0xfeec0501 },
    { 0x00001638, 0x8531f783 },
    { 0x0000163c, 0x353285d7 },
    { 0x00001640, 0xca3ba561 },
    { 0x00001644, 0x1584cc50 },
    { 0x00001648, 0x21630801 },
    { 0x0000164c, 0x2d14e739 },
    { 0x00001650, 0x803edc68 },
    { 0x00001654, 0x079adcb6 },
    { 0x00001658, 0x503e4f02 },
    { 0x0000165c, 0xaa747711 },
    { 0x00002300, 0x4d2c3309 },
    { 0x00002304, 0xc439a719 },
    { 0x00002308, 0xbbf51ae5 },
    { 0x0000230c, 0x2f91d576 },
    { 0x00002310, 0x108d87c8 },
    { 0x00002314, 0xfaf4446d },
    { 0x00002318, 0x29bd63c5 },
    { 0x0000231c, 0x1a210add },
    { 0x00002320, 0xc14e8aca },
    { 0x00002324, 0xbdc1bec7 },
    { 0x00002328, 0x1c552de1 },
    { 0x0000232c, 0xadf6d794 },
    { 0x00002330, 0x233c643e },
    { 0x00002334, 0xed11f277 },
    { 0x00002338, 0x1271f4f0 },
    { 0x0000233c, 0x98c555cc },
    { 0x00002340, 0xc505267a },
    { 0x00002344, 0xba121321 },
    { 0x00002348, 0x0ceadba4 },
    { 0x0000234c, 0x31c0bdba },
    { 0x00002350, 0x2966f93f },
    { 0x00002354, 0x92eb372c },
    { 0x00002358, 0xdb5fc778 },
    { 0x0000235c, 0xe0a4e2b5 },
    { 0x000025c0, 0x49148911 },
    { 0x000025c4, 0x3883a234 },
    { 0x000025c8, 0x99fa1d0b },
    { 0x000025cc, 0x5095e154 },
    { 0x000025d0, 0x43624708 },
    { 0x000025d4, 0x863c31f2 },
    { 0x000025d8, 0x50c9086b },
    { 0x000025dc, 0x60f1b95e },
    { 0x000025e0, 0x746af305 },
    { 0x000025e4, 0x55ab76e3 },
    { 0x000025e8, 0xa2982da0 },
    { 0x000025ec, 0xd3a9afe5 },
    { 0x000025f0, 0x9fda7ab6 },
    { 0x000025f4, 0x2bad5848 },
    { 0x000025f8, 0xd5f3c7fa },
    { 0x000025fc, 0x76dc91c4 },
    { 0x00002600, 0xa16e9dce },
    { 0x00002604, 0x555a597d },
    { 0x00002608, 0x632ef299 },
    { 0x0000260c, 0x72c53c39 },
    { 0x00002610, 0xa0eedbf7 },
    { 0x00002614, 0xea9f7d99 },
    { 0x00002618, 0x12e603f4 },
    { 0x0000261c, 0x38220256 },
    { 0x00002880, 0x9e737ea9 },
    { 0x00002884, 0xf3ca795a },
    { 0x00002888, 0xd6b9d97b },
    { 0x0000288c, 0xa1bea6c3 },
    { 0x00002890, 0xa9b1cb27 },
    { 0x00002894, 0x48520970 },
    { 0x00002898, 0x30c4bc71 },
    { 0x0000289c, 0xf2ec412a },
    { 0x000028a0, 0x556d85c5 },
    { 0x000028a4, 0x2e6e17e8 },
    { 0x000028a8, 0x226cb437 },
    { 0x000028ac, 0xaa2fdfa4 },
    { 0x000028b0, 0x09d953ac },
    { 0x000028b4, 0x345fffd9 },
    { 0x000028b8, 0xe5aecede },
    { 0x000028bc, 0xc7e27f0e },
    { 0x000028c0, 0x968fe5d4 },
    { 0x000028c4, 0xaec77409 },
    { 0x000028c8, 0x98942a93 },
    { 0x000028cc, 0x7c3faa57 },
    { 0x000028d0, 0x904af287 },
    { 0x000028d4, 0x97fd46fa },
    { 0x000028d8, 0x07da94cb },
    { 0x000028dc, 0x04516f55 },
    { 0x00002b40, 0xb4d439a4 },
    { 0x00002b44, 0x0ca09e95 },
    { 0x00002b48, 0xe6e9aa79 },
    { 0x00002b4c, 0x21e5f8c3 },
    { 0x00002b50, 0x69d0c1c4 },
    { 0x00002b54, 0xd65defbe },
    { 0x00002b58, 0xde0b82a2 },
    { 0x00002b5c, 0xaba23784 },
    { 0x00002b60, 0x95e53478 },
    { 0x00002b64, 0xb6a1e123 },
    { 0x00002b68, 0xf2b15321 },
    { 0x00002b6c, 0x63282ab2 },
    { 0x00002b70, 0xc398f41c },
    { 0x00002b74, 0x16b486a5 },
    { 0x00002b78, 0xf92e606a },
    { 0x00002b7c, 0x1ba03ef8 },
    { 0x00002b80, 0x7e9f21c4 },
    { 0x00002b84, 0x6f56b3b8 },
    { 0x00002b88, 0x9809ed9b },
    { 0x00002b8c, 0x16c68f89 },
    { 0x00002b90, 0x852e5486 },
    { 0x00002b94, 0xbc09cf2f },
    { 0x00002b98, 0xfb52dacd },
    { 0x00002b9c, 0x1aa3b237 },
    { 0x00002e00, 0xcf527e7f },
    { 0x00002e04, 0x5f3baa1d },
    { 0x00002e08, 0xfaad1f65 },
    { 0x00002e0c, 0x433e2d13 },
    { 0x00002e10, 0x991629f2 },
    { 0x00002e14, 0x75388a4e },
    { 0x00002e18, 0xdb1152f9 },
    { 0x00002e1c, 0x2c6ef726 },
    { 0x00002e20, 0x2e2c2990 },
    { 0x00002e24, 0x80292629 },
    { 0x00002e28, 0x98a7e64c },
    { 0x00002e2c, 0x9408b6e5 },
    { 0x00002e30, 0x797472a1 },
    { 0x00002e34, 0x5fb6f1bc },
    { 0x00002e38, 0x45322186 },
    { 0x00002e3c, 0xd05ffa3e },
    { 0x00002e40, 0xfb7464ad },
    { 0x00002e44, 0x26195eb7 },
    { 0x00002e48, 0xc60fd5c7 },
    { 0x00002e4c, 0x2d39e9d7 },
    { 0x00002e50, 0x1d9f6d54 },
    { 0x00002e54, 0x613f77d6 },
    { 0x00002e58, 0x531695a2 },
    { 0x00002e5c, 0x837e423f },
    { 0x000030c0, 0x1ae92ed8 },
    { 0x000030c4, 0xbfd37a39 },
    { 0x000030c8, 0x77937219 },
    { 0x000030cc, 0xd6c5afe0 },
    { 0x000030d0, 0x699ec3b2 },
    { 0x000030d4, 0xda6e4ee6 },
    { 0x000030d8, 0x1f1c342d },
    { 0x000030dc, 0x30a5042c },
    { 0x000030e0, 0x8e95cc7a },
    { 0x000030e4, 0x06384cf7 },
    { 0x000030e8, 0x527e91ab },
    { 0x000030ec, 0xf5a5afe2 },
    { 0x000030f0, 0x8fd55144 },
    { 0x000030f4, 0xcd8d2fc1 },
    { 0x000030f8, 0xfe6f37a5 },
    { 0x000030fc, 0x19f141d4 },
    { 0x00003100, 0x89b84893 },
    { 0x00003104, 0x3e75ffe7 },
    { 0x00003108, 0x293d1ec0 },
    { 0x0000310c, 0x660f3ffe },
    { 0x00003110, 0xa196fe0c },
    { 0x00003114, 0xb8948b64 },
    { 0x00003118, 0xf233377f },
    { 0x0000311c, 0xba4b52c2 },
    { 0x00003380, 0x70c95716 },
    { 0x00003384, 0x81be5565 },
    { 0x00003388, 0x3fb589de },
    { 0x0000338c, 0xe2a2a68f },
    { 0x00003390, 0x5f524f9f },
    { 0x00003394, 0x24cab8b0 },
    { 0x00003398, 0xace8d171 },
    { 0x0000339c, 0x052f333c },
    { 0x000033a0, 0x935fd191 },
    { 0x000033a4, 0xee205fd2 },
    { 0x000033a8, 0x9737c277 },
    { 0x000033ac, 0xb1490b61 },
    { 0x000033b0, 0x65efdd8f },
    { 0x000033b4, 0x6d9deb2d },
    { 0x000033b8, 0xa049bb2b },
    { 0x000033bc, 0x66e482a6 },
    { 0x000033c0, 0xc28982bc },
    { 0x000033c4, 0x50dfb756 },
    { 0x000033c8, 0xaf099414 },
    { 0x000033cc, 0x694943c8 },
    { 0x000033d0, 0x3e0a1662 },
    { 0x000033d4, 0x1da0de9c },
    { 0x000033d8, 0x30fd5b81 },
    { 0x000033dc, 0x904a7dd4 },
    { 0x00003640, 0x54d867ed },
    { 0x00003644, 0x10ef628b },
    { 0x00003648, 0x888c4d10 },
    { 0x0000364c, 0x8c4e4788 },
    { 0x00003650, 0xa8548f0c },
    { 0x00003654, 0x01bb764a },
    { 0x00003658, 0x7cae1691 },
    { 0x0000365c, 0xcdca3d54 },
    { 0x00003660, 0x3bda1f9b },
    { 0x00003664, 0x5aba37ad },
    { 0x00003668, 0x50ad098a },
    { 0x0000366c, 0x2026bc93 },
    { 0x00003670, 0x9c373d31 },
    { 0x00003674, 0x20017136 },
    { 0x00003678, 0xe58f95d4 },
    { 0x0000367c, 0x70b80405 },
    { 0x00003680, 0x26fe32bb },
    { 0x00003684, 0x86b8659e },
    { 0x00003688, 0xc2fca1f9 },
    { 0x0000368c, 0x93f6ba84 },
    { 0x00003690, 0xa8329931 },
    { 0x00003694, 0xad42ba15 },
    { 0x00003698, 0xd5540152 },
    { 0x0000369c, 0x73e2cf8e },
    { 0x00003900, 0x0ab0aced },
    { 0x00003904, 0x5c66dfe5 },
    { 0x00003908, 0xbb25395d },
    { 0x0000390c, 0x0d3d3788 },
    { 0x00003910, 0x0eb88e52 },
    { 0x00003914, 0xe8817098 },
    { 0x00003918, 0xb9e131c9 },
    { 0x0000391c, 0x9a51fdb6 },
    { 0x00003920, 0x86703bcf },
    { 0x00003924, 0x3cffc0c1 },
    { 0x00003928, 0x2d836220 },
    { 0x0000392c, 0x4595a1e6 },
    { 0x00003930, 0x79daca9c },
    { 0x00003934, 0x128905a6 },
    { 0x00003938, 0x40d7bb0d },
    { 0x0000393c, 0x2b191e42 },
    { 0x00003940, 0x028217c1 },
    { 0x00003944, 0xee40168f },
    { 0x00003948, 0xb7842996 },
    { 0x0000394c, 0x349bd3b7 },
    { 0x00003950, 0x2ae6882c },
    { 0x00003954, 0xaa2a91cf },
    { 0x00003958, 0x74367a97 },
    { 0x0000395c, 0x573490f1 },
    { 0x00004600, 0x00977504 },
    { 0x00004608, 0x00b4dd50 },
    { 0x00004610, 0x00d9258a },
    { 0x00004618, 0x00bfc4db },
    { 0x00004620, 0x0037a440 },
    { 0x00004628, 0x008bfc7d },
    { 0x00004630, 0x0052ed50 },
    { 0x00004638, 0x0049ff9f },
    { 0x00004640, 0x000aaa90 },
    { 0x00004648, 0x00e0c068 },
    { 0x00004650, 0x0035aaa3 },
    { 0x00004658, 0x008d6f89 },
    { 0x000048c0, 0x008f7c0b },
    { 0x000048c8, 0x00d9881e },
    { 0x000048d0, 0x008ce484 },
    { 0x000048d8, 0x00ee3b80 },
    { 0x000048e0, 0x002a8303 },
    { 0x000048e8, 0x00d903b0 },
    { 0x000048f0, 0x00bf3b4e },
    { 0x000048f8, 0x0049ca04 },
    { 0x00004900, 0x0088b440 },
    { 0x00004908, 0x00b42be9 },
    { 0x00004910, 0x0050b239 },
    { 0x00004918, 0x00493f0f },
    { 0x00004b80, 0x000c818e },
    { 0x00004b88, 0x00e7194a },
    { 0x00004b90, 0x000eccd5 },
    { 0x00004b98, 0x0074f1f1 },
    { 0x00004ba0, 0x00a4e086 },
    { 0x00004ba8, 0x002a4ba9 },
    { 0x00004bb0, 0x00dc6877 },
    { 0x00004bb8, 0x006b2cf6 },
    { 0x00004bc0, 0x008efebe },
    { 0x00004bc8, 0x0018c6dc },
    { 0x00004bd0, 0x0017c187 },
    { 0x00004bd8, 0x00373597 },
    { 0x00004e40, 0x006c62e0 },
    { 0x00004e48, 0x00b405ae },
    { 0x00004e50, 0x009736e3 },
    { 0x00004e58, 0x007cf924 },
    { 0x00004e60, 0x00636201 },
    { 0x00004e68, 0x0060c159 },
    { 0x00004e70, 0x0068f0dc },
    { 0x00004e78, 0x0097c090 },
    { 0x00004e80, 0x00196d8e },
    { 0x00004e88, 0x00279081 },
    { 0x00004e90, 0x00d91561 },
    { 0x00004e98, 0x005f8a67 },
    { 0x00005100, 0x0040dc72 },
    { 0x00005108, 0x002d5692 },
    { 0x00005110, 0x0036bc6a },
    { 0x00005118, 0x001b1f8c },
    { 0x00005120, 0x00ca2beb },
    { 0x00005128, 0x006747bf },
    { 0x00005130, 0x0014585a },
    { 0x00005138, 0x001c331a },
    { 0x00005140, 0x00741f30 },
    { 0x00005148, 0x00b0c052 },
    { 0x00005150, 0x00d558d2 },
    { 0x00005158, 0x0038e564 },
    { 0x000053c0, 0x00e48662 },
    { 0x000053c8, 0x00bb5648 },
    { 0x000053d0, 0x0098b0c5 },
    { 0x000053d8, 0x00dbbda6 },
    { 0x000053e0, 0x006c8bc1 },
    { 0x000053e8, 0x000b58e4 },
    { 0x000053f0, 0x00e61a66 },
    { 0x000053f8, 0x00a1e1db },
    { 0x00005400, 0x00cfed95 },
    { 0x00005408, 0x00b5e9ef },
    { 0x00005410, 0x00af2b9d },
    { 0x00005418, 0x000bbb52 },
    { 0x00005680, 0x0038d113 },
    { 0x00005688, 0x00a994d5 },
    { 0x00005690, 0x00739b1e },
    { 0x00005698, 0x00fb69d3 },
    { 0x000056a0, 0x00a0ae75 },
    { 0x000056a8, 0x00252b88 },
    { 0x000056b0, 0x006ca7a1 },
    { 0x000056b8, 0x00d2de54 },
    { 0x000056c0, 0x00b784d1 },
    { 0x000056c8, 0x009e6c22 },
    { 0x000056d0, 0x0001df2a },
    { 0x000056d8, 0x00d20bdd },
    { 0x00005940, 0x00e18e73 },
    { 0x00005948, 0x0036b5f4 },
    { 0x00005950, 0x00755c84 },
    { 0x00005958, 0x00db6f13 },
    { 0x00005960, 0x009dd9bd },
    { 0x00005968, 0x0008d415 },
    { 0x00005970, 0x0022f03d },
    { 0x00005978, 0x00d2f06d },
    { 0x00005980, 0x0077c176 },
    { 0x00005988, 0x00666bf5 },
    { 0x00005990, 0x0073b8a6 },
    { 0x00005998, 0x002eba61 },
    { 0x00005c00, 0x00b5211a },
    { 0x00005c08, 0x003dc1f4 },
    { 0x00005c10, 0x0040219d },
    { 0x00005c18, 0x00db2d53 },
    { 0x00005c20, 0x00e21c67 },
    { 0x00005c28, 0x0014703b },
    { 0x00005c30, 0x002114d7 },
    { 0x00005c38, 0x007097aa },
    { 0x00005c40, 0x00b928ba },
    { 0x00005c48, 0x009ec6ab },
    { 0x00005c50, 0x002653dc },
    { 0x00005c58, 0x00258b9e },
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

    { /* Zero-init region 0xc0800000 size 0x6900 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 28672, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0800000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 26880); munmap(zpage, 28672); }
    }
    { /* Zero-init region 0xc0000000 size 0x1e6c0 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 126976, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 124608); munmap(zpage, 126976); }
    }

    /* Load embedded data (replaces devmem load scripts) */
    { /* Data: base=0xc0800000, 540 writes */
        uint8_t* dpage = (uint8_t*)mmap(NULL, 24576, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0800000);
        if (dpage != MAP_FAILED) {
            const uint32_t (*dp)[2] = embed_0_data;
            uint32_t base_off = 0x0;
            while ((*dp)[0] != 0xFFFFFFFF) {
                *(volatile uint32_t*)(dpage + base_off + (*dp)[0]) = (*dp)[1];
                dp++;
            }
            munmap(dpage, 24576);
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
    { /* CRC[0] zero-init: addr=0xc0000000 size=0x1e6c0 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 126976, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 124608); munmap(zpage, 126976); }
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
    { /* CRC[0]: addr=0xc0000000, size=0x1e6c0, expect=0x388f5f72 */
        uint8_t* page = (uint8_t*)mmap(NULL, 126976, PROT_READ, MAP_SHARED, fd, 0xc0000000);
        if (page != MAP_FAILED) {
            uint32_t crc = calc_crc32(page + 0, 124608);
            printf("CRC[0] = 0x%08x (expect 0x388f5f72) %s\n", crc, crc == 0x388f5f72 ? "PASS" : "FAIL");
            if (crc != 0x388f5f72) failures++;
            munmap(page, 126976);
        } else { perror("mmap out"); }
    }

    printf("Results: %d failures\n", failures);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return failures;
}