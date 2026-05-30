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
    { 0xa01c, 0xc0800000 }, /* NVDLA_PDP_RDMA_D_SRC_BASE_ADDR_LOW_0 */
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

/* Embedded data blocks (loaded via mmap instead of devmem scripts) */
static const uint32_t embed_0_data[][2] = {
    { 0x00000000, 0xbd7e3f00 },
    { 0x00000004, 0xb97a3bfc },
    { 0x00000008, 0xbe7f4001 },
    { 0x0000000c, 0xba7b3cfd },
    { 0x00000010, 0xbf804102 },
    { 0x00000014, 0xbb7c3dfe },
    { 0x00000018, 0xc0814203 },
    { 0x0000001c, 0xbc7d3eff },
    { 0x00000020, 0xc1824304 },
    { 0x00000024, 0xbd7e3f00 },
    { 0x00000028, 0xc2834405 },
    { 0x0000002c, 0xbe7f4001 },
    { 0x00000030, 0xc3844506 },
    { 0x00000034, 0xbf804102 },
    { 0x000000a0, 0xc4854607 },
    { 0x000000a4, 0xc0814203 },
    { 0x000000a8, 0xc5864708 },
    { 0x000000ac, 0xc1824304 },
    { 0x000000b0, 0xc6874809 },
    { 0x000000b4, 0xc2834405 },
    { 0x000000b8, 0xc788490a },
    { 0x000000bc, 0xc3844506 },
    { 0x000000c0, 0xc8894a0b },
    { 0x000000c4, 0xc4854607 },
    { 0x000000c8, 0xc98a4b0c },
    { 0x000000cc, 0xc5864708 },
    { 0x000000d0, 0xca8b4c0d },
    { 0x000000d4, 0xc6874809 },
    { 0x00000140, 0xcb8c4d0e },
    { 0x00000144, 0xc788490a },
    { 0x00000148, 0xcc8d4e0f },
    { 0x0000014c, 0xc8894a0b },
    { 0x00000150, 0xcd8e4f10 },
    { 0x00000154, 0xc98a4b0c },
    { 0x00000158, 0xce8f5011 },
    { 0x0000015c, 0xca8b4c0d },
    { 0x00000160, 0xcf905112 },
    { 0x00000164, 0xcb8c4d0e },
    { 0x00000168, 0xd0915213 },
    { 0x0000016c, 0xcc8d4e0f },
    { 0x00000170, 0xd1925314 },
    { 0x00000174, 0xcd8e4f10 },
    { 0x000001e0, 0xd2935415 },
    { 0x000001e4, 0xce8f5011 },
    { 0x000001e8, 0xd3945516 },
    { 0x000001ec, 0xcf905112 },
    { 0x000001f0, 0xd4955617 },
    { 0x000001f4, 0xd0915213 },
    { 0x000001f8, 0xd5965718 },
    { 0x000001fc, 0xd1925314 },
    { 0x00000200, 0xd6975819 },
    { 0x00000204, 0xd2935415 },
    { 0x00000208, 0xd798591a },
    { 0x0000020c, 0xd3945516 },
    { 0x00000210, 0xd8995a1b },
    { 0x00000214, 0xd4955617 },
    { 0x00000280, 0xd99a5b1c },
    { 0x00000284, 0xd5965718 },
    { 0x00000288, 0xda9b5c1d },
    { 0x0000028c, 0xd6975819 },
    { 0x00000290, 0xdb9c5d1e },
    { 0x00000294, 0xd798591a },
    { 0x00000298, 0xdc9d5e1f },
    { 0x0000029c, 0xd8995a1b },
    { 0x000002a0, 0xdd9e5f20 },
    { 0x000002a4, 0xd99a5b1c },
    { 0x000002a8, 0xde9f6021 },
    { 0x000002ac, 0xda9b5c1d },
    { 0x000002b0, 0xdfa06122 },
    { 0x000002b4, 0xdb9c5d1e },
    { 0x00000320, 0xe0a16223 },
    { 0x00000324, 0xdc9d5e1f },
    { 0x00000328, 0xe1a26324 },
    { 0x0000032c, 0xdd9e5f20 },
    { 0x00000330, 0xe2a36425 },
    { 0x00000334, 0xde9f6021 },
    { 0x00000338, 0xe3a46526 },
    { 0x0000033c, 0xdfa06122 },
    { 0x00000340, 0xe4a56627 },
    { 0x00000344, 0xe0a16223 },
    { 0x00000348, 0xe5a66728 },
    { 0x0000034c, 0xe1a26324 },
    { 0x00000350, 0xe6a76829 },
    { 0x00000354, 0xe2a36425 },
    { 0x000003c0, 0xe7a8692a },
    { 0x000003c4, 0xe3a46526 },
    { 0x000003c8, 0xe8a96a2b },
    { 0x000003cc, 0xe4a56627 },
    { 0x000003d0, 0xe9aa6b2c },
    { 0x000003d4, 0xe5a66728 },
    { 0x000003d8, 0xeaab6c2d },
    { 0x000003dc, 0xe6a76829 },
    { 0x000003e0, 0xebac6d2e },
    { 0x000003e4, 0xe7a8692a },
    { 0x000003e8, 0xecad6e2f },
    { 0x000003ec, 0xe8a96a2b },
    { 0x000003f0, 0xedae6f30 },
    { 0x000003f4, 0xe9aa6b2c },
    { 0x00000460, 0xeeaf7031 },
    { 0x00000464, 0xeaab6c2d },
    { 0x00000468, 0xefb07132 },
    { 0x0000046c, 0xebac6d2e },
    { 0x00000470, 0xf0b17233 },
    { 0x00000474, 0xecad6e2f },
    { 0x00000478, 0xf1b27334 },
    { 0x0000047c, 0xedae6f30 },
    { 0x00000480, 0xf2b37435 },
    { 0x00000484, 0xeeaf7031 },
    { 0x00000488, 0xf3b47536 },
    { 0x0000048c, 0xefb07132 },
    { 0x00000490, 0xf4b57637 },
    { 0x00000494, 0xf0b17233 },
    { 0x00000500, 0xf5b67738 },
    { 0x00000504, 0xf1b27334 },
    { 0x00000508, 0xf6b77839 },
    { 0x0000050c, 0xf2b37435 },
    { 0x00000510, 0xf7b8793a },
    { 0x00000514, 0xf3b47536 },
    { 0x00000518, 0xf8b97a3b },
    { 0x0000051c, 0xf4b57637 },
    { 0x00000520, 0xf9ba7b3c },
    { 0x00000524, 0xf5b67738 },
    { 0x00000528, 0xfabb7c3d },
    { 0x0000052c, 0xf6b77839 },
    { 0x00000530, 0xfbbc7d3e },
    { 0x00000534, 0xf7b8793a },
    { 0x00000880, 0x000037f8 },
    { 0x00000888, 0x000038f9 },
    { 0x00000890, 0x000039fa },
    { 0x00000898, 0x00003afb },
    { 0x000008a0, 0x00003bfc },
    { 0x000008a8, 0x00003cfd },
    { 0x000008b0, 0x00003dfe },
    { 0x00000920, 0x00003eff },
    { 0x00000928, 0x00003f00 },
    { 0x00000930, 0x00004001 },
    { 0x00000938, 0x00004102 },
    { 0x00000940, 0x00004203 },
    { 0x00000948, 0x00004304 },
    { 0x00000950, 0x00004405 },
    { 0x000009c0, 0x00004506 },
    { 0x000009c8, 0x00004607 },
    { 0x000009d0, 0x00004708 },
    { 0x000009d8, 0x00004809 },
    { 0x000009e0, 0x0000490a },
    { 0x000009e8, 0x00004a0b },
    { 0x000009f0, 0x00004b0c },
    { 0x00000a60, 0x00004c0d },
    { 0x00000a68, 0x00004d0e },
    { 0x00000a70, 0x00004e0f },
    { 0x00000a78, 0x00004f10 },
    { 0x00000a80, 0x00005011 },
    { 0x00000a88, 0x00005112 },
    { 0x00000a90, 0x00005213 },
    { 0x00000b00, 0x00005314 },
    { 0x00000b08, 0x00005415 },
    { 0x00000b10, 0x00005516 },
    { 0x00000b18, 0x00005617 },
    { 0x00000b20, 0x00005718 },
    { 0x00000b28, 0x00005819 },
    { 0x00000b30, 0x0000591a },
    { 0x00000ba0, 0x00005a1b },
    { 0x00000ba8, 0x00005b1c },
    { 0x00000bb0, 0x00005c1d },
    { 0x00000bb8, 0x00005d1e },
    { 0x00000bc0, 0x00005e1f },
    { 0x00000bc8, 0x00005f20 },
    { 0x00000bd0, 0x00006021 },
    { 0x00000c40, 0x00006122 },
    { 0x00000c48, 0x00006223 },
    { 0x00000c50, 0x00006324 },
    { 0x00000c58, 0x00006425 },
    { 0x00000c60, 0x00006526 },
    { 0x00000c68, 0x00006627 },
    { 0x00000c70, 0x00006728 },
    { 0x00000ce0, 0x00006829 },
    { 0x00000ce8, 0x0000692a },
    { 0x00000cf0, 0x00006a2b },
    { 0x00000cf8, 0x00006b2c },
    { 0x00000d00, 0x00006c2d },
    { 0x00000d08, 0x00006d2e },
    { 0x00000d10, 0x00006e2f },
    { 0x00000d80, 0x00006f30 },
    { 0x00000d88, 0x00007031 },
    { 0x00000d90, 0x00007132 },
    { 0x00000d98, 0x00007233 },
    { 0x00000da0, 0x00007334 },
    { 0x00000da8, 0x00007435 },
    { 0x00000db0, 0x00007536 },
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

    { /* Zero-init region 0xc0800000 size 0x1100 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0800000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 4352); munmap(zpage, 8192); }
    }
    { /* Zero-init region 0xc0000000 size 0x6140 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 28672, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 24896); munmap(zpage, 28672); }
    }

    /* Load embedded data (replaces devmem load scripts) */
    { /* Data: base=0xc0800000, 189 writes */
        uint8_t* dpage = (uint8_t*)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0800000);
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

    printf("Programming initial registers...\n");
    const struct dla_reg* p = program_initial;
    while (p->offset != -1) {
        *(volatile uint32_t*)(dla_mmio + p->offset) = p->value;
        p++;
    }
    printf("Done initial registers.\n");
    /* Zero-init CRC output regions to prevent false positives */
    { /* CRC[0] zero-init: addr=0xc0000000 size=0x6140 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 28672, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 24896); munmap(zpage, 28672); }
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