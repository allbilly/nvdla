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
    { 0xd004, 0x00000000 }, /* NVDLA_CDP_S_POINTER_0 */
    { 0xd078, 0x00000001 }, /* NVDLA_CDP_D_DATIN_SCALE_0 */
    { 0xd0b0, 0x00000000 }, /* NVDLA_CDP_D_PERF_LUT_LE_HIT_0 */
    { 0xd06c, 0x00000001 }, /* NVDLA_CDP_D_NAN_FLUSH_TO_ZERO_0 */
    { 0xd054, 0x00000000 }, /* NVDLA_CDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0xd098, 0x00000000 }, /* NVDLA_CDP_D_OUT_SATURATION_0 */
    { 0xd080, 0x00000000 }, /* NVDLA_CDP_D_DATOUT_OFFSET_0 */
    { 0xd050, 0xc0800000 }, /* NVDLA_CDP_D_DST_BASE_ADDR_LOW_0 */
    { 0xd0b4, 0x00000000 }, /* NVDLA_CDP_D_PERF_LUT_LO_HIT_0 */
    { 0xd068, 0x00000000 }, /* NVDLA_CDP_D_DATA_FORMAT_0 */
    { 0xd084, 0x00000001 }, /* NVDLA_CDP_D_DATOUT_SCALE_0 */
    { 0xd0a8, 0x00000000 }, /* NVDLA_CDP_D_PERF_LUT_OFLOW_0 */
    { 0xd088, 0x00000009 }, /* NVDLA_CDP_D_DATOUT_SHIFTER_0 */
    { 0xd0b8, 0x6543645d }, /* NVDLA_CDP_D_CYA_0 */
    { 0xd09c, 0x00000001 }, /* NVDLA_CDP_D_PERF_ENABLE_0 */
    { 0xd07c, 0x00000000 }, /* NVDLA_CDP_D_DATIN_SHIFTER_0 */
    { 0xd000, 0x00000000 }, /* NVDLA_CDP_S_STATUS_0 */
    { 0xd08c, 0x00000000 }, /* NVDLA_CDP_D_NAN_INPUT_NUM_0 */
    { 0xd058, 0x00000100 }, /* NVDLA_CDP_D_DST_LINE_STRIDE_0 */
    { 0xd05c, 0x00000800 }, /* NVDLA_CDP_D_DST_SURFACE_STRIDE_0 */
    { 0xd070, 0x00000003 }, /* NVDLA_CDP_D_LRN_CFG_0 */
    { 0xd094, 0x00000000 }, /* NVDLA_CDP_D_NAN_OUTPUT_NUM_0 */
    { 0xd090, 0x00000000 }, /* NVDLA_CDP_D_INF_INPUT_NUM_0 */
    { 0xd060, 0x00000001 }, /* NVDLA_CDP_D_DST_DMA_CFG_0 */
    { 0xd0a0, 0x00000000 }, /* NVDLA_CDP_D_PERF_WRITE_STALL_0 */
    { 0xd0ac, 0x00000000 }, /* NVDLA_CDP_D_PERF_LUT_HYBRID_0 */
    { 0xd0a4, 0x00000000 }, /* NVDLA_CDP_D_PERF_LUT_UFLOW_0 */
    { 0xd04c, 0x00000000 }, /* NVDLA_CDP_D_FUNC_BYPASS_0 */
    { 0xd074, 0x00000000 }, /* NVDLA_CDP_D_DATIN_OFFSET_0 */
    { 0xd008, 0x00020000 }, /* NVDLA_CDP_S_LUT_ACCESS_CFG_0 */
    { 0xd00c, 0x00000000 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000001 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000002 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000003 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000004 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000005 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000006 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000007 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000008 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000009 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000000a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000000b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000000c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000000d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000000e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000000f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000010 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000011 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000012 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000013 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000014 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000015 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000016 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000017 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000018 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000019 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000001a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000001b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000001c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000001d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000001e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000001f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000020 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000021 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000022 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000023 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000024 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000025 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000026 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000027 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000028 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000029 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000002a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000002b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000002c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000002d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000002e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000002f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000030 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000031 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000032 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000033 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000034 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000035 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000036 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000037 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000038 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000039 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000003a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000003b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000003c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000003d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000003e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000003f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000040 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd008, 0x00030000 }, /* NVDLA_CDP_S_LUT_ACCESS_CFG_0 */
    { 0xd00c, 0x00000000 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000001 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000002 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000003 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000004 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000005 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000006 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000007 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000008 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000009 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000000a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000000b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000000c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000000d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000000e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000000f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000010 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000011 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000012 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000013 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000014 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000015 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000016 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000017 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000018 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000019 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000001a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000001b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000001c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000001d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000001e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000001f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000020 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000021 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000022 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000023 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000024 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000025 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000026 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000027 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000028 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000029 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000002a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000002b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000002c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000002d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000002e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000002f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000030 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000031 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000032 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000033 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000034 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000035 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000036 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000037 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000038 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000039 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000003a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000003b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000003c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000003d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000003e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000003f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000040 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000041 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000042 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000043 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000044 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000045 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000046 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000047 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000048 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000049 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000004a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000004b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000004c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000004d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000004e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000004f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000050 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000051 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000052 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000053 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000054 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000055 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000056 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000057 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000058 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000059 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000005a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000005b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000005c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000005d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000005e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000005f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000060 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000061 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000062 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000063 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000064 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000065 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000066 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000067 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000068 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000069 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000006a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000006b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000006c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000006d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000006e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000006f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000070 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000071 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000072 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000073 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000074 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000075 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000076 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000077 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000078 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000079 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000007a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000007b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000007c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000007d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000007e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000007f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000080 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000081 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000082 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000083 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000084 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000085 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000086 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000087 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000088 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000089 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000008a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000008b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000008c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000008d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000008e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000008f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000090 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000091 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000092 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000093 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000094 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000095 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000096 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000097 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000098 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000099 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000009a }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000009b }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000009c }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000009d }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000009e }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x0000009f }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000a0 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000a1 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000a2 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000a3 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000a4 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000a5 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000a6 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000a7 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000a8 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000a9 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000aa }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000ab }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000ac }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000ad }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000ae }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000af }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000b0 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000b1 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000b2 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000b3 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000b4 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000b5 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000b6 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000b7 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000b8 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000b9 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000ba }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000bb }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000bc }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000bd }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000be }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000bf }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000c0 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000c1 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000c2 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000c3 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000c4 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000c5 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000c6 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000c7 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000c8 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000c9 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000ca }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000cb }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000cc }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000cd }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000ce }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000cf }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000d0 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000d1 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000d2 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000d3 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000d4 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000d5 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000d6 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000d7 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000d8 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000d9 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000da }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000db }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000dc }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000dd }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000de }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000df }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000e0 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000e1 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000e2 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000e3 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000e4 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000e5 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000e6 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000e7 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000e8 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000e9 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000ea }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000eb }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000ec }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000ed }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000ee }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000ef }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000f0 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000f1 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000f2 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000f3 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000f4 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000f5 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000f6 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000f7 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000f8 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000f9 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000fa }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000fb }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000fc }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000fd }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000fe }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x000000ff }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd00c, 0x00000100 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd010, 0x00000071 }, /* NVDLA_CDP_S_LUT_CFG_0 */
    { 0xd010, 0x00000071 }, /* NVDLA_CDP_S_LUT_CFG_0 */
    { 0xd014, 0x000cfe00 }, /* NVDLA_CDP_S_LUT_INFO_0 */
    { 0xd018, 0xfffffff2 }, /* NVDLA_CDP_S_LUT_LE_START_LOW_0 */
    { 0xd01c, 0x0000003f }, /* NVDLA_CDP_S_LUT_LE_START_HIGH_0 */
    { 0xd020, 0x00000002 }, /* NVDLA_CDP_S_LUT_LE_END_LOW_0 */
    { 0xd024, 0x00000000 }, /* NVDLA_CDP_S_LUT_LE_END_HIGH_0 */
    { 0xd028, 0x00000000 }, /* NVDLA_CDP_S_LUT_LO_START_LOW_0 */
    { 0xd02c, 0x00000000 }, /* NVDLA_CDP_S_LUT_LO_START_HIGH_0 */
    { 0xd030, 0x00100000 }, /* NVDLA_CDP_S_LUT_LO_END_LOW_0 */
    { 0xd034, 0x00000000 }, /* NVDLA_CDP_S_LUT_LO_END_HIGH_0 */
    { 0xd038, 0x00010001 }, /* NVDLA_CDP_S_LUT_LE_SLOPE_SCALE_0 */
    { 0xd03c, 0x00000000 }, /* NVDLA_CDP_S_LUT_LE_SLOPE_SHIFT_0 */
    { 0xd040, 0x00010001 }, /* NVDLA_CDP_S_LUT_LO_SLOPE_SCALE_0 */
    { 0xd044, 0x00000000 }, /* NVDLA_CDP_S_LUT_LO_SLOPE_SHIFT_0 */
    { 0xc004, 0x00000000 }, /* NVDLA_CDP_RDMA_S_POINTER_0 */
    { 0xc03c, 0x00000000 }, /* NVDLA_CDP_RDMA_D_PERF_READ_STALL_0 */
    { 0xc040, 0x6463f6c9 }, /* NVDLA_CDP_RDMA_D_CYA_0 */
    { 0xc024, 0x00000800 }, /* NVDLA_CDP_RDMA_D_SRC_SURFACE_STRIDE_0 */
    { 0xc030, 0x00000000 }, /* NVDLA_CDP_RDMA_D_OPERATION_MODE_0 */
    { 0xc010, 0x00000007 }, /* NVDLA_CDP_RDMA_D_DATA_CUBE_HEIGHT_0 */
    { 0xc034, 0x00000000 }, /* NVDLA_CDP_RDMA_D_DATA_FORMAT_0 */
    { 0xc01c, 0x00000000 }, /* NVDLA_CDP_RDMA_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xc020, 0x00000100 }, /* NVDLA_CDP_RDMA_D_SRC_LINE_STRIDE_0 */
    { 0xc00c, 0x00000007 }, /* NVDLA_CDP_RDMA_D_DATA_CUBE_WIDTH_0 */
    { 0xc02c, 0x00000000 }, /* NVDLA_CDP_RDMA_D_SRC_COMPRESSION_EN_0 */
    { 0xc038, 0x00000000 }, /* NVDLA_CDP_RDMA_D_PERF_ENABLE_0 */
    { 0xc028, 0x00000001 }, /* NVDLA_CDP_RDMA_D_SRC_DMA_CFG_0 */
    { 0xc014, 0x0000001f }, /* NVDLA_CDP_RDMA_D_DATA_CUBE_CHANNEL_0 */
    { 0xc018, 0xc0000c00 }, /* NVDLA_CDP_RDMA_D_SRC_BASE_ADDR_LOW_0 */
    { 0xc000, 0x00000000 }, /* NVDLA_CDP_RDMA_S_STATUS_0 */
    { 0xd048, 0x00000001 }, /* NVDLA_CDP_D_OP_ENABLE_0 */
    { 0xc008, 0x00000001 }, /* NVDLA_CDP_RDMA_D_OP_ENABLE_0 */
    {-1, 0}
};

