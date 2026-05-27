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
    { 0xb054, 0x00000055 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_5_CFG_0 */
    { 0xb028, 0x00000001 }, /* NVDLA_PDP_D_NAN_FLUSH_TO_ZERO_0 */
    { 0xb020, 0x00000012 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_CHANNEL_0 */
    { 0xb060, 0xc0800000 }, /* NVDLA_PDP_D_SRC_BASE_ADDR_LOW_0 */
    { 0xb08c, 0x00000000 }, /* NVDLA_PDP_D_NAN_INPUT_NUM_0 */
    { 0xb034, 0x00110202 }, /* NVDLA_PDP_D_POOLING_KERNEL_CFG_0 */
    { 0xb01c, 0x00000005 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_HEIGHT_0 */
    { 0xb09c, 0xd76bec80 }, /* NVDLA_PDP_D_CYA_0 */
    { 0xb080, 0x00000001 }, /* NVDLA_PDP_D_DST_RAM_CFG_0 */
    { 0xb040, 0x00002021 }, /* NVDLA_PDP_D_POOLING_PADDING_CFG_0 */
    { 0xb024, 0x00000010 }, /* NVDLA_PDP_D_OPERATION_MODE_CFG_0 */
    { 0xb038, 0x00005555 }, /* NVDLA_PDP_D_RECIP_KERNEL_WIDTH_0 */
    { 0xb06c, 0x00002240 }, /* NVDLA_PDP_D_SRC_SURFACE_STRIDE_0 */
    { 0xb094, 0x00000000 }, /* NVDLA_PDP_D_PERF_ENABLE_0 */
    { 0xb05c, 0x00000077 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_7_CFG_0 */
    { 0xb014, 0x00000012 }, /* NVDLA_PDP_D_DATA_CUBE_IN_CHANNEL_0 */
    { 0xb050, 0x00000044 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_4_CFG_0 */
    { 0xb068, 0x00000280 }, /* NVDLA_PDP_D_SRC_LINE_STRIDE_0 */
    { 0xb030, 0x2c5f1256 }, /* NVDLA_PDP_D_PARTIAL_WIDTH_OUT_0 */
    { 0xb010, 0x00000008 }, /* NVDLA_PDP_D_DATA_CUBE_IN_HEIGHT_0 */
    { 0xb000, 0x00000000 }, /* NVDLA_PDP_S_STATUS_0 */
    { 0xb058, 0x00000066 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_6_CFG_0 */
    { 0xb088, 0x00000000 }, /* NVDLA_PDP_D_INF_INPUT_NUM_0 */
    { 0xb07c, 0x0000c000 }, /* NVDLA_PDP_D_DST_SURFACE_STRIDE_0 */
    { 0xb074, 0x00000000 }, /* NVDLA_PDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0xb048, 0x00000022 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_2_CFG_0 */
    { 0xb098, 0x00000000 }, /* NVDLA_PDP_D_PERF_WRITE_STALL_0 */
    { 0xb070, 0xc0000000 }, /* NVDLA_PDP_D_DST_BASE_ADDR_LOW_0 */
    { 0xb084, 0x00000000 }, /* NVDLA_PDP_D_DATA_FORMAT_0 */
    { 0xb018, 0x00000003 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_WIDTH_0 */
    { 0xb090, 0x00000000 }, /* NVDLA_PDP_D_NAN_OUTPUT_NUM_0 */
    { 0xb078, 0x00002000 }, /* NVDLA_PDP_D_DST_LINE_STRIDE_0 */
    { 0xb03c, 0x00005555 }, /* NVDLA_PDP_D_RECIP_KERNEL_HEIGHT_0 */
    { 0xb00c, 0x00000007 }, /* NVDLA_PDP_D_DATA_CUBE_IN_WIDTH_0 */
    { 0xb064, 0x00000000 }, /* NVDLA_PDP_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xb04c, 0x00000033 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_3_CFG_0 */
    { 0xb02c, 0x2eb9f0bb }, /* NVDLA_PDP_D_PARTIAL_WIDTH_IN_0 */
    { 0xb044, 0x00000011 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_1_CFG_0 */
    { 0xa004, 0x00000000 }, /* NVDLA_PDP_RDMA_S_POINTER_0 */
    { 0xa010, 0x00000008 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_HEIGHT_0 */
    { 0xa028, 0x00002240 }, /* NVDLA_PDP_RDMA_D_SRC_SURFACE_STRIDE_0 */
    { 0xa030, 0x00000000 }, /* NVDLA_PDP_RDMA_D_DATA_FORMAT_0 */
    { 0xa020, 0x00000000 }, /* NVDLA_PDP_RDMA_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xa000, 0x00000000 }, /* NVDLA_PDP_RDMA_S_STATUS_0 */
    { 0xa040, 0x2eb9f0bb }, /* NVDLA_PDP_RDMA_D_PARTIAL_WIDTH_IN_0 */
    { 0xa014, 0x00000012 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_CHANNEL_0 */
    { 0xa00c, 0x00000007 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_WIDTH_0 */
    { 0xa034, 0x00000000 }, /* NVDLA_PDP_RDMA_D_OPERATION_MODE_CFG_0 */
    { 0xa018, 0x00000001 }, /* NVDLA_PDP_RDMA_D_FLYING_MODE_0 */
    { 0xa044, 0x00000001 }, /* NVDLA_PDP_RDMA_D_PERF_ENABLE_0 */
    { 0xa04c, 0xc09253f8 }, /* NVDLA_PDP_RDMA_D_CYA_0 */
    { 0xa01c, 0xc0800000 }, /* NVDLA_PDP_RDMA_D_SRC_BASE_ADDR_LOW_0 */
    { 0xa048, 0x00000000 }, /* NVDLA_PDP_RDMA_D_PERF_READ_STALL_0 */
    { 0xa02c, 0x00000001 }, /* NVDLA_PDP_RDMA_D_SRC_RAM_CFG_0 */
    { 0xa024, 0x00000280 }, /* NVDLA_PDP_RDMA_D_SRC_LINE_STRIDE_0 */
    { 0xa038, 0x00000012 }, /* NVDLA_PDP_RDMA_D_POOLING_KERNEL_CFG_0 */
    { 0xa03c, 0x00000005 }, /* NVDLA_PDP_RDMA_D_POOLING_PADDING_CFG_0 */
    { 0xb008, 0x00000001 }, /* NVDLA_PDP_D_OP_ENABLE_0 */
    { 0xa008, 0x00000001 }, /* NVDLA_PDP_RDMA_D_OP_ENABLE_0 */
    {-1, 0}
};

static const struct dla_reg program_enable[] = {
    {-1, 0}
};

/* Embedded data blocks (loaded via mmap instead of devmem scripts) */
static const uint32_t embed_0_data[][2] = {
    { 0x00000000, 0xb54aa0b9 },
    { 0x00000004, 0x88ef65a6 },
    { 0x00000008, 0x27a508e6 },
    { 0x0000000c, 0x7a0df00c },
    { 0x00000010, 0x5155bd09 },
    { 0x00000014, 0x0572fd40 },
    { 0x00000018, 0x2929f5b8 },
    { 0x0000001c, 0xb0b41034 },
    { 0x00000020, 0x47b69573 },
    { 0x00000024, 0x2d68e5dd },
    { 0x00000028, 0x55fe5376 },
    { 0x0000002c, 0x37ff2dca },
    { 0x00000030, 0xc87dec69 },
    { 0x00000034, 0x4e7d656c },
    { 0x00000038, 0x17e60393 },
    { 0x0000003c, 0xc077df5e },
    { 0x00000280, 0x52330bc5 },
    { 0x00000284, 0xf620e868 },
    { 0x00000288, 0x1790afdb },
    { 0x0000028c, 0x54bb377a },
    { 0x00000290, 0x74459303 },
    { 0x00000294, 0x2f2525a9 },
    { 0x00000298, 0xf1602a99 },
    { 0x0000029c, 0x776f6a8f },
    { 0x000002a0, 0x1e6b1e9b },
    { 0x000002a4, 0x3cbe857e },
    { 0x000002a8, 0x940fb995 },
    { 0x000002ac, 0x6d3c6056 },
    { 0x000002b0, 0x089f6d2d },
    { 0x000002b4, 0x884e058a },
    { 0x000002b8, 0x8bc2776d },
    { 0x000002bc, 0x3f4eaf26 },
    { 0x00000500, 0xc8e04d68 },
    { 0x00000504, 0x77cb21fa },
    { 0x00000508, 0x7eb07e2e },
    { 0x0000050c, 0xda233fad },
    { 0x00000510, 0xb28c39a0 },
    { 0x00000514, 0x322a3d04 },
    { 0x00000518, 0x011a7018 },
    { 0x0000051c, 0x9e53382e },
    { 0x00000520, 0x210b6c19 },
    { 0x00000524, 0xfe2f337e },
    { 0x00000528, 0x96cbf98c },
    { 0x0000052c, 0x89731aef },
    { 0x00000530, 0x981f4745 },
    { 0x00000534, 0xfee50beb },
    { 0x00000538, 0xcc3ec225 },
    { 0x0000053c, 0x54528fae },
    { 0x00000780, 0x7fda81be },
    { 0x00000784, 0xb7697666 },
    { 0x00000788, 0x2de2e487 },
    { 0x0000078c, 0x192d08e1 },
    { 0x00000790, 0xd7df089c },
    { 0x00000794, 0x1277503b },
    { 0x00000798, 0xbadc4841 },
    { 0x0000079c, 0x8e677f39 },
    { 0x000007a0, 0x42167c2a },
    { 0x000007a4, 0xec70cc80 },
    { 0x000007a8, 0x7c55d8c1 },
    { 0x000007ac, 0x8ff4c0cd },
    { 0x000007b0, 0x736f46e6 },
    { 0x000007b4, 0x9f1455d1 },
    { 0x000007b8, 0x9bd15639 },
    { 0x000007bc, 0x178d5252 },
    { 0x00000a00, 0x54d35284 },
    { 0x00000a04, 0x539feb88 },
    { 0x00000a08, 0xe2c9b0a4 },
    { 0x00000a0c, 0x585afae1 },
    { 0x00000a10, 0x9e990fad },
    { 0x00000a14, 0x86c36416 },
    { 0x00000a18, 0x6a1904c1 },
    { 0x00000a1c, 0x80f4ff35 },
    { 0x00000a20, 0x29957b90 },
    { 0x00000a24, 0xe1912f77 },
    { 0x00000a28, 0x54ddc216 },
    { 0x00000a2c, 0xe7529c82 },
    { 0x00000a30, 0x361e967a },
    { 0x00000a34, 0x1cec044a },
    { 0x00000a38, 0xb5e2aac0 },
    { 0x00000a3c, 0xdab4462e },
    { 0x00000c80, 0x3750dde2 },
    { 0x00000c84, 0x25238f3b },
    { 0x00000c88, 0x2c9df8b6 },
    { 0x00000c8c, 0x55626c5f },
    { 0x00000c90, 0x9d25fb8c },
    { 0x00000c94, 0x19aca981 },
    { 0x00000c98, 0x66d312db },
    { 0x00000c9c, 0x192717b7 },
    { 0x00000ca0, 0x509d32a5 },
    { 0x00000ca4, 0x35a15f56 },
    { 0x00000ca8, 0x17247e19 },
    { 0x00000cac, 0x661cea7d },
    { 0x00000cb0, 0xf5389c01 },
    { 0x00000cb4, 0xb87829c4 },
    { 0x00000cb8, 0xed9319f1 },
    { 0x00000cbc, 0xf2a72bd5 },
    { 0x00000f00, 0x8af372ec },
    { 0x00000f04, 0x8e700af8 },
    { 0x00000f08, 0xaefa0dd5 },
    { 0x00000f0c, 0x94c1e663 },
    { 0x00000f10, 0x602af778 },
    { 0x00000f14, 0xc2ca6a01 },
    { 0x00000f18, 0x7c15c4ea },
    { 0x00000f1c, 0xd45a8931 },
    { 0x00000f20, 0x91a0e763 },
    { 0x00000f24, 0xf25faa73 },
    { 0x00000f28, 0x8481b6a1 },
    { 0x00000f2c, 0xee044eda },
    { 0x00000f30, 0xd53814ba },
    { 0x00000f34, 0xc54ba162 },
    { 0x00000f38, 0x01d136eb },
    { 0x00000f3c, 0xb1563248 },
    { 0x00001180, 0xfc58e16c },
    { 0x00001184, 0xbd69dfb6 },
    { 0x00001188, 0x4cd4b086 },
    { 0x0000118c, 0xa11a1633 },
    { 0x00001190, 0xd755ed36 },
    { 0x00001194, 0x323ba689 },
    { 0x00001198, 0x98d76e4e },
    { 0x0000119c, 0x60455690 },
    { 0x000011a0, 0x37acc41a },
    { 0x000011a4, 0x532d3c1f },
    { 0x000011a8, 0x06f5b756 },
    { 0x000011ac, 0x19de68a0 },
    { 0x000011b0, 0x2ad429e7 },
    { 0x000011b4, 0xb931982c },
    { 0x000011b8, 0xf29c8a11 },
    { 0x000011bc, 0x2273e02f },
    { 0x00001400, 0xde1211ca },
    { 0x00001404, 0xf4ed6df7 },
    { 0x00001408, 0x28dd8f5e },
    { 0x0000140c, 0x999c8467 },
    { 0x00001410, 0x7727e63d },
    { 0x00001414, 0x76afc1b8 },
    { 0x00001418, 0x47035abe },
    { 0x0000141c, 0x3785ded5 },
    { 0x00001420, 0x5f7fca0c },
    { 0x00001424, 0xbaa66be8 },
    { 0x00001428, 0xb7a9b40a },
    { 0x0000142c, 0x77b7e555 },
    { 0x00001430, 0x5f1e6b90 },
    { 0x00001434, 0x8b98e975 },
    { 0x00001438, 0x14f62baa },
    { 0x0000143c, 0x6cf5394a },
    { 0x00002240, 0x3dac8c37 },
    { 0x00002244, 0xb07d11ab },
    { 0x00002248, 0x44c48e5c },
    { 0x0000224c, 0x39fedcc7 },
    { 0x00002250, 0x97b99539 },
    { 0x00002254, 0xb621da1e },
    { 0x00002258, 0x402f2416 },
    { 0x0000225c, 0xc5d52f83 },
    { 0x00002260, 0xbcb01d7e },
    { 0x00002264, 0x21a8f820 },
    { 0x00002268, 0x34a9b538 },
    { 0x0000226c, 0xc6f755c4 },
    { 0x00002270, 0x1407958d },
    { 0x00002274, 0xc22bd304 },
    { 0x00002278, 0x44d48898 },
    { 0x0000227c, 0xad09be70 },
    { 0x000024c0, 0x1616fe86 },
    { 0x000024c4, 0x37a0def3 },
    { 0x000024c8, 0x1c1f7238 },
    { 0x000024cc, 0x28735f26 },
    { 0x000024d0, 0xb292997c },
    { 0x000024d4, 0x631aa083 },
    { 0x000024d8, 0xcf720e06 },
    { 0x000024dc, 0x557a2fb5 },
    { 0x000024e0, 0xc3b1be81 },
    { 0x000024e4, 0x6ee5d0fe },
    { 0x000024e8, 0x418b0a5c },
    { 0x000024ec, 0x983c86e4 },
    { 0x000024f0, 0x8ed5045e },
    { 0x000024f4, 0x5ab6b434 },
    { 0x000024f8, 0x42202c1d },
    { 0x000024fc, 0xde302f97 },
    { 0x00002740, 0x6d38792e },
    { 0x00002744, 0xd487102b },
    { 0x00002748, 0xb8ce21dc },
    { 0x0000274c, 0xfff266aa },
    { 0x00002750, 0xde9422d2 },
    { 0x00002754, 0x34a14e66 },
    { 0x00002758, 0x79cd163f },
    { 0x0000275c, 0x2de24870 },
    { 0x00002760, 0x7b886164 },
    { 0x00002764, 0x3b172753 },
    { 0x00002768, 0x74a8c036 },
    { 0x0000276c, 0x9e99522a },
    { 0x00002770, 0x8b40af37 },
    { 0x00002774, 0xbeb9e80a },
    { 0x00002778, 0xd76352e9 },
    { 0x0000277c, 0xb2c2442d },
    { 0x000029c0, 0x395e50ab },
    { 0x000029c4, 0x7b5f3b79 },
    { 0x000029c8, 0x05a7b16d },
    { 0x000029cc, 0xa2d27177 },
    { 0x000029d0, 0xc1e27158 },
    { 0x000029d4, 0xf5f1aeeb },
    { 0x000029d8, 0x19aaa60b },
    { 0x000029dc, 0x20b8b5d3 },
    { 0x000029e0, 0x0cf709bb },
    { 0x000029e4, 0x2ddbedb1 },
    { 0x000029e8, 0x29316f3e },
    { 0x000029ec, 0x83a93b2d },
    { 0x000029f0, 0x98015254 },
    { 0x000029f4, 0xe1f35752 },
    { 0x000029f8, 0x3f1a3a55 },
    { 0x000029fc, 0x5775db2b },
    { 0x00002c40, 0x76918b52 },
    { 0x00002c44, 0x61401955 },
    { 0x00002c48, 0x449ac9c0 },
    { 0x00002c4c, 0xca8626ab },
    { 0x00002c50, 0x779e3665 },
    { 0x00002c54, 0x09071e16 },
    { 0x00002c58, 0x4557dc12 },
    { 0x00002c5c, 0x55807404 },
    { 0x00002c60, 0x2a4cbb3d },
    { 0x00002c64, 0x1cc3b662 },
    { 0x00002c68, 0x2ac3040a },
    { 0x00002c6c, 0xf3eeaaea },
    { 0x00002c70, 0x16455c5f },
    { 0x00002c74, 0x897bff53 },
    { 0x00002c78, 0x93f63ae9 },
    { 0x00002c7c, 0xd69b21d9 },
    { 0x00002ec0, 0x432419aa },
    { 0x00002ec4, 0xe14021f2 },
    { 0x00002ec8, 0x50163bc2 },
    { 0x00002ecc, 0xa6083b35 },
    { 0x00002ed0, 0x515170d2 },
    { 0x00002ed4, 0x05a4671c },
    { 0x00002ed8, 0x305cbc75 },
    { 0x00002edc, 0x3ebb6e0c },
    { 0x00002ee0, 0xae42bced },
    { 0x00002ee4, 0xdb918295 },
    { 0x00002ee8, 0x3c905a4a },
    { 0x00002eec, 0x2cd4c8d2 },
    { 0x00002ef0, 0x9f2b738e },
    { 0x00002ef4, 0x377587b9 },
    { 0x00002ef8, 0xd33e3dcf },
    { 0x00002efc, 0x39a1faca },
    { 0x00003140, 0x1b72c5a0 },
    { 0x00003144, 0x368d49fa },
    { 0x00003148, 0x04745b41 },
    { 0x0000314c, 0x2b58132e },
    { 0x00003150, 0xe5cab016 },
    { 0x00003154, 0x92203031 },
    { 0x00003158, 0x0be5aa8f },
    { 0x0000315c, 0xb3e73609 },
    { 0x00003160, 0xc1993965 },
    { 0x00003164, 0xa92759bf },
    { 0x00003168, 0x4971bdae },
    { 0x0000316c, 0xd6f24e22 },
    { 0x00003170, 0x967d93bb },
    { 0x00003174, 0xba9a226a },
    { 0x00003178, 0x0541af26 },
    { 0x0000317c, 0x76e582a4 },
    { 0x000033c0, 0xc0670b1c },
    { 0x000033c4, 0xa1c7a49f },
    { 0x000033c8, 0xd9698062 },
    { 0x000033cc, 0x78c26290 },
    { 0x000033d0, 0xb4730fa3 },
    { 0x000033d4, 0x816c8a17 },
    { 0x000033d8, 0x887e09de },
    { 0x000033dc, 0x26aaf2f2 },
    { 0x000033e0, 0x977ab266 },
    { 0x000033e4, 0x9756d2c8 },
    { 0x000033e8, 0xcea2768b },
    { 0x000033ec, 0x86c1b543 },
    { 0x000033f0, 0xf89de3e9 },
    { 0x000033f4, 0xd4165cec },
    { 0x000033f8, 0xe4b89e8f },
    { 0x000033fc, 0x1cb01459 },
    { 0x00003640, 0x692621cd },
    { 0x00003644, 0x1e26bdc4 },
    { 0x00003648, 0x54cd6013 },
    { 0x0000364c, 0xffbfa6d9 },
    { 0x00003650, 0xbbb88271 },
    { 0x00003654, 0x7eb38fc7 },
    { 0x00003658, 0x5462a5a4 },
    { 0x0000365c, 0x90610850 },
    { 0x00003660, 0xa0c0878e },
    { 0x00003664, 0x8c0f65e0 },
    { 0x00003668, 0xbc89fbdc },
    { 0x0000366c, 0xcf901a7d },
    { 0x00003670, 0x9d498bf3 },
    { 0x00003674, 0xfc399fd2 },
    { 0x00003678, 0x1c2ce9ad },
    { 0x0000367c, 0x978477ba },
    { 0x00004480, 0x00123ed1 },
    { 0x00004488, 0x006c5479 },
    { 0x00004490, 0x005f7c0a },
    { 0x00004498, 0x00f23c10 },
    { 0x000044a0, 0x00805978 },
    { 0x000044a8, 0x00c4aa62 },
    { 0x000044b0, 0x00ad5261 },
    { 0x000044b8, 0x0088fe46 },
    { 0x00004700, 0x00a57f6f },
    { 0x00004708, 0x002ccb8b },
    { 0x00004710, 0x00f71f8a },
    { 0x00004718, 0x000ed079 },
    { 0x00004720, 0x0027ccee },
    { 0x00004728, 0x001eb25a },
    { 0x00004730, 0x00b450f2 },
    { 0x00004738, 0x00882fcd },
    { 0x00004980, 0x003de08a },
    { 0x00004988, 0x0084229b },
    { 0x00004990, 0x00049732 },
    { 0x00004998, 0x00706475 },
    { 0x000049a0, 0x006813f0 },
    { 0x000049a8, 0x0049e54b },
    { 0x000049b0, 0x0074e604 },
    { 0x000049b8, 0x00a5aa9f },
    { 0x00004c00, 0x008bafd7 },
    { 0x00004c08, 0x009ee6e6 },
    { 0x00004c10, 0x008572e7 },
    { 0x00004c18, 0x00f41e48 },
    { 0x00004c20, 0x007fe25d },
    { 0x00004c28, 0x00bd82da },
    { 0x00004c30, 0x00400530 },
    { 0x00004c38, 0x003b3e84 },
    { 0x00004e80, 0x00f4eb82 },
    { 0x00004e88, 0x001e18a1 },
    { 0x00004e90, 0x0060b475 },
    { 0x00004e98, 0x00d6a8a3 },
    { 0x00004ea0, 0x0009d61c },
    { 0x00004ea8, 0x00a4df01 },
    { 0x00004eb0, 0x00ffa8ce },
    { 0x00004eb8, 0x001d4a44 },
    { 0x00005100, 0x00c12c3d },
    { 0x00005108, 0x00018dfc },
    { 0x00005110, 0x00eaacc0 },
    { 0x00005118, 0x00f58265 },
    { 0x00005120, 0x00b6a274 },
    { 0x00005128, 0x0017a843 },
    { 0x00005130, 0x0049262d },
    { 0x00005138, 0x00d9572e },
    { 0x00005380, 0x0022dc2b },
    { 0x00005388, 0x00a449d4 },
    { 0x00005390, 0x00aa71bf },
    { 0x00005398, 0x0014c795 },
    { 0x000053a0, 0x0083b92a },
    { 0x000053a8, 0x006f4f3e },
    { 0x000053b0, 0x00b6fc88 },
    { 0x000053b8, 0x006dde08 },
    { 0x00005600, 0x00ad2233 },
    { 0x00005608, 0x0025e0f3 },
    { 0x00005610, 0x00ab20ae },
    { 0x00005618, 0x00d1d1a6 },
    { 0x00005620, 0x0002ae42 },
    { 0x00005628, 0x005993bb },
    { 0x00005630, 0x008755b9 },
    { 0x00005638, 0x00a7dfd9 },
    { 0x00005880, 0x00b62193 },
    { 0x00005888, 0x00ae7d6d },
    { 0x00005890, 0x00fb4b3d },
    { 0x00005898, 0x00e3a653 },
    { 0x000058a0, 0x00e9effb },
    { 0x000058a8, 0x00c9cbbb },
    { 0x000058b0, 0x00c50766 },
    { 0x000058b8, 0x00e47997 },
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

    { /* Zero-init region 0xc0800000 size 0x66c0 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 28672, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0800000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 26304); munmap(zpage, 28672); }
    }
    { /* Zero-init region 0xc0000000 size 0x24000 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 147456, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 147456); munmap(zpage, 147456); }
    }

    /* Load embedded data (replaces devmem load scripts) */
    { /* Data: base=0xc0800000, 360 writes */
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
    { /* CRC[0] zero-init: addr=0xc0000000 size=0x24000 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 147456, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 147456); munmap(zpage, 147456); }
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
    { /* CRC[0]: addr=0xc0000000, size=0x24000, expect=0x36ca65e7 */
        uint8_t* page = (uint8_t*)mmap(NULL, 147456, PROT_READ, MAP_SHARED, fd, 0xc0000000);
        if (page != MAP_FAILED) {
            uint32_t crc = calc_crc32(page + 0, 147456);
            printf("CRC[0] = 0x%08x (expect 0x36ca65e7) %s\n", crc, crc == 0x36ca65e7 ? "PASS" : "FAIL");
            if (crc != 0x36ca65e7) failures++;
            munmap(page, 147456);
        } else { perror("mmap out"); }
    }

    printf("Results: %d failures\n", failures);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return failures;
}