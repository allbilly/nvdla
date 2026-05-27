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
    { 0xd004, 0x00000000 }, /* NVDLA_CDP_S_POINTER_0 */
    { 0xd084, 0x00000001 }, /* NVDLA_CDP_D_DATOUT_SCALE_0 */
    { 0xd068, 0x00000000 }, /* NVDLA_CDP_D_DATA_FORMAT_0 */
    { 0xd050, 0xd0000000 }, /* NVDLA_CDP_D_DST_BASE_ADDR_LOW_0 */
    { 0xd0a0, 0x00000000 }, /* NVDLA_CDP_D_PERF_WRITE_STALL_0 */
    { 0xd098, 0x00000000 }, /* NVDLA_CDP_D_OUT_SATURATION_0 */
    { 0xd0b8, 0x68fbfa54 }, /* NVDLA_CDP_D_CYA_0 */
    { 0xd054, 0x00000000 }, /* NVDLA_CDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0xd0a4, 0x00000000 }, /* NVDLA_CDP_D_PERF_LUT_UFLOW_0 */
    { 0xd0ac, 0x00000000 }, /* NVDLA_CDP_D_PERF_LUT_HYBRID_0 */
    { 0xd074, 0x00000000 }, /* NVDLA_CDP_D_DATIN_OFFSET_0 */
    { 0xd08c, 0x00000000 }, /* NVDLA_CDP_D_NAN_INPUT_NUM_0 */
    { 0xd070, 0x00000000 }, /* NVDLA_CDP_D_LRN_CFG_0 */
    { 0xd04c, 0x00000000 }, /* NVDLA_CDP_D_FUNC_BYPASS_0 */
    { 0xd058, 0x00000040 }, /* NVDLA_CDP_D_DST_LINE_STRIDE_0 */
    { 0xd080, 0x00000000 }, /* NVDLA_CDP_D_DATOUT_OFFSET_0 */
    { 0xd088, 0x00000009 }, /* NVDLA_CDP_D_DATOUT_SHIFTER_0 */
    { 0xd06c, 0x00000001 }, /* NVDLA_CDP_D_NAN_FLUSH_TO_ZERO_0 */
    { 0xd078, 0x00000001 }, /* NVDLA_CDP_D_DATIN_SCALE_0 */
    { 0xd07c, 0x00000000 }, /* NVDLA_CDP_D_DATIN_SHIFTER_0 */
    { 0xd09c, 0x00000003 }, /* NVDLA_CDP_D_PERF_ENABLE_0 */
    { 0xd05c, 0x00000200 }, /* NVDLA_CDP_D_DST_SURFACE_STRIDE_0 */
    { 0xd090, 0x00000000 }, /* NVDLA_CDP_D_INF_INPUT_NUM_0 */
    { 0xd000, 0x00000000 }, /* NVDLA_CDP_S_STATUS_0 */
    { 0xd094, 0x00000000 }, /* NVDLA_CDP_D_NAN_OUTPUT_NUM_0 */
    { 0xd0b4, 0x00000000 }, /* NVDLA_CDP_D_PERF_LUT_LO_HIT_0 */
    { 0xd0b0, 0x00000000 }, /* NVDLA_CDP_D_PERF_LUT_LE_HIT_0 */
    { 0xd060, 0x00000001 }, /* NVDLA_CDP_D_DST_DMA_CFG_0 */
    { 0xd0a8, 0x00000000 }, /* NVDLA_CDP_D_PERF_LUT_OFLOW_0 */
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
    { 0xd010, 0x00000000 }, /* NVDLA_CDP_S_LUT_CFG_0 */
    { 0xd010, 0x00000000 }, /* NVDLA_CDP_S_LUT_CFG_0 */
    { 0xd014, 0x00000000 }, /* NVDLA_CDP_S_LUT_INFO_0 */
    { 0xd018, 0x00000000 }, /* NVDLA_CDP_S_LUT_LE_START_LOW_0 */
    { 0xd01c, 0x00000000 }, /* NVDLA_CDP_S_LUT_LE_START_HIGH_0 */
    { 0xd020, 0x00000012 }, /* NVDLA_CDP_S_LUT_LE_END_LOW_0 */
    { 0xd024, 0x00000000 }, /* NVDLA_CDP_S_LUT_LE_END_HIGH_0 */
    { 0xd028, 0x00040000 }, /* NVDLA_CDP_S_LUT_LO_START_LOW_0 */
    { 0xd02c, 0x00000000 }, /* NVDLA_CDP_S_LUT_LO_START_HIGH_0 */
    { 0xd030, 0x00040100 }, /* NVDLA_CDP_S_LUT_LO_END_LOW_0 */
    { 0xd034, 0x00000000 }, /* NVDLA_CDP_S_LUT_LO_END_HIGH_0 */
    { 0xd038, 0x00010001 }, /* NVDLA_CDP_S_LUT_LE_SLOPE_SCALE_0 */
    { 0xd03c, 0x00000000 }, /* NVDLA_CDP_S_LUT_LE_SLOPE_SHIFT_0 */
    { 0xd040, 0x00010001 }, /* NVDLA_CDP_S_LUT_LO_SLOPE_SCALE_0 */
    { 0xd044, 0x00000000 }, /* NVDLA_CDP_S_LUT_LO_SLOPE_SHIFT_0 */
    { 0xc004, 0x00000000 }, /* NVDLA_CDP_RDMA_S_POINTER_0 */
    { 0xc01c, 0x00000000 }, /* NVDLA_CDP_RDMA_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xc018, 0x80000000 }, /* NVDLA_CDP_RDMA_D_SRC_BASE_ADDR_LOW_0 */
    { 0xc038, 0x00000000 }, /* NVDLA_CDP_RDMA_D_PERF_ENABLE_0 */
    { 0xc024, 0x00000200 }, /* NVDLA_CDP_RDMA_D_SRC_SURFACE_STRIDE_0 */
    { 0xc034, 0x00000000 }, /* NVDLA_CDP_RDMA_D_DATA_FORMAT_0 */
    { 0xc02c, 0x00000000 }, /* NVDLA_CDP_RDMA_D_SRC_COMPRESSION_EN_0 */
    { 0xc03c, 0x00000000 }, /* NVDLA_CDP_RDMA_D_PERF_READ_STALL_0 */
    { 0xc010, 0x00000007 }, /* NVDLA_CDP_RDMA_D_DATA_CUBE_HEIGHT_0 */
    { 0xc028, 0x00000001 }, /* NVDLA_CDP_RDMA_D_SRC_DMA_CFG_0 */
    { 0xc000, 0x00000000 }, /* NVDLA_CDP_RDMA_S_STATUS_0 */
    { 0xc040, 0x0c478d52 }, /* NVDLA_CDP_RDMA_D_CYA_0 */
    { 0xc030, 0x00000000 }, /* NVDLA_CDP_RDMA_D_OPERATION_MODE_0 */
    { 0xc00c, 0x00000007 }, /* NVDLA_CDP_RDMA_D_DATA_CUBE_WIDTH_0 */
    { 0xc014, 0x0000003f }, /* NVDLA_CDP_RDMA_D_DATA_CUBE_CHANNEL_0 */
    { 0xc020, 0x00000040 }, /* NVDLA_CDP_RDMA_D_SRC_LINE_STRIDE_0 */
    { 0xd048, 0x00000001 }, /* NVDLA_CDP_D_OP_ENABLE_0 */
    { 0xc008, 0x00000001 }, /* NVDLA_CDP_RDMA_D_OP_ENABLE_0 */
    {-1, 0}
};

static const struct dla_reg program_enable[] = {
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
    { /* CRC[0]: addr=0xd0000000, size=0x1000, expect=0xfbb9c78c */
        uint8_t* page = (uint8_t*)mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0xd0000000);
        if (page != MAP_FAILED) {
            uint32_t crc = calc_crc32(page + 0, 4096);
            printf("CRC[0] = 0x%08x (expect 0xfbb9c78c) %s\n", crc, crc == 0xfbb9c78c ? "PASS" : "FAIL");
            if (crc != 0xfbb9c78c) failures++;
            munmap(page, 4096);
        } else { perror("mmap out"); }
    }

    printf("Results: %d failures\n", failures);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return failures;
}