static const struct dla_reg program_enable[] = {
    {-1, 0}
};

/* Embedded data blocks (loaded via mmap instead of devmem scripts) */
static const uint32_t embed_0_data[][2] = {
    { 0x00000000, 0xe9170425 },
    { 0x00000004, 0xe4ae167a },
    { 0x00000008, 0xd9c91757 },
    { 0x0000000c, 0xb6e00a6a },
    { 0x00000010, 0x0cf46b2f },
    { 0x00000014, 0xa8f930b8 },
    { 0x00000018, 0xf52cff9c },
    { 0x0000001c, 0x8fdba754 },
    { 0x00000020, 0xb4e9e574 },
    { 0x00000024, 0xf3950e61 },
    { 0x00000028, 0x925fca71 },
    { 0x0000002c, 0xdbe1294e },
    { 0x00000030, 0xab01ddf1 },
    { 0x00000034, 0xeec637aa },
    { 0x00000038, 0x5effacc2 },
    { 0x0000003c, 0xbbd00fbb },
    { 0x00000100, 0x62166cfb },
    { 0x00000104, 0xe4582866 },
    { 0x00000108, 0xc6dab02e },
    { 0x0000010c, 0x04c559c0 },
    { 0x00000110, 0xfed52b52 },
    { 0x00000114, 0x26a8c0b4 },
    { 0x00000118, 0x9b569099 },
    { 0x0000011c, 0xc77b5111 },
    { 0x00000120, 0x8bd234c0 },
    { 0x00000124, 0x0b466850 },
    { 0x00000128, 0x9f08c234 },
    { 0x0000012c, 0xa4bb4748 },
    { 0x00000130, 0xe33f579d },
    { 0x00000134, 0x8ea6da2d },
    { 0x00000138, 0x7226a46f },
    { 0x0000013c, 0x6988d2b8 },
    { 0x00000200, 0x9fa9b8b6 },
    { 0x00000204, 0x5e298139 },
    { 0x00000208, 0x565312f1 },
    { 0x0000020c, 0xdfb68c61 },
    { 0x00000210, 0xc0b6f427 },
    { 0x00000214, 0x8cb75187 },
    { 0x00000218, 0x4c950a71 },
    { 0x0000021c, 0x7204538f },
    { 0x00000220, 0x0b5bce1c },
    { 0x00000224, 0x5f2c4a40 },
    { 0x00000228, 0xb6c162db },
    { 0x0000022c, 0x98cfc3d4 },
    { 0x00000230, 0x38e1255f },
    { 0x00000234, 0xaac04cf4 },
    { 0x00000238, 0xb9b8525d },
    { 0x0000023c, 0xd9dc21a7 },
    { 0x00000300, 0x2bcadb63 },
    { 0x00000304, 0x98f2aa38 },
    { 0x00000308, 0x81d6831b },
    { 0x0000030c, 0x7e216ddc },
    { 0x00000310, 0x8b026251 },
    { 0x00000314, 0x3fd9c901 },
    { 0x00000318, 0x6d6695c0 },
    { 0x0000031c, 0x38739d1e },
    { 0x00000320, 0xdc68756b },
    { 0x00000324, 0x29fd7765 },
    { 0x00000328, 0xdbbac55f },
    { 0x0000032c, 0xd1c1151c },
    { 0x00000330, 0xf274e085 },
    { 0x00000334, 0xe2e554d8 },
    { 0x00000338, 0x5524565e },
    { 0x0000033c, 0xa1900d76 },
    { 0x00000400, 0xbc12f797 },
    { 0x00000404, 0x5e638950 },
    { 0x00000408, 0x3c6fe85c },
    { 0x0000040c, 0x8b8f451f },
    { 0x00000410, 0x9a985ab6 },
    { 0x00000414, 0xfd2d9e59 },
    { 0x00000418, 0x037a10f3 },
    { 0x0000041c, 0x6ddfca3b },
    { 0x00000420, 0x8f141fd6 },
    { 0x00000424, 0xaa2eb9b5 },
    { 0x00000428, 0x16e920e4 },
    { 0x0000042c, 0x19690543 },
    { 0x00000430, 0xc2bff7ae },
    { 0x00000434, 0x93353df4 },
    { 0x00000438, 0x760b80e7 },
    { 0x0000043c, 0xaa02da8d },
    { 0x00000500, 0xb2768e65 },
    { 0x00000504, 0xba8170b4 },
    { 0x00000508, 0xd0662bc1 },
    { 0x0000050c, 0x77708caf },
    { 0x00000510, 0x1212172a },
    { 0x00000514, 0x16b2bc88 },
    { 0x00000518, 0xb4402571 },
    { 0x0000051c, 0xa62afaca },
    { 0x00000520, 0xae3f61a9 },
    { 0x00000524, 0xc5f0c8d1 },
    { 0x00000528, 0x7f8fc40c },
    { 0x0000052c, 0xeb14886f },
    { 0x00000530, 0x6b2e9597 },
    { 0x00000534, 0xb497218e },
    { 0x00000538, 0xb3a8595c },
    { 0x0000053c, 0x25e5c1f6 },
    { 0x00000600, 0xaa58ac35 },
    { 0x00000604, 0xee732194 },
    { 0x00000608, 0xcbf33380 },
    { 0x0000060c, 0x977dd736 },
    { 0x00000610, 0x0ab2bee1 },
    { 0x00000614, 0xe6f28e2c },
    { 0x00000618, 0x2d83201f },
    { 0x0000061c, 0xf83e2bd0 },
    { 0x00000620, 0xd9b123f3 },
    { 0x00000624, 0xe51da619 },
    { 0x00000628, 0xcd31c14a },
    { 0x0000062c, 0x2a0aba2a },
    { 0x00000630, 0xe2f6effa },
    { 0x00000634, 0x3d9b327e },
    { 0x00000638, 0x4daff37c },
    { 0x0000063c, 0x66bde906 },
    { 0x00000700, 0x22991df8 },
    { 0x00000704, 0x52fc3077 },
    { 0x00000708, 0x210929e5 },
    { 0x0000070c, 0x14871170 },
    { 0x00000710, 0x5c3da515 },
    { 0x00000714, 0x8af9f3b0 },
    { 0x00000718, 0xa1b80d37 },
    { 0x0000071c, 0xfe197819 },
    { 0x00000720, 0xc17cfd75 },
    { 0x00000724, 0x6f25eba7 },
    { 0x00000728, 0xf8462916 },
    { 0x0000072c, 0x051e07a8 },
    { 0x00000730, 0xddef86d5 },
    { 0x00000734, 0xe3b914e9 },
    { 0x00000738, 0x1ad51a48 },
    { 0x0000073c, 0xa41f4ab9 },
    { 0x00000800, 0xe12771fc },
    { 0x00000804, 0xf9153712 },
    { 0x00000808, 0xa7c06aaf },
    { 0x0000080c, 0xd2c86482 },
    { 0x00000810, 0x0d8f5ec3 },
    { 0x00000814, 0xf9c9deae },
    { 0x00000818, 0x2d31645a },
    { 0x0000081c, 0x133b0c65 },
    { 0x00000820, 0x5fad0894 },
    { 0x00000824, 0x569c3c62 },
    { 0x00000828, 0xea1646e2 },
    { 0x0000082c, 0x903c6014 },
    { 0x00000830, 0xc149f716 },
    { 0x00000834, 0x8105201a },
    { 0x00000838, 0xde8296d3 },
    { 0x0000083c, 0x2f7d55ae },
    { 0x00000900, 0xa5a9379f },
    { 0x00000904, 0xd0e9ce95 },
    { 0x00000908, 0x09bb8d09 },
    { 0x0000090c, 0x535908d7 },
    { 0x00000910, 0x8a598655 },
    { 0x00000914, 0x22abfb3d },
    { 0x00000918, 0xec3842ef },
    { 0x0000091c, 0x82cc29e2 },
    { 0x00000920, 0xc1c9a7c1 },
    { 0x00000924, 0x1c1cbb9c },
    { 0x00000928, 0x4178987d },
    { 0x0000092c, 0x4ead493b },
    { 0x00000930, 0x7fedd227 },
    { 0x00000934, 0xe1e6be58 },
    { 0x00000938, 0x3454bfb0 },
    { 0x0000093c, 0x09e57573 },
    { 0x00000a00, 0x1eabf8fe },
    { 0x00000a04, 0x8b6cbad5 },
    { 0x00000a08, 0x09e90ef2 },
    { 0x00000a0c, 0xcc87e531 },
    { 0x00000a10, 0x9d66f33f },
    { 0x00000a14, 0x9c260977 },
    { 0x00000a18, 0xd52f6609 },
    { 0x00000a1c, 0xa60a7d6c },
    { 0x00000a20, 0x84443d1b },
    { 0x00000a24, 0x77e6fab8 },
    { 0x00000a28, 0xb7ab5f19 },
    { 0x00000a2c, 0x11886cc5 },
    { 0x00000a30, 0x225f3486 },
    { 0x00000a34, 0xc4663e3f },
    { 0x00000a38, 0x219990db },
    { 0x00000a3c, 0x73733664 },
    { 0x00000b00, 0x3214ccf1 },
    { 0x00000b04, 0x32a98159 },
    { 0x00000b08, 0x2e6e2154 },
    { 0x00000b0c, 0xbb3a23ab },
    { 0x00000b10, 0xb6a68af2 },
    { 0x00000b14, 0x3275ce44 },
    { 0x00000b18, 0x2c496172 },
    { 0x00000b1c, 0x3a4ead82 },
    { 0x00000b20, 0x92cf4226 },
    { 0x00000b24, 0x0491b5e3 },
    { 0x00000b28, 0x5289b421 },
    { 0x00000b2c, 0x55fa6e22 },
    { 0x00000b30, 0x33809d71 },
    { 0x00000b34, 0xbeecc32d },
    { 0x00000b38, 0xc65fe6af },
    { 0x00000b3c, 0x50d2c588 },
    { 0x00000c00, 0xee70a408 },
    { 0x00000c04, 0x2ff732a9 },
    { 0x00000c08, 0x54c40862 },
    { 0x00000c0c, 0xa05337e7 },
    { 0x00000c10, 0xc1b0fa47 },
    { 0x00000c14, 0x072d306a },
    { 0x00000c18, 0x0484eaff },
    { 0x00000c1c, 0x95294131 },
    { 0x00000c20, 0x6c1967df },
    { 0x00000c24, 0x36213e2a },
    { 0x00000c28, 0x488a08d7 },
    { 0x00000c2c, 0xca8fe7d1 },
    { 0x00000c30, 0xda365eff },
    { 0x00000c34, 0x7921da02 },
    { 0x00000c38, 0xa63b8d3f },
    { 0x00000c3c, 0x1e4b3180 },
    { 0x00000d00, 0x0c679ea4 },
    { 0x00000d04, 0xd976bd93 },
    { 0x00000d08, 0xae6a3077 },
    { 0x00000d0c, 0x5fc166f1 },
    { 0x00000d10, 0xfa26154e },
    { 0x00000d14, 0x0ff51aa1 },
    { 0x00000d18, 0x7a5d56e5 },
    { 0x00000d1c, 0xadb28aab },
    { 0x00000d20, 0xca4fe26f },
    { 0x00000d24, 0xbd5bff9d },
    { 0x00000d28, 0x9e89796e },
    { 0x00000d2c, 0xba5de655 },
    { 0x00000d30, 0x8f26b12a },
    { 0x00000d34, 0xc7107d33 },
    { 0x00000d38, 0x62521f3f },
    { 0x00000d3c, 0xb78b2d20 },
    { 0x00000e00, 0xbeaa70ea },
    { 0x00000e04, 0x26a0190c },
    { 0x00000e08, 0xa9d6743c },
    { 0x00000e0c, 0x86ba050d },
    { 0x00000e10, 0xcf0a2f56 },
    { 0x00000e14, 0xc1dd2190 },
    { 0x00000e18, 0x669bec82 },
    { 0x00000e1c, 0x2cfb4a1b },
    { 0x00000e20, 0x80a10b4f },
    { 0x00000e24, 0x7ccd7615 },
    { 0x00000e28, 0xb0cf5002 },
    { 0x00000e2c, 0x39e9cb56 },
    { 0x00000e30, 0x399e6137 },
    { 0x00000e34, 0x161e5431 },
    { 0x00000e38, 0x519e1a9c },
    { 0x00000e3c, 0xbb7e4f9d },
    { 0x00000f00, 0x267e03a1 },
    { 0x00000f04, 0x4ed4375c },
    { 0x00000f08, 0x9897bc98 },
    { 0x00000f0c, 0x9ff46052 },
    { 0x00000f10, 0x4d99afcc },
    { 0x00000f14, 0x5c31ef73 },
    { 0x00000f18, 0xf2b4bab4 },
    { 0x00000f1c, 0x38760699 },
    { 0x00000f20, 0x854c8a9a },
    { 0x00000f24, 0x38418be5 },
    { 0x00000f28, 0xf08c6816 },
    { 0x00000f2c, 0x8a55862a },
    { 0x00000f30, 0x1567e787 },
    { 0x00000f34, 0xa238373f },
    { 0x00000f38, 0x233c5862 },
    { 0x00000f3c, 0xed9f50f1 },
    { 0x00001000, 0xf5558765 },
    { 0x00001004, 0x2767885e },
    { 0x00001008, 0x494091da },
    { 0x0000100c, 0x32db96f0 },
    { 0x00001010, 0x10bac3ee },
    { 0x00001014, 0xc6dec612 },
    { 0x00001018, 0xc9ba121c },
    { 0x0000101c, 0x7621c982 },
    { 0x00001020, 0xa8c44ad5 },
    { 0x00001024, 0x350183c2 },
    { 0x00001028, 0xf02524da },
    { 0x0000102c, 0x1724e6ef },
    { 0x00001030, 0xaa4aec22 },
    { 0x00001034, 0x7a2d22b9 },
    { 0x00001038, 0x48ff3cde },
    { 0x0000103c, 0x7f9dddf0 },
    { 0x00001100, 0xb6db50a0 },
    { 0x00001104, 0xc575e8de },
    { 0x00001108, 0x1c12aab3 },
    { 0x0000110c, 0x35b8e83b },
    { 0x00001110, 0x34370981 },
    { 0x00001114, 0x977eff83 },
    { 0x00001118, 0x693cc9fc },
    { 0x0000111c, 0x56ed521c },
    { 0x00001120, 0x5047aa2a },
    { 0x00001124, 0xac9abc44 },
    { 0x00001128, 0x69e18cbe },
    { 0x0000112c, 0xae619349 },
    { 0x00001130, 0x506cf2bc },
    { 0x00001134, 0xb5af1c9e },
    { 0x00001138, 0x9e8bad01 },
    { 0x0000113c, 0xef89cbd4 },
    { 0x00001200, 0xbd9f493e },
    { 0x00001204, 0x2a44df08 },
    { 0x00001208, 0x81d17084 },
    { 0x0000120c, 0x34a266dc },
    { 0x00001210, 0xe9629421 },
    { 0x00001214, 0xa4305e22 },
    { 0x00001218, 0x7660626d },
    { 0x0000121c, 0x6a0132ed },
    { 0x00001220, 0x6937d376 },
    { 0x00001224, 0x3da03d89 },
    { 0x00001228, 0x2d72f905 },
    { 0x0000122c, 0x4b717291 },
    { 0x00001230, 0x39f93c53 },
    { 0x00001234, 0xf4505782 },
    { 0x00001238, 0x5a83d0fa },
    { 0x0000123c, 0xa08e402b },
    { 0x00001300, 0xcdb3bb68 },
    { 0x00001304, 0xb0be4039 },
    { 0x00001308, 0x9b974b76 },
    { 0x0000130c, 0x8b6ebfa6 },
    { 0x00001310, 0xcea97b63 },
    { 0x00001314, 0xe82c98b0 },
    { 0x00001318, 0xf8ee44f7 },
    { 0x0000131c, 0xcd56bff2 },
    { 0x00001320, 0xa6aa5bbf },
    { 0x00001324, 0xf3ac10c1 },
    { 0x00001328, 0x96670839 },
    { 0x0000132c, 0x3ce1109e },
    { 0x00001330, 0xc479be7a },
    { 0x00001334, 0xa41336c5 },
    { 0x00001338, 0xebaadc8d },
    { 0x0000133c, 0x83d9c1c1 },
    { 0x00001400, 0x2ddae57b },
    { 0x00001404, 0xaa9f74fa },
    { 0x00001408, 0x591fb994 },
    { 0x0000140c, 0x17f5d501 },
    { 0x00001410, 0x55809704 },
    { 0x00001414, 0x8c9325ba },
    { 0x00001418, 0xd22a8868 },
    { 0x0000141c, 0x2579a2d9 },
    { 0x00001420, 0x1682d9d1 },
    { 0x00001424, 0xe4a06330 },
    { 0x00001428, 0x0134b674 },
    { 0x0000142c, 0xecb578e2 },
    { 0x00001430, 0xcb90f164 },
    { 0x00001434, 0x72ac1063 },
    { 0x00001438, 0x543b46bb },
    { 0x0000143c, 0x4d0ae1de },
    { 0x00001500, 0xb1a5e16f },
    { 0x00001504, 0xeb0b832a },
    { 0x00001508, 0x5fafae18 },
    { 0x0000150c, 0xf484cf14 },
    { 0x00001510, 0x7093fcf3 },
    { 0x00001514, 0xc2795540 },
    { 0x00001518, 0x77627e6f },
    { 0x0000151c, 0x1420941a },
    { 0x00001520, 0xc4fe8436 },
    { 0x00001524, 0xc01650d1 },
    { 0x00001528, 0xc2dca4e0 },
    { 0x0000152c, 0xc7b560d4 },
    { 0x00001530, 0x53e82b2f },
    { 0x00001534, 0x0737a20c },
    { 0x00001538, 0x87938ed6 },
    { 0x0000153c, 0x179f3893 },
    { 0x00001600, 0x32c8407f },
    { 0x00001604, 0x293dae19 },
    { 0x00001608, 0x51520f05 },
    { 0x0000160c, 0x78711dbe },
    { 0x00001610, 0x5336e247 },
    { 0x00001614, 0xff161984 },
    { 0x00001618, 0x6feaead4 },
    { 0x0000161c, 0xb98a8819 },
    { 0x00001620, 0x61546aa8 },
    { 0x00001624, 0x2a3e2c7a },
    { 0x00001628, 0xcaba4784 },
    { 0x0000162c, 0x91bdfae3 },
    { 0x00001630, 0xd971ea10 },
    { 0x00001634, 0x496c455f },
    { 0x00001638, 0xc61043eb },
    { 0x0000163c, 0xa0f5dec8 },
    { 0x00001700, 0x413ec2f2 },
    { 0x00001704, 0xccbba586 },
    { 0x00001708, 0x8fdcc102 },
    { 0x0000170c, 0xdf452e03 },
    { 0x00001710, 0x9cdf2958 },
    { 0x00001714, 0x62c089fb },
    { 0x00001718, 0xf34e2f4a },
    { 0x0000171c, 0x50876598 },
    { 0x00001720, 0x23a13781 },
    { 0x00001724, 0x081985b8 },
    { 0x00001728, 0x8cd1b065 },
    { 0x0000172c, 0x139d622b },
    { 0x00001730, 0x3130a1fb },
    { 0x00001734, 0x3d6de4ed },
    { 0x00001738, 0x1cc517de },
    { 0x0000173c, 0xf218ca98 },
    { 0x00001800, 0x5569ce1a },
    { 0x00001804, 0x707c8674 },
    { 0x00001808, 0x1cb7f6c7 },
    { 0x0000180c, 0x26b75a05 },
    { 0x00001810, 0x572297ac },
    { 0x00001814, 0x45637c2f },
    { 0x00001818, 0x0453386d },
    { 0x0000181c, 0xe0cd52a5 },
    { 0x00001820, 0x28d301b2 },
    { 0x00001824, 0x77b852b8 },
    { 0x00001828, 0xb9917398 },
    { 0x0000182c, 0x2bd7b6c1 },
    { 0x00001830, 0xb56d65a1 },
    { 0x00001834, 0x2c7d1961 },
    { 0x00001838, 0x06a191d9 },
    { 0x0000183c, 0xc74a521a },
    { 0x00001900, 0x449352ff },
    { 0x00001904, 0x76eb3e99 },
    { 0x00001908, 0xa29a914e },
    { 0x0000190c, 0x1a2a7da3 },
    { 0x00001910, 0x7a5d3ce7 },
    { 0x00001914, 0x2e527404 },
    { 0x00001918, 0xbdd41a88 },
    { 0x0000191c, 0x4a2801bb },
    { 0x00001920, 0x19a48ede },
    { 0x00001924, 0x7e11ba49 },
    { 0x00001928, 0x405a5cd4 },
    { 0x0000192c, 0x1ee86c33 },
    { 0x00001930, 0xb6c5dbd8 },
    { 0x00001934, 0x55e06b36 },
    { 0x00001938, 0xafb085e6 },
    { 0x0000193c, 0x773bf537 },
    { 0x00001a00, 0x1d97bde5 },
    { 0x00001a04, 0x54a7cb1c },
    { 0x00001a08, 0x0474f7d0 },
    { 0x00001a0c, 0x97db5f7d },
    { 0x00001a10, 0x82a50bd3 },
    { 0x00001a14, 0xb7b57327 },
    { 0x00001a18, 0x18b80b70 },
    { 0x00001a1c, 0x17ad37dd },
    { 0x00001a20, 0xc0431e52 },
    { 0x00001a24, 0xf1e65652 },
    { 0x00001a28, 0x956db942 },
    { 0x00001a2c, 0x1e37cfee },
    { 0x00001a30, 0xe1bc0865 },
    { 0x00001a34, 0xaa040230 },
    { 0x00001a38, 0x7f6e79e2 },
    { 0x00001a3c, 0xfc4e8e3b },
    { 0x00001b00, 0x51d2cd1d },
    { 0x00001b04, 0x21f755dd },
    { 0x00001b08, 0x2f483b30 },
    { 0x00001b0c, 0x202bfea0 },
    { 0x00001b10, 0x1376b214 },
    { 0x00001b14, 0xbb24dfd3 },
    { 0x00001b18, 0x6c046f91 },
    { 0x00001b1c, 0xcacc6f2f },
    { 0x00001b20, 0x95963054 },
    { 0x00001b24, 0xe03c581a },
    { 0x00001b28, 0x3069683b },
    { 0x00001b2c, 0x2c1639bf },
    { 0x00001b30, 0x3f7320d2 },
    { 0x00001b34, 0x8b21a011 },
    { 0x00001b38, 0xab77044e },
    { 0x00001b3c, 0x7fdc84c3 },
    { 0x00001c00, 0xd7e2b7ba },
    { 0x00001c04, 0x8b16f097 },
    { 0x00001c08, 0x7553a49f },
    { 0x00001c0c, 0x8f0b850f },
    { 0x00001c10, 0xd6c65413 },
    { 0x00001c14, 0x1e4948d5 },
    { 0x00001c18, 0xfe4355b6 },
    { 0x00001c1c, 0x03545246 },
    { 0x00001c20, 0xb0bd9939 },
    { 0x00001c24, 0x38a89bed },
    { 0x00001c28, 0xe679429e },
    { 0x00001c2c, 0x38a3a0e0 },
    { 0x00001c30, 0xc7c1114b },
    { 0x00001c34, 0xecd3e47c },
    { 0x00001c38, 0x0c4e17b8 },
    { 0x00001c3c, 0x706123ae },
    { 0x00001d00, 0xd861217f },
    { 0x00001d04, 0x6ddb8988 },
    { 0x00001d08, 0x94849440 },
    { 0x00001d0c, 0x42baf209 },
    { 0x00001d10, 0x52123ec0 },
    { 0x00001d14, 0xb1e278e5 },
    { 0x00001d18, 0x25d1fd8b },
    { 0x00001d1c, 0x83a1ca95 },
    { 0x00001d20, 0xb6917b41 },
    { 0x00001d24, 0x64b07e89 },
    { 0x00001d28, 0x11553926 },
    { 0x00001d2c, 0x92095a39 },
    { 0x00001d30, 0xad8a90b4 },
    { 0x00001d34, 0x5aa6d26c },
    { 0x00001d38, 0xb9d685ed },
    { 0x00001d3c, 0x0b367d5a },
    { 0x00001e00, 0xd5e978c6 },
    { 0x00001e04, 0x2360a38d },
    { 0x00001e08, 0x91bcc2fe },
    { 0x00001e0c, 0x2e05ec61 },
    { 0x00001e10, 0x6e256ec4 },
    { 0x00001e14, 0xd7a9d62c },
    { 0x00001e18, 0xd076a374 },
    { 0x00001e1c, 0xb4adb6c5 },
    { 0x00001e20, 0xdc59dc3e },
    { 0x00001e24, 0x79868f50 },
    { 0x00001e28, 0xa4335782 },
    { 0x00001e2c, 0xfee93b0f },
    { 0x00001e30, 0x0e332699 },
    { 0x00001e34, 0xce82d475 },
    { 0x00001e38, 0x96908e2e },
    { 0x00001e3c, 0x39a676ab },
    { 0x00001f00, 0xcc45a1e8 },
    { 0x00001f04, 0xd4229c2d },
    { 0x00001f08, 0xf76d7510 },
    { 0x00001f0c, 0x2b042a4f },
    { 0x00001f10, 0xad01c704 },
    { 0x00001f14, 0xc4fe2d1e },
    { 0x00001f18, 0xc4eb60de },
    { 0x00001f1c, 0xda45f529 },
    { 0x00001f20, 0x3a1e6da0 },
    { 0x00001f24, 0xfe1e5036 },
    { 0x00001f28, 0xe5eced0e },
    { 0x00001f2c, 0xdafe77ca },
    { 0x00001f30, 0x987727bf },
    { 0x00001f34, 0x16a26acd },
    { 0x00001f38, 0xf1d4393d },
    { 0x00001f3c, 0x0d345297 },
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

    { /* Zero-init region 0xc0000c00 size 0x2000 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 12288, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (zpage != MAP_FAILED) { memset(zpage + 3072, 0, 8192); munmap(zpage, 12288); }
    }
    { /* Zero-init region 0xc0800000 size 0x2000 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0800000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 8192); munmap(zpage, 8192); }
    }

    /* Load embedded data (replaces devmem load scripts) */
    { /* Data: base=0xc0000c00, 512 writes */
        uint8_t* dpage = (uint8_t*)mmap(NULL, 12288, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (dpage != MAP_FAILED) {
            const uint32_t (*dp)[2] = embed_0_data;
            uint32_t base_off = 0xc00;
            while ((*dp)[0] != 0xFFFFFFFF) {
                *(volatile uint32_t*)(dpage + base_off + (*dp)[0]) = (*dp)[1];
                dp++;
            }
            munmap(dpage, 12288);
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
    { /* CRC[0] zero-init: addr=0xc0800000 size=0x2000 */
        uint8_t* zpage = (uint8_t*)mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0800000);
        if (zpage != MAP_FAILED) { memset(zpage + 0, 0, 8192); munmap(zpage, 8192); }
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
    { /* CRC[0]: addr=0xc0800000, size=0x2000, expect=0xa12a1c04 */
        uint8_t* page = (uint8_t*)mmap(NULL, 8192, PROT_READ, MAP_SHARED, fd, 0xc0800000);
        if (page != MAP_FAILED) {
            uint32_t crc = calc_crc32(page + 0, 8192);
            printf("CRC[0] = 0x%08x (expect 0xa12a1c04) %s\n", crc, crc == 0xa12a1c04 ? "PASS" : "FAIL");
            if (crc != 0xa12a1c04) failures++;
            munmap(page, 8192);
        } else { perror("mmap out"); }
    }

    printf("Results: %d failures\n", failures);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return failures;
}