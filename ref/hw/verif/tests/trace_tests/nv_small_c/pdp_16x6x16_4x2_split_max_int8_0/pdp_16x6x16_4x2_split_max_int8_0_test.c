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
    { 0xb038, 0x0000f307 }, /* NVDLA_PDP_D_RECIP_KERNEL_WIDTH_0 */
    { 0xb05c, 0x00072af9 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_7_CFG_0 */
    { 0xb020, 0x0000000f }, /* NVDLA_PDP_D_DATA_CUBE_OUT_CHANNEL_0 */
    { 0xb024, 0x00000211 }, /* NVDLA_PDP_D_OPERATION_MODE_CFG_0 */
    { 0xb018, 0x00000008 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_WIDTH_0 */
    { 0xb02c, 0x00b00401 }, /* NVDLA_PDP_D_PARTIAL_WIDTH_IN_0 */
    { 0xb028, 0x00000000 }, /* NVDLA_PDP_D_NAN_FLUSH_TO_ZERO_0 */
    { 0xb074, 0x00000000 }, /* NVDLA_PDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0xb098, 0x00000000 }, /* NVDLA_PDP_D_PERF_WRITE_STALL_0 */
    { 0xb070, 0xc0000080 }, /* NVDLA_PDP_D_DST_BASE_ADDR_LOW_0 */
    { 0xb068, 0x00000340 }, /* NVDLA_PDP_D_SRC_LINE_STRIDE_0 */
    { 0xb090, 0x00000000 }, /* NVDLA_PDP_D_NAN_OUTPUT_NUM_0 */
    { 0xb00c, 0x0000000f }, /* NVDLA_PDP_D_DATA_CUBE_IN_WIDTH_0 */
    { 0xb088, 0x00000000 }, /* NVDLA_PDP_D_INF_INPUT_NUM_0 */
    { 0xb054, 0x0006f586 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_5_CFG_0 */
    { 0xb080, 0x00000001 }, /* NVDLA_PDP_D_DST_RAM_CFG_0 */
    { 0xb064, 0x00000000 }, /* NVDLA_PDP_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xb078, 0x00002100 }, /* NVDLA_PDP_D_DST_LINE_STRIDE_0 */
    { 0xb060, 0x00000000 }, /* NVDLA_PDP_D_SRC_BASE_ADDR_LOW_0 */
    { 0xb034, 0x00110103 }, /* NVDLA_PDP_D_POOLING_KERNEL_CFG_0 */
    { 0xb03c, 0x00004971 }, /* NVDLA_PDP_D_RECIP_KERNEL_HEIGHT_0 */
    { 0xb094, 0x00000000 }, /* NVDLA_PDP_D_PERF_ENABLE_0 */
    { 0xb084, 0x00000000 }, /* NVDLA_PDP_D_DATA_FORMAT_0 */
    { 0xb048, 0x0001db74 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_2_CFG_0 */
    { 0xb014, 0x0000000f }, /* NVDLA_PDP_D_DATA_CUBE_IN_CHANNEL_0 */
    { 0xb09c, 0x68b2216b }, /* NVDLA_PDP_D_CYA_0 */
    { 0xb058, 0x000714ff }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_6_CFG_0 */
    { 0xb07c, 0x00008a60 }, /* NVDLA_PDP_D_DST_SURFACE_STRIDE_0 */
    { 0xb030, 0x00500400 }, /* NVDLA_PDP_D_PARTIAL_WIDTH_OUT_0 */
    { 0xb010, 0x00000005 }, /* NVDLA_PDP_D_DATA_CUBE_IN_HEIGHT_0 */
    { 0xb044, 0x00052098 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_1_CFG_0 */
    { 0xb040, 0x00001212 }, /* NVDLA_PDP_D_POOLING_PADDING_CFG_0 */
    { 0xb000, 0x00000000 }, /* NVDLA_PDP_S_STATUS_0 */
    { 0xb01c, 0x00000003 }, /* NVDLA_PDP_D_DATA_CUBE_OUT_HEIGHT_0 */
    { 0xb06c, 0x00002060 }, /* NVDLA_PDP_D_SRC_SURFACE_STRIDE_0 */
    { 0xb08c, 0x00000000 }, /* NVDLA_PDP_D_NAN_INPUT_NUM_0 */
    { 0xb050, 0x000602b3 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_4_CFG_0 */
    { 0xb04c, 0x00044c29 }, /* NVDLA_PDP_D_POOLING_PADDING_VALUE_3_CFG_0 */
    { 0xa004, 0x00000000 }, /* NVDLA_PDP_RDMA_S_POINTER_0 */
    { 0xa018, 0x00000001 }, /* NVDLA_PDP_RDMA_D_FLYING_MODE_0 */
    { 0xa02c, 0x00000001 }, /* NVDLA_PDP_RDMA_D_SRC_RAM_CFG_0 */
    { 0xa01c, 0xc0808e00 }, /* NVDLA_PDP_RDMA_D_SRC_BASE_ADDR_LOW_0 */
    { 0xa03c, 0x00000006 }, /* NVDLA_PDP_RDMA_D_POOLING_PADDING_CFG_0 */
    { 0xa030, 0x00000000 }, /* NVDLA_PDP_RDMA_D_DATA_FORMAT_0 */
    { 0xa000, 0x00000000 }, /* NVDLA_PDP_RDMA_S_STATUS_0 */
    { 0xa044, 0x00000000 }, /* NVDLA_PDP_RDMA_D_PERF_ENABLE_0 */
    { 0xa010, 0x00000005 }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_HEIGHT_0 */
    { 0xa04c, 0x81e7f8f3 }, /* NVDLA_PDP_RDMA_D_CYA_0 */
    { 0xa038, 0x00000013 }, /* NVDLA_PDP_RDMA_D_POOLING_KERNEL_CFG_0 */
    { 0xa020, 0x00000000 }, /* NVDLA_PDP_RDMA_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xa024, 0x00000340 }, /* NVDLA_PDP_RDMA_D_SRC_LINE_STRIDE_0 */
    { 0xa048, 0x00000000 }, /* NVDLA_PDP_RDMA_D_PERF_READ_STALL_0 */
    { 0xa014, 0x0000000f }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_CHANNEL_0 */
    { 0xa034, 0x00000002 }, /* NVDLA_PDP_RDMA_D_OPERATION_MODE_CFG_0 */
    { 0xa028, 0x00002060 }, /* NVDLA_PDP_RDMA_D_SRC_SURFACE_STRIDE_0 */
    { 0xa00c, 0x0000000f }, /* NVDLA_PDP_RDMA_D_DATA_CUBE_IN_WIDTH_0 */
    { 0xa040, 0x00b00401 }, /* NVDLA_PDP_RDMA_D_PARTIAL_WIDTH_IN_0 */
    { 0xb008, 0x00000001 }, /* NVDLA_PDP_D_OP_ENABLE_0 */
    { 0xa008, 0x00000001 }, /* NVDLA_PDP_RDMA_D_OP_ENABLE_0 */
    {-1, 0}
};

static const struct dla_reg program_enable[] = {
    {-1, 0}
};

/* Embedded data blocks (loaded via mmap instead of devmem scripts) */
static const uint32_t embed_0_data[][2] = {
    { 0x00000000, 0x6d3d0c34 },
    { 0x00000004, 0xbeaee91a },
    { 0x00000008, 0x9df7de25 },
    { 0x0000000c, 0x365f2a6f },
    { 0x00000010, 0xcef046e4 },
    { 0x00000014, 0x89301758 },
    { 0x00000018, 0xb952a739 },
    { 0x0000001c, 0x943eaa54 },
    { 0x00000020, 0xde6aab96 },
    { 0x00000024, 0xa5b4bac3 },
    { 0x00000028, 0x497d3051 },
    { 0x0000002c, 0x90d3367b },
    { 0x00000030, 0xed9b8673 },
    { 0x00000034, 0x96f9e29c },
    { 0x00000038, 0x1eb9c2d8 },
    { 0x0000003c, 0x8b60b908 },
    { 0x00000040, 0xc204c37d },
    { 0x00000044, 0xfeea0474 },
    { 0x00000048, 0x05df1760 },
    { 0x0000004c, 0xe555b595 },
    { 0x00000050, 0x845f6519 },
    { 0x00000054, 0x894e5fbc },
    { 0x00000058, 0xd349762c },
    { 0x0000005c, 0xef94ace8 },
    { 0x00000060, 0xd508c337 },
    { 0x00000064, 0xdcae8a06 },
    { 0x00000068, 0x172b5f88 },
    { 0x0000006c, 0xb0fab27c },
    { 0x00000070, 0xd19da139 },
    { 0x00000074, 0x1eb6e660 },
    { 0x00000078, 0x1cd1e7ca },
    { 0x0000007c, 0xabbd556e },
    { 0x00000340, 0xf4aa1681 },
    { 0x00000344, 0x91c809ca },
    { 0x00000348, 0x941ccda4 },
    { 0x0000034c, 0xb9fe390c },
    { 0x00000350, 0x1d140f1a },
    { 0x00000354, 0x3cc5651a },
    { 0x00000358, 0xf58a0533 },
    { 0x0000035c, 0xdaafb204 },
    { 0x00000360, 0x824ab9c0 },
    { 0x00000364, 0xaf88a044 },
    { 0x00000368, 0x2fa0ffec },
    { 0x0000036c, 0xc7cf4d55 },
    { 0x00000370, 0x9bdf8479 },
    { 0x00000374, 0x0d562780 },
    { 0x00000378, 0xb0574ac4 },
    { 0x0000037c, 0x6f7cb782 },
    { 0x00000380, 0x4a2d1a19 },
    { 0x00000384, 0x317254b2 },
    { 0x00000388, 0x2e9296b5 },
    { 0x0000038c, 0x11761762 },
    { 0x00000390, 0xd562e886 },
    { 0x00000394, 0x9f6d175a },
    { 0x00000398, 0xff6cad38 },
    { 0x0000039c, 0x0c132c29 },
    { 0x000003a0, 0x8a5b21bc },
    { 0x000003a4, 0xe62e3627 },
    { 0x000003a8, 0x520c7d53 },
    { 0x000003ac, 0x57ae7241 },
    { 0x000003b0, 0x5dfb296f },
    { 0x000003b4, 0xdf012274 },
    { 0x000003b8, 0xd3b49c54 },
    { 0x000003bc, 0x2862daf5 },
    { 0x00000680, 0x4a031c33 },
    { 0x00000684, 0x50877969 },
    { 0x00000688, 0xcf107310 },
    { 0x0000068c, 0x775c5019 },
    { 0x00000690, 0x2373e11d },
    { 0x00000694, 0xe904395f },
    { 0x00000698, 0x761ec355 },
    { 0x0000069c, 0xe418c224 },
    { 0x000006a0, 0x52f37f28 },
    { 0x000006a4, 0x6579f311 },
    { 0x000006a8, 0x9d5466c1 },
    { 0x000006ac, 0x1fc052a9 },
    { 0x000006b0, 0xa4be7e93 },
    { 0x000006b4, 0xf071a932 },
    { 0x000006b8, 0x08260bd4 },
    { 0x000006bc, 0x1cd7dfa1 },
    { 0x000006c0, 0x16c53009 },
    { 0x000006c4, 0xde73a8db },
    { 0x000006c8, 0xba82ad59 },
    { 0x000006cc, 0x61bf52e5 },
    { 0x000006d0, 0x84d3f465 },
    { 0x000006d4, 0x42afb488 },
    { 0x000006d8, 0xd9e61e21 },
    { 0x000006dc, 0x0f78b173 },
    { 0x000006e0, 0x2b3a8241 },
    { 0x000006e4, 0x827a6eda },
    { 0x000006e8, 0x7422b5db },
    { 0x000006ec, 0xd13d75bb },
    { 0x000006f0, 0xb67c7314 },
    { 0x000006f4, 0x777d9fcb },
    { 0x000006f8, 0x3f1caa56 },
    { 0x000006fc, 0x6a67d4bb },
    { 0x000009c0, 0x3c40f9be },
    { 0x000009c4, 0xa8392f19 },
    { 0x000009c8, 0x77c1e1b3 },
    { 0x000009cc, 0x039fad79 },
    { 0x000009d0, 0xabefd9d5 },
    { 0x000009d4, 0x52e9bb64 },
    { 0x000009d8, 0x4b55b6ae },
    { 0x000009dc, 0x5055c410 },
    { 0x000009e0, 0x26ddf217 },
    { 0x000009e4, 0x13e0a890 },
    { 0x000009e8, 0x73a95e33 },
    { 0x000009ec, 0xc008cccd },
    { 0x000009f0, 0x421eba78 },
    { 0x000009f4, 0xc7803179 },
    { 0x000009f8, 0xa942dc6c },
    { 0x000009fc, 0x3322c235 },
    { 0x00000a00, 0xd26501a1 },
    { 0x00000a04, 0xa019bbe4 },
    { 0x00000a08, 0xfa756c63 },
    { 0x00000a0c, 0x544ea22f },
    { 0x00000a10, 0x21664e1e },
    { 0x00000a14, 0xac866849 },
    { 0x00000a18, 0xfbb066ec },
    { 0x00000a1c, 0x9762ff5e },
    { 0x00000a20, 0x7a82c1dd },
    { 0x00000a24, 0x2983f27f },
    { 0x00000a28, 0xaa45843b },
    { 0x00000a2c, 0x41366db8 },
    { 0x00000a30, 0xbd8839fe },
    { 0x00000a34, 0xf329e65c },
    { 0x00000a38, 0x2a98e1fe },
    { 0x00000a3c, 0x8677cba6 },
    { 0x00000d00, 0x95dd5a4c },
    { 0x00000d04, 0x958f8912 },
    { 0x00000d08, 0x6d774720 },
    { 0x00000d0c, 0x6cf75df2 },
    { 0x00000d10, 0xfebb8b60 },
    { 0x00000d14, 0x2aeb5735 },
    { 0x00000d18, 0x6c487ece },
    { 0x00000d1c, 0xcb37325a },
    { 0x00000d20, 0xe0c2a17e },
    { 0x00000d24, 0x262c1669 },
    { 0x00000d28, 0xcb753675 },
    { 0x00000d2c, 0x9bde55e2 },
    { 0x00000d30, 0x2cbf3738 },
    { 0x00000d34, 0xcb7ab60f },
    { 0x00000d38, 0xe2742b1c },
    { 0x00000d3c, 0xf68a3776 },
    { 0x00000d40, 0x373c5ab1 },
    { 0x00000d44, 0x76161f27 },
    { 0x00000d48, 0x9f642271 },
    { 0x00000d4c, 0x1670aa5c },
    { 0x00000d50, 0x9e718e8f },
    { 0x00000d54, 0xb18843d4 },
    { 0x00000d58, 0x77ac8197 },
    { 0x00000d5c, 0x1a372d32 },
    { 0x00000d60, 0xa1b09b1a },
    { 0x00000d64, 0x8dba1c52 },
    { 0x00000d68, 0x8bb807c4 },
    { 0x00000d6c, 0x7039c617 },
    { 0x00000d70, 0x528f3e30 },
    { 0x00000d74, 0xc0160493 },
    { 0x00000d78, 0x159a3fe2 },
    { 0x00000d7c, 0xd920aac2 },
    { 0x00001040, 0x1aaa693c },
    { 0x00001044, 0x492a8fb9 },
    { 0x00001048, 0x4af5a2ec },
    { 0x0000104c, 0xd43c1632 },
    { 0x00001050, 0x31d154ba },
    { 0x00001054, 0xaf68dba3 },
    { 0x00001058, 0x5396b5d2 },
    { 0x0000105c, 0x1ec7a440 },
    { 0x00001060, 0x8f4558d5 },
    { 0x00001064, 0x2621decc },
    { 0x00001068, 0xc0b2b69a },
    { 0x0000106c, 0x3d198dad },
    { 0x00001070, 0x6b169e09 },
    { 0x00001074, 0x7de0f6d8 },
    { 0x00001078, 0x8e6ec724 },
    { 0x0000107c, 0xe605f260 },
    { 0x00001080, 0x4f39acbd },
    { 0x00001084, 0x70494b3d },
    { 0x00001088, 0x2bbe053c },
    { 0x0000108c, 0x4e93f1c1 },
    { 0x00001090, 0xfaca3bae },
    { 0x00001094, 0x1c631e76 },
    { 0x00001098, 0xa680a43e },
    { 0x0000109c, 0xa9ff378f },
    { 0x000010a0, 0x97a7cee9 },
    { 0x000010a4, 0x9779bd0b },
    { 0x000010a8, 0x80018e56 },
    { 0x000010ac, 0xb9a6d72d },
    { 0x000010b0, 0x77b6978c },
    { 0x000010b4, 0x9da685b5 },
    { 0x000010b8, 0x3188e825 },
    { 0x000010bc, 0xc1b4b73e },
    { 0x00001de0, 0x616d3d61 },
    { 0x00001de4, 0x657e7e66 },
    { 0x00001de8, 0x616c6361 },
    { 0x00001dec, 0x657e6c2c },
    { 0x00001df0, 0x41636377 },
    { 0x00001df4, 0x5a725543 },
    { 0x00001e00, 0x70744777 },
    { 0x00001e04, 0x5a386b6f },
    { 0x00001e08, 0x707e5061 },
    { 0x00001e0c, 0x78566b6f },
    { 0x00001e10, 0x697e703b },
    { 0x00001e14, 0x78737e43 },
    { 0x00001e18, 0x747b7067 },
    { 0x00001e1c, 0x7a737e12 },
    { 0x00001e20, 0x747b5567 },
    { 0x00001e24, 0x7a53c9b0 },
    { 0x00002060, 0xc6da232f },
    { 0x00002064, 0xa922af8d },
    { 0x00002068, 0xf3fe55e7 },
    { 0x0000206c, 0x0e3282aa },
    { 0x00002070, 0x087dd201 },
    { 0x00002074, 0x25279231 },
    { 0x00002078, 0x58f244ee },
    { 0x0000207c, 0x7c454ec0 },
    { 0x00002080, 0x9de6fcd7 },
    { 0x00002084, 0x48f80dd9 },
    { 0x00002088, 0xe293ca61 },
    { 0x0000208c, 0xfc70e7f2 },
    { 0x00002090, 0xd12d6e36 },
    { 0x00002094, 0x52f5e0a2 },
    { 0x00002098, 0x07a181bb },
    { 0x0000209c, 0xa1edddd2 },
    { 0x000020a0, 0xcd7166a0 },
    { 0x000020a4, 0xb63eae8c },
    { 0x000020a8, 0x90f752db },
    { 0x000020ac, 0x381212ea },
    { 0x000020b0, 0xc5807f2d },
    { 0x000020b4, 0x74ac7257 },
    { 0x000020b8, 0x2fff0452 },
    { 0x000020bc, 0x273b99e5 },
    { 0x000020c0, 0xe95ffdb8 },
    { 0x000020c4, 0x49401c80 },
    { 0x000020c8, 0x756dd48c },
    { 0x000020cc, 0xe57f87e6 },
    { 0x000020d0, 0xf8211ac9 },
    { 0x000020d4, 0x2db0b6ca },
    { 0x000020d8, 0xe10ab92c },
    { 0x000020dc, 0x11667638 },
    { 0x000023a0, 0x34c4caed },
    { 0x000023a4, 0x426c360b },
    { 0x000023a8, 0x296da720 },
    { 0x000023ac, 0x35c19e62 },
    { 0x000023b0, 0xe981aad1 },
    { 0x000023b4, 0xaab4d1ef },
    { 0x000023b8, 0xf0e51361 },
    { 0x000023bc, 0x6525cd2c },
    { 0x000023c0, 0x0e4ded2d },
    { 0x000023c4, 0x2f523419 },
    { 0x000023c8, 0x2f61821d },
    { 0x000023cc, 0x1863d0f0 },
    { 0x000023d0, 0xa563474b },
    { 0x000023d4, 0x05fc9340 },
    { 0x000023d8, 0x1038c448 },
    { 0x000023dc, 0x88c29843 },
    { 0x000023e0, 0x3bb48a26 },
    { 0x000023e4, 0x0a38da25 },
    { 0x000023e8, 0x19a58a61 },
    { 0x000023ec, 0xf2216ba5 },
    { 0x000023f0, 0xad512d88 },
    { 0x000023f4, 0xb856d62b },
    { 0x000023f8, 0xe0cfe51a },
    { 0x000023fc, 0x9e0e1714 },
    { 0x00002400, 0x08a0708f },
    { 0x00002404, 0x0c73ec8d },
    { 0x00002408, 0x203b26a1 },
    { 0x0000240c, 0x1b095912 },
    { 0x00002410, 0xf27ba6ee },
    { 0x00002414, 0xf4b6c58e },
    { 0x00002418, 0x745c5567 },
    { 0x0000241c, 0x2d0cb787 },
    { 0x000026e0, 0xa0d62eed },
    { 0x000026e4, 0xab8b7e66 },
    { 0x000026e8, 0x8b2fdae3 },
    { 0x000026ec, 0x9e2b6083 },
    { 0x000026f0, 0x616c0b16 },
    { 0x000026f4, 0x8e496ce2 },
    { 0x000026f8, 0x97ea3daa },
    { 0x000026fc, 0x4e7e018b },
    { 0x00002700, 0x98d963de },
    { 0x00002704, 0x457293c1 },
    { 0x00002708, 0x2a53c1d9 },
    { 0x0000270c, 0x99c1040b },
    { 0x00002710, 0xf389cd77 },
    { 0x00002714, 0x5a213595 },
    { 0x00002718, 0x41d70804 },
    { 0x0000271c, 0x56ae5599 },
    { 0x00002720, 0x707404f6 },
    { 0x00002724, 0x39c8a0e5 },
    { 0x00002728, 0x5a6886bf },
    { 0x0000272c, 0xfbd0d66f },
    { 0x00002730, 0xf13fafde },
    { 0x00002734, 0xbb98ed43 },
    { 0x00002738, 0x907e503b },
    { 0x0000273c, 0x78f2c42e },
    { 0x00002740, 0x6977c0a5 },
    { 0x00002744, 0x019b7e02 },
    { 0x00002748, 0xa38c2337 },
    { 0x0000274c, 0xb7122eb9 },
    { 0x00002750, 0xb351a82a },
    { 0x00002754, 0x7a90c9b0 },
    { 0x00002758, 0x12685314 },
    { 0x0000275c, 0x5f539ea6 },
    { 0x00002a20, 0xc47303d5 },
    { 0x00002a24, 0x3d22c5f3 },
    { 0x00002a28, 0xf511952a },
    { 0x00002a2c, 0x55481ede },
    { 0x00002a30, 0x9555f2d5 },
    { 0x00002a34, 0x4645a498 },
    { 0x00002a38, 0xe383720b },
    { 0x00002a3c, 0xdad4cae2 },
    { 0x00002a40, 0x1668eb8c },
    { 0x00002a44, 0x953ccc19 },
    { 0x00002a48, 0x44d61ffa },
    { 0x00002a4c, 0x5105b497 },
    { 0x00002a50, 0x3d8eab6f },
    { 0x00002a54, 0xe77ce194 },
    { 0x00002a58, 0x1626d215 },
    { 0x00002a5c, 0x063a66ad },
    { 0x00002a60, 0x26406826 },
    { 0x00002a64, 0x6dbcdcdc },
    { 0x00002a68, 0xbd295e75 },
    { 0x00002a6c, 0x8986c46e },
    { 0x00002a70, 0x62788737 },
    { 0x00002a74, 0xa6a42a52 },
    { 0x00002a78, 0x907079bd },
    { 0x00002a7c, 0xb0bb8a05 },
    { 0x00002a80, 0x8f5644be },
    { 0x00002a84, 0x41766ab8 },
    { 0x00002a88, 0xc0b8f871 },
    { 0x00002a8c, 0xc84e9c9c },
    { 0x00002a90, 0x5962c6d6 },
    { 0x00002a94, 0x90c2dbba },
    { 0x00002a98, 0x04b49fb5 },
    { 0x00002a9c, 0xc550d46d },
    { 0x00002d60, 0x3ffeec09 },
    { 0x00002d64, 0x0f582a77 },
    { 0x00002d68, 0x69ca2484 },
    { 0x00002d6c, 0x7fe604aa },
    { 0x00002d70, 0x1d04392b },
    { 0x00002d74, 0x725e2ccc },
    { 0x00002d78, 0x37943c1a },
    { 0x00002d7c, 0x2b6a554a },
    { 0x00002d80, 0x5491e78b },
    { 0x00002d84, 0x77916da4 },
    { 0x00002d88, 0xf56be850 },
    { 0x00002d8c, 0x30845fa2 },
    { 0x00002d90, 0xd3d440e5 },
    { 0x00002d94, 0xfe213ee9 },
    { 0x00002d98, 0x1ff1eebb },
    { 0x00002d9c, 0x2584f72c },
    { 0x00002da0, 0x369feb91 },
    { 0x00002da4, 0xfa49d27f },
    { 0x00002da8, 0x9f2d3c0a },
    { 0x00002dac, 0xc853b6a8 },
    { 0x00002db0, 0x09f546c5 },
    { 0x00002db4, 0xa0876733 },
    { 0x00002db8, 0xb0f0ea81 },
    { 0x00002dbc, 0x278a5ea2 },
    { 0x00002dc0, 0x6e479f3e },
    { 0x00002dc4, 0xbebab1eb },
    { 0x00002dc8, 0x2a143b82 },
    { 0x00002dcc, 0xb8980883 },
    { 0x00002dd0, 0xda67fa0a },
    { 0x00002dd4, 0x530934b6 },
    { 0x00002dd8, 0x9dae59fc },
    { 0x00002ddc, 0x3f32dc79 },
    { 0x000030a0, 0xd5af7017 },
    { 0x000030a4, 0xabe4b999 },
    { 0x000030a8, 0x6d201b85 },
    { 0x000030ac, 0x96393d50 },
    { 0x000030b0, 0xadeb0be5 },
    { 0x000030b4, 0x3b16ba4c },
    { 0x000030b8, 0x45b7df21 },
    { 0x000030bc, 0xa630fb0a },
    { 0x000030c0, 0x5f16df01 },
    { 0x000030c4, 0x5fdb5bf1 },
    { 0x000030c8, 0x301c2b44 },
    { 0x000030cc, 0xcedc4e2f },
    { 0x000030d0, 0x1e14c186 },
    { 0x000030d4, 0x7b7b197e },
    { 0x000030d8, 0x8555d73d },
    { 0x000030dc, 0xe120f819 },
    { 0x000030e0, 0xc07a21b4 },
    { 0x000030e4, 0x8b40c998 },
    { 0x000030e8, 0x07015c64 },
    { 0x000030ec, 0x290ad5f4 },
    { 0x000030f0, 0x2fec5156 },
    { 0x000030f4, 0xcd77693b },
    { 0x000030f8, 0xf7599cff },
    { 0x000030fc, 0xd49990a1 },
    { 0x00003100, 0xf3626fdc },
    { 0x00003104, 0xbae5580d },
    { 0x00003108, 0x2e54f97d },
    { 0x0000310c, 0x88762e99 },
    { 0x00003110, 0x64a44e8c },
    { 0x00003114, 0xb298504a },
    { 0x00003118, 0x932d226c },
    { 0x0000311c, 0x8bf49dbc },
    { 0x00003ee0, 0x6973722b },
    { 0x00003ee4, 0x7f6a5577 },
    { 0x00003ee8, 0x546b7250 },
    { 0x00003eec, 0x776a6d4a },
    { 0x00003ef0, 0x546b406f },
    { 0x00003ef4, 0x777c6d2c },
    { 0x00003f00, 0x3d406875 },
    { 0x00003f04, 0x6d7c667f },
    { 0x00003f08, 0x62787975 },
    { 0x00003f0c, 0x6d53677f },
    { 0x00003f10, 0x6e787971 },
    { 0x00003f14, 0x41766a52 },
    { 0x00003f18, 0x6e675971 },
    { 0x00003f1c, 0x53766a79 },
    { 0x00003f20, 0x5967590a },
    { 0x00003f24, 0x53503479 },
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

    { /* Zero-init region 0xc0808e00 size 0x40c0 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 20480, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0808000);
        if (zpage != MAP_FAILED) { memset(zpage + 3584, 0, 16576); munmap(zpage, 20480); }
    }
    { /* Zero-init region 0xc0000080 size 0x114c0 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 73728, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 128, 0, 70848); munmap(zpage, 73728); }
    }

    /* Load embedded data (replaces devmem load scripts) */
    { /* Data: base=0xc0808e00, 416 writes */
        uint8_t* dpage = (uint8_t*)mmap(NULL, 20480, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0808000);
        if (dpage != MAP_FAILED) {
            const uint32_t (*dp)[2] = embed_0_data;
            uint32_t base_off = 0xe00;
            while ((*dp)[0] != 0xFFFFFFFF) {
                *(volatile uint32_t*)(dpage + base_off + (*dp)[0]) = (*dp)[1];
                dp++;
            }
            munmap(dpage, 20480);
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
    { /* CRC[0] zero-init: addr=0xc0000080 size=0x114c0 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 73728, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 128, 0, 70848); munmap(zpage, 73728); }
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
    { /* CRC[0]: addr=0xc0000080, size=0x114c0, expect=0xf0ca2c1b */
        uint8_t* page = (uint8_t*)mmap(NULL, 73728, PROT_READ, MAP_SHARED, fd, 0xc0000000);
        if (page != MAP_FAILED) {
            uint32_t crc = calc_crc32(page + 128, 70848);
            printf("CRC[0] = 0x%08x (expect 0xf0ca2c1b) %s\n", crc, crc == 0xf0ca2c1b ? "PASS" : "FAIL");
            if (crc != 0xf0ca2c1b) failures++;
            munmap(page, 73728);
        } else { perror("mmap out"); }
    }

    printf("Results: %d failures\n", failures);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return failures;
}