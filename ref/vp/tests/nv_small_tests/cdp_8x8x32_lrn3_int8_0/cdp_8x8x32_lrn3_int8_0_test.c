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
    { 0xd018, 0x00000000 }, /* NVDLA_CDP_S_LUT_LE_START_LOW_0 */
    { 0xd030, 0x00000100 }, /* NVDLA_CDP_S_LUT_LO_END_LOW_0 */
    { 0xd008, 0x00000000 }, /* NVDLA_CDP_S_LUT_ACCESS_CFG_0 */
    { 0xd00c, 0x00000000 }, /* NVDLA_CDP_S_LUT_ACCESS_DATA_0 */
    { 0xd01c, 0x00000000 }, /* NVDLA_CDP_S_LUT_LE_START_HIGH_0 */
    { 0xd034, 0x00000000 }, /* NVDLA_CDP_S_LUT_LO_END_HIGH_0 */
    { 0xd010, 0x00000001 }, /* NVDLA_CDP_S_LUT_CFG_0 */
    { 0xd03c, 0x00000000 }, /* NVDLA_CDP_S_LUT_LE_SLOPE_SHIFT_0 */
    { 0xd038, 0x00000000 }, /* NVDLA_CDP_S_LUT_LE_SLOPE_SCALE_0 */
    { 0xd014, 0x00000000 }, /* NVDLA_CDP_S_LUT_INFO_0 */
    { 0xd020, 0x00000040 }, /* NVDLA_CDP_S_LUT_LE_END_LOW_0 */
    { 0xd040, 0x00000000 }, /* NVDLA_CDP_S_LUT_LO_SLOPE_SCALE_0 */
    { 0xd024, 0x00000000 }, /* NVDLA_CDP_S_LUT_LE_END_HIGH_0 */
    { 0xd02c, 0x00000000 }, /* NVDLA_CDP_S_LUT_LO_START_HIGH_0 */
    { 0xd028, 0x00000000 }, /* NVDLA_CDP_S_LUT_LO_START_LOW_0 */
    { 0xd044, 0x00000000 }, /* NVDLA_CDP_S_LUT_LO_SLOPE_SHIFT_0 */
    { 0xd080, 0x00000080 }, /* NVDLA_CDP_D_DATOUT_OFFSET_0 */
    { 0xd05c, 0x00000200 }, /* NVDLA_CDP_D_DST_SURFACE_STRIDE_0 */
    { 0xc018, 0xc0000000 }, /* NVDLA_CDP_RDMA_D_SRC_BASE_ADDR_LOW_0 */
    { 0xd060, 0x00000001 }, /* NVDLA_CDP_D_DST_DMA_CFG_0 */
    { 0xc00c, 0x00000007 }, /* NVDLA_CDP_RDMA_D_DATA_CUBE_WIDTH_0 */
    { 0xc034, 0x00000000 }, /* NVDLA_CDP_RDMA_D_DATA_FORMAT_0 */
    { 0xd078, 0x00000001 }, /* NVDLA_CDP_D_DATIN_SCALE_0 */
    { 0xd088, 0x00000000 }, /* NVDLA_CDP_D_DATOUT_SHIFTER_0 */
    { 0xd0b8, 0x00000000 }, /* NVDLA_CDP_D_CYA_0 */
    { 0xc038, 0x00000000 }, /* NVDLA_CDP_RDMA_D_PERF_ENABLE_0 */
    { 0xd070, 0x00000000 }, /* NVDLA_CDP_D_LRN_CFG_0 */
    { 0xc014, 0x0000001f }, /* NVDLA_CDP_RDMA_D_DATA_CUBE_CHANNEL_0 */
    { 0xd068, 0x00000000 }, /* NVDLA_CDP_D_DATA_FORMAT_0 */
    { 0xd07c, 0x00000000 }, /* NVDLA_CDP_D_DATIN_SHIFTER_0 */
    { 0xd09c, 0x00000000 }, /* NVDLA_CDP_D_PERF_ENABLE_0 */
    { 0xc01c, 0x00000000 }, /* NVDLA_CDP_RDMA_D_SRC_BASE_ADDR_HIGH_0 */
    { 0xd054, 0x00000000 }, /* NVDLA_CDP_D_DST_BASE_ADDR_HIGH_0 */
    { 0xc028, 0x00000001 }, /* NVDLA_CDP_RDMA_D_SRC_DMA_CFG_0 */
    { 0xd084, 0x00000001 }, /* NVDLA_CDP_D_DATOUT_SCALE_0 */
    { 0xd074, 0x00000080 }, /* NVDLA_CDP_D_DATIN_OFFSET_0 */
    { 0xd06c, 0x00000000 }, /* NVDLA_CDP_D_NAN_FLUSH_TO_ZERO_0 */
    { 0xd04c, 0x00000003 }, /* NVDLA_CDP_D_FUNC_BYPASS_0 */
    { 0xd050, 0xc0080000 }, /* NVDLA_CDP_D_DST_BASE_ADDR_LOW_0 */
    { 0xc040, 0x00000000 }, /* NVDLA_CDP_RDMA_D_CYA_0 */
    { 0xc024, 0x00000200 }, /* NVDLA_CDP_RDMA_D_SRC_SURFACE_STRIDE_0 */
    { 0xd058, 0x00000040 }, /* NVDLA_CDP_D_DST_LINE_STRIDE_0 */
    { 0xc020, 0x00000040 }, /* NVDLA_CDP_RDMA_D_SRC_LINE_STRIDE_0 */
    { 0xc010, 0x00000007 }, /* NVDLA_CDP_RDMA_D_DATA_CUBE_HEIGHT_0 */
    { 0xc008, 0x00000001 }, /* NVDLA_CDP_RDMA_D_OP_ENABLE_0 */
    { 0xd048, 0x00000001 }, /* NVDLA_CDP_D_OP_ENABLE_0 */
    {-1, 0}
};

static const struct dla_reg program_enable[] = {
    {-1, 0}
};

/* Embedded data blocks (loaded via mmap instead of devmem scripts) */
static const uint32_t embed_0_data[][2] = {
    { 0x00000000, 0xf16d21a7 },
    { 0x00000004, 0x83fb15db },
    { 0x00000008, 0x8f580083 },
    { 0x0000000c, 0x0b486eeb },
    { 0x00000010, 0x54267e0e },
    { 0x00000014, 0xcaff188a },
    { 0x00000018, 0x240253d2 },
    { 0x0000001c, 0x22230389 },
    { 0x00000020, 0xc49af67e },
    { 0x00000024, 0x6b70ee49 },
    { 0x00000028, 0xb9a2fc6f },
    { 0x0000002c, 0x77b4d654 },
    { 0x00000030, 0x7747b7e5 },
    { 0x00000034, 0x29cb0372 },
    { 0x00000038, 0x08f93005 },
    { 0x0000003c, 0x3d227e25 },
    { 0x00000040, 0x0886b5ed },
    { 0x00000044, 0xe1b8925e },
    { 0x00000048, 0x8bb99b04 },
    { 0x0000004c, 0xd1188ad0 },
    { 0x00000050, 0x9bb97b62 },
    { 0x00000054, 0x85afc73d },
    { 0x00000058, 0xe90b3050 },
    { 0x0000005c, 0x8556e709 },
    { 0x00000060, 0xcdaa919c },
    { 0x00000064, 0x6ee3ee26 },
    { 0x00000068, 0x0d0258b0 },
    { 0x0000006c, 0xa5b5e064 },
    { 0x00000070, 0x0e193deb },
    { 0x00000074, 0x3841b503 },
    { 0x00000078, 0xf85f6135 },
    { 0x0000007c, 0xf1fde8dd },
    { 0x00000080, 0xe49efa55 },
    { 0x00000084, 0x4687c4c1 },
    { 0x00000088, 0x0355e6ff },
    { 0x0000008c, 0x439c689c },
    { 0x00000090, 0x4376d403 },
    { 0x00000094, 0xbf2cbc7a },
    { 0x00000098, 0x8109f26b },
    { 0x0000009c, 0x17d72860 },
    { 0x000000a0, 0xdb442542 },
    { 0x000000a4, 0x7a213692 },
    { 0x000000a8, 0x35525201 },
    { 0x000000ac, 0xe896b981 },
    { 0x000000b0, 0x5f210ce9 },
    { 0x000000b4, 0x806a1550 },
    { 0x000000b8, 0x6512ce0f },
    { 0x000000bc, 0x2d7f6d15 },
    { 0x000000c0, 0xa4e71358 },
    { 0x000000c4, 0x473c8055 },
    { 0x000000c8, 0xf4f05398 },
    { 0x000000cc, 0x9c5a2217 },
    { 0x000000d0, 0x8ab18cf3 },
    { 0x000000d4, 0x4d89e9ef },
    { 0x000000d8, 0x3fec2c58 },
    { 0x000000dc, 0xdcf622be },
    { 0x000000e0, 0x124b02e9 },
    { 0x000000e4, 0xe5a84734 },
    { 0x000000e8, 0x38411196 },
    { 0x000000ec, 0x6f41cc3e },
    { 0x000000f0, 0xe7a93d47 },
    { 0x000000f4, 0xdaadb632 },
    { 0x000000f8, 0xc8ea2fcb },
    { 0x000000fc, 0xca3313f9 },
    { 0x00000100, 0x122c1d26 },
    { 0x00000104, 0x323736e0 },
    { 0x00000108, 0x9ebe989c },
    { 0x0000010c, 0x00549f8b },
    { 0x00000110, 0x1e1614d2 },
    { 0x00000114, 0xae276e25 },
    { 0x00000118, 0xeae4e16c },
    { 0x0000011c, 0x91795a11 },
    { 0x00000120, 0xdf66f148 },
    { 0x00000124, 0xbb2dfce9 },
    { 0x00000128, 0xcabd3424 },
    { 0x0000012c, 0x27efe429 },
    { 0x00000130, 0x906a7a69 },
    { 0x00000134, 0xe24d0049 },
    { 0x00000138, 0x17f6c120 },
    { 0x0000013c, 0x6ec0071b },
    { 0x00000140, 0x5f7c1436 },
    { 0x00000144, 0xb322632a },
    { 0x00000148, 0xbf996935 },
    { 0x0000014c, 0x528f284f },
    { 0x00000150, 0x12880840 },
    { 0x00000154, 0x10e1c277 },
    { 0x00000158, 0xe803960d },
    { 0x0000015c, 0x2ce6141a },
    { 0x00000160, 0x13f66e48 },
    { 0x00000164, 0x76605da6 },
    { 0x00000168, 0x0e122aaa },
    { 0x0000016c, 0xfc7e5af9 },
    { 0x00000170, 0x2ece8f29 },
    { 0x00000174, 0x9dee387f },
    { 0x00000178, 0x0b991b03 },
    { 0x0000017c, 0xbac84147 },
    { 0x00000180, 0x56284783 },
    { 0x00000184, 0x11b9e733 },
    { 0x00000188, 0xbd04a334 },
    { 0x0000018c, 0xa47ab57a },
    { 0x00000190, 0x21c768a2 },
    { 0x00000194, 0xcb10da42 },
    { 0x00000198, 0x35dc940c },
    { 0x0000019c, 0x127b646a },
    { 0x000001a0, 0x760755aa },
    { 0x000001a4, 0xe303b3bf },
    { 0x000001a8, 0xcdfcd2e3 },
    { 0x000001ac, 0xc5c5ae35 },
    { 0x000001b0, 0x1279cf6c },
    { 0x000001b4, 0xdd3e35a9 },
    { 0x000001b8, 0xf8a073e9 },
    { 0x000001bc, 0x909913c0 },
    { 0x000001c0, 0xb09b1d4d },
    { 0x000001c4, 0x3f5972dd },
    { 0x000001c8, 0x72b44193 },
    { 0x000001cc, 0x0df91ef4 },
    { 0x000001d0, 0x2353b738 },
    { 0x000001d4, 0x27ca59d8 },
    { 0x000001d8, 0x09f1505a },
    { 0x000001dc, 0xcd5cd5fb },
    { 0x000001e0, 0x4dec2c13 },
    { 0x000001e4, 0xafda0d76 },
    { 0x000001e8, 0x6a836a93 },
    { 0x000001ec, 0x92755f6b },
    { 0x000001f0, 0xc31a3908 },
    { 0x000001f4, 0x47770814 },
    { 0x000001f8, 0x31a31f0f },
    { 0x000001fc, 0xa75198dc },
    { 0x00000200, 0x3e7d701e },
    { 0x00000204, 0x11ffb099 },
    { 0x00000208, 0x3ff36135 },
    { 0x0000020c, 0x36663860 },
    { 0x00000210, 0x97a344b6 },
    { 0x00000214, 0x179ddc7c },
    { 0x00000218, 0x15e82df6 },
    { 0x0000021c, 0x494e494d },
    { 0x00000220, 0x99248fda },
    { 0x00000224, 0xc29d420e },
    { 0x00000228, 0x60a4cf91 },
    { 0x0000022c, 0x628b4af2 },
    { 0x00000230, 0x710af6a6 },
    { 0x00000234, 0xa6952241 },
    { 0x00000238, 0xc556d9cd },
    { 0x0000023c, 0x0bdde8e2 },
    { 0x00000240, 0xbbc4ef32 },
    { 0x00000244, 0x30bf64ae },
    { 0x00000248, 0x3e43b405 },
    { 0x0000024c, 0x89f6d6c5 },
    { 0x00000250, 0x49e786f8 },
    { 0x00000254, 0xa13ba715 },
    { 0x00000258, 0xf24b86f7 },
    { 0x0000025c, 0xcfcee910 },
    { 0x00000260, 0x6d676eec },
    { 0x00000264, 0x93e7b5c2 },
    { 0x00000268, 0x9733e81c },
    { 0x0000026c, 0xe1a2865d },
    { 0x00000270, 0x8222a7d1 },
    { 0x00000274, 0xfea2c032 },
    { 0x00000278, 0xf9b2fa04 },
    { 0x0000027c, 0xa49595bc },
    { 0x00000280, 0x536cdb20 },
    { 0x00000284, 0x5b628093 },
    { 0x00000288, 0x559d599a },
    { 0x0000028c, 0x9ec471d6 },
    { 0x00000290, 0x669e1d27 },
    { 0x00000294, 0x31fe6e97 },
    { 0x00000298, 0x00d8eea3 },
    { 0x0000029c, 0x9379bd68 },
    { 0x000002a0, 0x4d18bb47 },
    { 0x000002a4, 0xfd36bc17 },
    { 0x000002a8, 0x6ec4b8d4 },
    { 0x000002ac, 0xa2ac7cd6 },
    { 0x000002b0, 0x7cf73ec2 },
    { 0x000002b4, 0x31d96644 },
    { 0x000002b8, 0x6c258fe5 },
    { 0x000002bc, 0xc1bcb2b8 },
    { 0x000002c0, 0x45b6c84e },
    { 0x000002c4, 0xf3a7d454 },
    { 0x000002c8, 0x388b7a50 },
    { 0x000002cc, 0xec23a3e3 },
    { 0x000002d0, 0x5e56720f },
    { 0x000002d4, 0x386d45f9 },
    { 0x000002d8, 0x6bfad58c },
    { 0x000002dc, 0x7484d7e1 },
    { 0x000002e0, 0x4ae194d3 },
    { 0x000002e4, 0x387a02f9 },
    { 0x000002e8, 0x309193be },
    { 0x000002ec, 0xda50b04d },
    { 0x000002f0, 0x1c220ef0 },
    { 0x000002f4, 0x28ae0174 },
    { 0x000002f8, 0x7c9fa238 },
    { 0x000002fc, 0xf90a99c6 },
    { 0x00000300, 0xe96e0183 },
    { 0x00000304, 0x12f59301 },
    { 0x00000308, 0xeaead532 },
    { 0x0000030c, 0xb8ffcd80 },
    { 0x00000310, 0x136541f6 },
    { 0x00000314, 0xf531c5da },
    { 0x00000318, 0x846cfd4e },
    { 0x0000031c, 0xb4f5b49d },
    { 0x00000320, 0x7f279b96 },
    { 0x00000324, 0x1ffbb2c3 },
    { 0x00000328, 0x31ecf31f },
    { 0x0000032c, 0xb4d70b11 },
    { 0x00000330, 0x027eccb1 },
    { 0x00000334, 0xc2a0fe4a },
    { 0x00000338, 0xf638ba3a },
    { 0x0000033c, 0xdb2829e2 },
    { 0x00000340, 0xa982d0b9 },
    { 0x00000344, 0xe0653147 },
    { 0x00000348, 0x428256cc },
    { 0x0000034c, 0x15aa00ae },
    { 0x00000350, 0x7b655515 },
    { 0x00000354, 0x2fb0a4dc },
    { 0x00000358, 0xaf2c9e5c },
    { 0x0000035c, 0xc9d7935d },
    { 0x00000360, 0x690630c9 },
    { 0x00000364, 0x4a7e851c },
    { 0x00000368, 0x4927c228 },
    { 0x0000036c, 0x0b053b19 },
    { 0x00000370, 0xe576c05b },
    { 0x00000374, 0xca4ecd81 },
    { 0x00000378, 0xe686fcd0 },
    { 0x0000037c, 0x7514009c },
    { 0x00000380, 0x328e4be6 },
    { 0x00000384, 0x18464432 },
    { 0x00000388, 0x5b7750a4 },
    { 0x0000038c, 0x4b85bdc0 },
    { 0x00000390, 0x2fa38d63 },
    { 0x00000394, 0xa6b3c632 },
    { 0x00000398, 0x052c63eb },
    { 0x0000039c, 0x5235f864 },
    { 0x000003a0, 0xcea68b45 },
    { 0x000003a4, 0xf591761a },
    { 0x000003a8, 0xb4345f43 },
    { 0x000003ac, 0x63f4af61 },
    { 0x000003b0, 0xc3b3ae38 },
    { 0x000003b4, 0xb3a12941 },
    { 0x000003b8, 0xb6c10cd4 },
    { 0x000003bc, 0x39154445 },
    { 0x000003c0, 0xfe2da687 },
    { 0x000003c4, 0xeb9af3dc },
    { 0x000003c8, 0x13ad086c },
    { 0x000003cc, 0x106daded },
    { 0x000003d0, 0xd5ef2afe },
    { 0x000003d4, 0x028b8183 },
    { 0x000003d8, 0x7c28b728 },
    { 0x000003dc, 0x018ea7bd },
    { 0x000003e0, 0x83b00812 },
    { 0x000003e4, 0x8dccf489 },
    { 0x000003e8, 0xbdc783b3 },
    { 0x000003ec, 0x9d90e667 },
    { 0x000003f0, 0xbe742c2a },
    { 0x000003f4, 0x8b2e5c5f },
    { 0x000003f8, 0x1172c936 },
    { 0x000003fc, 0xbc565ce3 },
    { 0x00000400, 0x4166a0de },
    { 0x00000404, 0xa359851f },
    { 0x00000408, 0xff26a31f },
    { 0x0000040c, 0x1eb4c6c2 },
    { 0x00000410, 0x1f423fe3 },
    { 0x00000414, 0xb6837b9a },
    { 0x00000418, 0xbc5eb20b },
    { 0x0000041c, 0x62da933e },
    { 0x00000420, 0x0ce3f5e5 },
    { 0x00000424, 0xa5141ef7 },
    { 0x00000428, 0x10819525 },
    { 0x0000042c, 0x82b1461a },
    { 0x00000430, 0x81393af7 },
    { 0x00000434, 0x401a406f },
    { 0x00000438, 0x537fd22f },
    { 0x0000043c, 0xbd121751 },
    { 0x00000440, 0x38b1d800 },
    { 0x00000444, 0xec0a6f2d },
    { 0x00000448, 0x0fe6f66d },
    { 0x0000044c, 0x5b90d0c4 },
    { 0x00000450, 0x8255e0f1 },
    { 0x00000454, 0x66d0d9aa },
    { 0x00000458, 0x180386e4 },
    { 0x0000045c, 0xadd6e1a4 },
    { 0x00000460, 0x3166d7cd },
    { 0x00000464, 0x3d276ce1 },
    { 0x00000468, 0x1f2eb052 },
    { 0x0000046c, 0x5900abfb },
    { 0x00000470, 0x10720c3e },
    { 0x00000474, 0x37bbb460 },
    { 0x00000478, 0x4e8545f3 },
    { 0x0000047c, 0x39dc3943 },
    { 0x00000480, 0x86968ff3 },
    { 0x00000484, 0xf4dd3049 },
    { 0x00000488, 0x8602eca4 },
    { 0x0000048c, 0x5b2288e3 },
    { 0x00000490, 0x287d67f1 },
    { 0x00000494, 0x0ef1aa8f },
    { 0x00000498, 0x0d437c7f },
    { 0x0000049c, 0x9729fbfe },
    { 0x000004a0, 0xf49a2e55 },
    { 0x000004a4, 0x18d2c710 },
    { 0x000004a8, 0x161b5bee },
    { 0x000004ac, 0x24c3cecf },
    { 0x000004b0, 0x65062cef },
    { 0x000004b4, 0xa49d7a2d },
    { 0x000004b8, 0x2e398279 },
    { 0x000004bc, 0xccefa242 },
    { 0x000004c0, 0x640a87cd },
    { 0x000004c4, 0xd5c4048b },
    { 0x000004c8, 0x0ccdff05 },
    { 0x000004cc, 0x3c767dc4 },
    { 0x000004d0, 0xb07494e3 },
    { 0x000004d4, 0x5ebdaf36 },
    { 0x000004d8, 0x1f586745 },
    { 0x000004dc, 0xb02fa8f8 },
    { 0x000004e0, 0xb231705c },
    { 0x000004e4, 0x79ec42f4 },
    { 0x000004e8, 0x9c164b7b },
    { 0x000004ec, 0x54855649 },
    { 0x000004f0, 0xc037d665 },
    { 0x000004f4, 0xd7efda8d },
    { 0x000004f8, 0xee35a3fa },
    { 0x000004fc, 0x12cc6512 },
    { 0x00000500, 0xcdd8c3e5 },
    { 0x00000504, 0xf4cae531 },
    { 0x00000508, 0x6e320055 },
    { 0x0000050c, 0x60014dd6 },
    { 0x00000510, 0x2c14052a },
    { 0x00000514, 0x6491af8a },
    { 0x00000518, 0xf8783d37 },
    { 0x0000051c, 0x5d1a4229 },
    { 0x00000520, 0x1f445474 },
    { 0x00000524, 0xbb264758 },
    { 0x00000528, 0xe759cfba },
    { 0x0000052c, 0xc6b2279a },
    { 0x00000530, 0xa7d723c4 },
    { 0x00000534, 0xdfd76e3b },
    { 0x00000538, 0xafdc7d34 },
    { 0x0000053c, 0xa2dc24a5 },
    { 0x00000540, 0x617d0f07 },
    { 0x00000544, 0xcd137daa },
    { 0x00000548, 0xda29d52e },
    { 0x0000054c, 0xdbf6e220 },
    { 0x00000550, 0x8cf4924f },
    { 0x00000554, 0x5c02f848 },
    { 0x00000558, 0xb4e4abad },
    { 0x0000055c, 0x2d848b8d },
    { 0x00000560, 0x388cae5a },
    { 0x00000564, 0x9538cdaf },
    { 0x00000568, 0x944f6318 },
    { 0x0000056c, 0x85c80739 },
    { 0x00000570, 0x2ec93342 },
    { 0x00000574, 0xb45af6b0 },
    { 0x00000578, 0xe4e1b9a7 },
    { 0x0000057c, 0x4f1db5c7 },
    { 0x00000580, 0xffb68a46 },
    { 0x00000584, 0xb1f0bbb7 },
    { 0x00000588, 0xd19eacc6 },
    { 0x0000058c, 0xcf7d8a5d },
    { 0x00000590, 0x105b2207 },
    { 0x00000594, 0xa3b2c799 },
    { 0x00000598, 0xedc3fa7a },
    { 0x0000059c, 0xff18ea9e },
    { 0x000005a0, 0x40b9d43e },
    { 0x000005a4, 0xb42155fa },
    { 0x000005a8, 0xf5f926ea },
    { 0x000005ac, 0x6793485d },
    { 0x000005b0, 0x5a6a557d },
    { 0x000005b4, 0x010205a9 },
    { 0x000005b8, 0x82b3ef07 },
    { 0x000005bc, 0x6c44d949 },
    { 0x000005c0, 0x5ddb35c4 },
    { 0x000005c4, 0xcf621dcd },
    { 0x000005c8, 0x7eecaa8c },
    { 0x000005cc, 0xec9eaf4e },
    { 0x000005d0, 0xf3fb273e },
    { 0x000005d4, 0xe9c2831e },
    { 0x000005d8, 0x8bcb405a },
    { 0x000005dc, 0x9f3a8697 },
    { 0x000005e0, 0xd74c08d5 },
    { 0x000005e4, 0x4b5f4efe },
    { 0x000005e8, 0x69e48a61 },
    { 0x000005ec, 0x433ae38a },
    { 0x000005f0, 0x2dec9b45 },
    { 0x000005f4, 0x67b220db },
    { 0x000005f8, 0x66bd3ebb },
    { 0x000005fc, 0xc08b412f },
    { 0x00000600, 0x6bafdc4c },
    { 0x00000604, 0xc332a26e },
    { 0x00000608, 0x626f6905 },
    { 0x0000060c, 0x7b094ffa },
    { 0x00000610, 0x234e91b9 },
    { 0x00000614, 0x8a2d4d3f },
    { 0x00000618, 0x868e9a58 },
    { 0x0000061c, 0xa7526be8 },
    { 0x00000620, 0xc61f0c1e },
    { 0x00000624, 0x0fc08cca },
    { 0x00000628, 0x983de92b },
    { 0x0000062c, 0x401f9116 },
    { 0x00000630, 0xb54ae294 },
    { 0x00000634, 0xfaa567eb },
    { 0x00000638, 0xe2729aad },
    { 0x0000063c, 0xa86ddc60 },
    { 0x00000640, 0x6bdea08f },
    { 0x00000644, 0xe793cf4d },
    { 0x00000648, 0x0f46d1ef },
    { 0x0000064c, 0x2e72eee6 },
    { 0x00000650, 0xb6e08ba9 },
    { 0x00000654, 0x5d080efa },
    { 0x00000658, 0x302471a8 },
    { 0x0000065c, 0x5e73d331 },
    { 0x00000660, 0x1d9f0406 },
    { 0x00000664, 0x1144d535 },
    { 0x00000668, 0xd92c8c60 },
    { 0x0000066c, 0xe148ae16 },
    { 0x00000670, 0x33288054 },
    { 0x00000674, 0x3d7eed22 },
    { 0x00000678, 0xc28822fd },
    { 0x0000067c, 0xce5c854d },
    { 0x00000680, 0x309a66c6 },
    { 0x00000684, 0x29e89ea7 },
    { 0x00000688, 0x32f778bf },
    { 0x0000068c, 0xc78063f5 },
    { 0x00000690, 0x791ea80b },
    { 0x00000694, 0x5280657a },
    { 0x00000698, 0x040edc19 },
    { 0x0000069c, 0x528dee4f },
    { 0x000006a0, 0x65a99230 },
    { 0x000006a4, 0x87f37829 },
    { 0x000006a8, 0xec7cd3ac },
    { 0x000006ac, 0x057a5e57 },
    { 0x000006b0, 0x7698ff41 },
    { 0x000006b4, 0xc89883ac },
    { 0x000006b8, 0x6004f0a6 },
    { 0x000006bc, 0x1c041617 },
    { 0x000006c0, 0xa4fad844 },
    { 0x000006c4, 0xf73dedd2 },
    { 0x000006c8, 0x4b9d31f4 },
    { 0x000006cc, 0x0dc2d61f },
    { 0x000006d0, 0xfa6dd82c },
    { 0x000006d4, 0xa05f029f },
    { 0x000006d8, 0xf57420d5 },
    { 0x000006dc, 0x1ae7b991 },
    { 0x000006e0, 0x48350da3 },
    { 0x000006e4, 0xa6a83243 },
    { 0x000006e8, 0xb0b0e33b },
    { 0x000006ec, 0x283c445e },
    { 0x000006f0, 0xa6dd1e3c },
    { 0x000006f4, 0x69572d75 },
    { 0x000006f8, 0x083123d1 },
    { 0x000006fc, 0xabb68bb2 },
    { 0x00000700, 0x056e6247 },
    { 0x00000704, 0xf3b39a09 },
    { 0x00000708, 0xd78772ae },
    { 0x0000070c, 0xecb50c72 },
    { 0x00000710, 0x690010d3 },
    { 0x00000714, 0x1adef872 },
    { 0x00000718, 0x7a7ed873 },
    { 0x0000071c, 0xbd3cbc29 },
    { 0x00000720, 0x602bd476 },
    { 0x00000724, 0xa374e41f },
    { 0x00000728, 0x99a7321c },
    { 0x0000072c, 0x6c57644b },
    { 0x00000730, 0xbdc46f03 },
    { 0x00000734, 0x36f1adc9 },
    { 0x00000738, 0xb720a64b },
    { 0x0000073c, 0xd37dc730 },
    { 0x00000740, 0xd5320ad4 },
    { 0x00000744, 0x9be4470f },
    { 0x00000748, 0x113a7a47 },
    { 0x0000074c, 0x3d82508a },
    { 0x00000750, 0x2333e625 },
    { 0x00000754, 0x652aa411 },
    { 0x00000758, 0x3b5c2c03 },
    { 0x0000075c, 0xdf7eb8f9 },
    { 0x00000760, 0xc9313d62 },
    { 0x00000764, 0xc0bc1688 },
    { 0x00000768, 0x6026e5a5 },
    { 0x0000076c, 0x20f4ce54 },
    { 0x00000770, 0x1608eb01 },
    { 0x00000774, 0x4d702c04 },
    { 0x00000778, 0xdd5e9c14 },
    { 0x0000077c, 0xda23afbb },
    { 0x00000780, 0xc1f2f1d7 },
    { 0x00000784, 0x2c43de40 },
    { 0x00000788, 0x67f61ac6 },
    { 0x0000078c, 0xd0900cbf },
    { 0x00000790, 0xba6a12c5 },
    { 0x00000794, 0x2a580756 },
    { 0x00000798, 0xb04d9eda },
    { 0x0000079c, 0x2f2e9bc8 },
    { 0x000007a0, 0x03dd10be },
    { 0x000007a4, 0x2e9a7c8e },
    { 0x000007a8, 0xc0442561 },
    { 0x000007ac, 0xd468e0a0 },
    { 0x000007b0, 0x99fb2cd6 },
    { 0x000007b4, 0x8612d998 },
    { 0x000007b8, 0x2ec73304 },
    { 0x000007bc, 0x5313d272 },
    { 0x000007c0, 0xc454ca71 },
    { 0x000007c4, 0xce75355b },
    { 0x000007c8, 0x9057b0fd },
    { 0x000007cc, 0x1de28c7b },
    { 0x000007d0, 0xbff0c7b6 },
    { 0x000007d4, 0x4eab9fa2 },
    { 0x000007d8, 0xc4a0ecd7 },
    { 0x000007dc, 0x62e80e47 },
    { 0x000007e0, 0xd84d1d5d },
    { 0x000007e4, 0xd7775e67 },
    { 0x000007e8, 0x5fa6775f },
    { 0x000007ec, 0x51489467 },
    { 0x000007f0, 0x539196b5 },
    { 0x000007f4, 0xe2a4ed13 },
    { 0x000007f8, 0xd6dd6651 },
    { 0x000007fc, 0x313c376f },
    { 0xFFFFFFFF, 0 }
};

static const uint32_t embed_1_data[][2] = {
    { 0x00000000, 0x00000000 },
    { 0x00000004, 0x00000000 },
    { 0x00000008, 0x00000000 },
    { 0x0000000c, 0x00000000 },
    { 0x00000010, 0x00000000 },
    { 0x00000014, 0x00000000 },
    { 0x00000018, 0x00000000 },
    { 0x0000001c, 0x00000000 },
    { 0x00000020, 0x00000000 },
    { 0x00000024, 0x00000000 },
    { 0x00000028, 0x00000000 },
    { 0x0000002c, 0x00000000 },
    { 0x00000030, 0x00000000 },
    { 0x00000034, 0x00000000 },
    { 0x00000038, 0x00000000 },
    { 0x0000003c, 0x00000000 },
    { 0x00000040, 0x00000000 },
    { 0x00000044, 0x00000000 },
    { 0x00000048, 0x00000000 },
    { 0x0000004c, 0x00000000 },
    { 0x00000050, 0x00000000 },
    { 0x00000054, 0x00000000 },
    { 0x00000058, 0x00000000 },
    { 0x0000005c, 0x00000000 },
    { 0x00000060, 0x00000000 },
    { 0x00000064, 0x00000000 },
    { 0x00000068, 0x00000000 },
    { 0x0000006c, 0x00000000 },
    { 0x00000070, 0x00000000 },
    { 0x00000074, 0x00000000 },
    { 0x00000078, 0x00000000 },
    { 0x0000007c, 0x00000000 },
    { 0x00000080, 0x00000000 },
    { 0x00000084, 0x00000000 },
    { 0x00000088, 0x00000000 },
    { 0x0000008c, 0x00000000 },
    { 0x00000090, 0x00000000 },
    { 0x00000094, 0x00000000 },
    { 0x00000098, 0x00000000 },
    { 0x0000009c, 0x00000000 },
    { 0x000000a0, 0x00000000 },
    { 0x000000a4, 0x00000000 },
    { 0x000000a8, 0x00000000 },
    { 0x000000ac, 0x00000000 },
    { 0x000000b0, 0x00000000 },
    { 0x000000b4, 0x00000000 },
    { 0x000000b8, 0x00000000 },
    { 0x000000bc, 0x00000000 },
    { 0x000000c0, 0x00000000 },
    { 0x000000c4, 0x00000000 },
    { 0x000000c8, 0x00000000 },
    { 0x000000cc, 0x00000000 },
    { 0x000000d0, 0x00000000 },
    { 0x000000d4, 0x00000000 },
    { 0x000000d8, 0x00000000 },
    { 0x000000dc, 0x00000000 },
    { 0x000000e0, 0x00000000 },
    { 0x000000e4, 0x00000000 },
    { 0x000000e8, 0x00000000 },
    { 0x000000ec, 0x00000000 },
    { 0x000000f0, 0x00000000 },
    { 0x000000f4, 0x00000000 },
    { 0x000000f8, 0x00000000 },
    { 0x000000fc, 0x00000000 },
    { 0x00000100, 0x00000000 },
    { 0x00000104, 0x00000000 },
    { 0x00000108, 0x00000000 },
    { 0x0000010c, 0x00000000 },
    { 0x00000110, 0x00000000 },
    { 0x00000114, 0x00000000 },
    { 0x00000118, 0x00000000 },
    { 0x0000011c, 0x00000000 },
    { 0x00000120, 0x00000000 },
    { 0x00000124, 0x00000000 },
    { 0x00000128, 0x00000000 },
    { 0x0000012c, 0x00000000 },
    { 0x00000130, 0x00000000 },
    { 0x00000134, 0x00000000 },
    { 0x00000138, 0x00000000 },
    { 0x0000013c, 0x00000000 },
    { 0x00000140, 0x00000000 },
    { 0x00000144, 0x00000000 },
    { 0x00000148, 0x00000000 },
    { 0x0000014c, 0x00000000 },
    { 0x00000150, 0x00000000 },
    { 0x00000154, 0x00000000 },
    { 0x00000158, 0x00000000 },
    { 0x0000015c, 0x00000000 },
    { 0x00000160, 0x00000000 },
    { 0x00000164, 0x00000000 },
    { 0x00000168, 0x00000000 },
    { 0x0000016c, 0x00000000 },
    { 0x00000170, 0x00000000 },
    { 0x00000174, 0x00000000 },
    { 0x00000178, 0x00000000 },
    { 0x0000017c, 0x00000000 },
    { 0x00000180, 0x00000000 },
    { 0x00000184, 0x00000000 },
    { 0x00000188, 0x00000000 },
    { 0x0000018c, 0x00000000 },
    { 0x00000190, 0x00000000 },
    { 0x00000194, 0x00000000 },
    { 0x00000198, 0x00000000 },
    { 0x0000019c, 0x00000000 },
    { 0x000001a0, 0x00000000 },
    { 0x000001a4, 0x00000000 },
    { 0x000001a8, 0x00000000 },
    { 0x000001ac, 0x00000000 },
    { 0x000001b0, 0x00000000 },
    { 0x000001b4, 0x00000000 },
    { 0x000001b8, 0x00000000 },
    { 0x000001bc, 0x00000000 },
    { 0x000001c0, 0x00000000 },
    { 0x000001c4, 0x00000000 },
    { 0x000001c8, 0x00000000 },
    { 0x000001cc, 0x00000000 },
    { 0x000001d0, 0x00000000 },
    { 0x000001d4, 0x00000000 },
    { 0x000001d8, 0x00000000 },
    { 0x000001dc, 0x00000000 },
    { 0x000001e0, 0x00000000 },
    { 0x000001e4, 0x00000000 },
    { 0x000001e8, 0x00000000 },
    { 0x000001ec, 0x00000000 },
    { 0x000001f0, 0x00000000 },
    { 0x000001f4, 0x00000000 },
    { 0x000001f8, 0x00000000 },
    { 0x000001fc, 0x00000000 },
    { 0x00000200, 0x00000000 },
    { 0x00000204, 0x00000000 },
    { 0x00000208, 0x00000000 },
    { 0x0000020c, 0x00000000 },
    { 0x00000210, 0x00000000 },
    { 0x00000214, 0x00000000 },
    { 0x00000218, 0x00000000 },
    { 0x0000021c, 0x00000000 },
    { 0x00000220, 0x00000000 },
    { 0x00000224, 0x00000000 },
    { 0x00000228, 0x00000000 },
    { 0x0000022c, 0x00000000 },
    { 0x00000230, 0x00000000 },
    { 0x00000234, 0x00000000 },
    { 0x00000238, 0x00000000 },
    { 0x0000023c, 0x00000000 },
    { 0x00000240, 0x00000000 },
    { 0x00000244, 0x00000000 },
    { 0x00000248, 0x00000000 },
    { 0x0000024c, 0x00000000 },
    { 0x00000250, 0x00000000 },
    { 0x00000254, 0x00000000 },
    { 0x00000258, 0x00000000 },
    { 0x0000025c, 0x00000000 },
    { 0x00000260, 0x00000000 },
    { 0x00000264, 0x00000000 },
    { 0x00000268, 0x00000000 },
    { 0x0000026c, 0x00000000 },
    { 0x00000270, 0x00000000 },
    { 0x00000274, 0x00000000 },
    { 0x00000278, 0x00000000 },
    { 0x0000027c, 0x00000000 },
    { 0x00000280, 0x00000000 },
    { 0x00000284, 0x00000000 },
    { 0x00000288, 0x00000000 },
    { 0x0000028c, 0x00000000 },
    { 0x00000290, 0x00000000 },
    { 0x00000294, 0x00000000 },
    { 0x00000298, 0x00000000 },
    { 0x0000029c, 0x00000000 },
    { 0x000002a0, 0x00000000 },
    { 0x000002a4, 0x00000000 },
    { 0x000002a8, 0x00000000 },
    { 0x000002ac, 0x00000000 },
    { 0x000002b0, 0x00000000 },
    { 0x000002b4, 0x00000000 },
    { 0x000002b8, 0x00000000 },
    { 0x000002bc, 0x00000000 },
    { 0x000002c0, 0x00000000 },
    { 0x000002c4, 0x00000000 },
    { 0x000002c8, 0x00000000 },
    { 0x000002cc, 0x00000000 },
    { 0x000002d0, 0x00000000 },
    { 0x000002d4, 0x00000000 },
    { 0x000002d8, 0x00000000 },
    { 0x000002dc, 0x00000000 },
    { 0x000002e0, 0x00000000 },
    { 0x000002e4, 0x00000000 },
    { 0x000002e8, 0x00000000 },
    { 0x000002ec, 0x00000000 },
    { 0x000002f0, 0x00000000 },
    { 0x000002f4, 0x00000000 },
    { 0x000002f8, 0x00000000 },
    { 0x000002fc, 0x00000000 },
    { 0x00000300, 0x00000000 },
    { 0x00000304, 0x00000000 },
    { 0x00000308, 0x00000000 },
    { 0x0000030c, 0x00000000 },
    { 0x00000310, 0x00000000 },
    { 0x00000314, 0x00000000 },
    { 0x00000318, 0x00000000 },
    { 0x0000031c, 0x00000000 },
    { 0x00000320, 0x00000000 },
    { 0x00000324, 0x00000000 },
    { 0x00000328, 0x00000000 },
    { 0x0000032c, 0x00000000 },
    { 0x00000330, 0x00000000 },
    { 0x00000334, 0x00000000 },
    { 0x00000338, 0x00000000 },
    { 0x0000033c, 0x00000000 },
    { 0x00000340, 0x00000000 },
    { 0x00000344, 0x00000000 },
    { 0x00000348, 0x00000000 },
    { 0x0000034c, 0x00000000 },
    { 0x00000350, 0x00000000 },
    { 0x00000354, 0x00000000 },
    { 0x00000358, 0x00000000 },
    { 0x0000035c, 0x00000000 },
    { 0x00000360, 0x00000000 },
    { 0x00000364, 0x00000000 },
    { 0x00000368, 0x00000000 },
    { 0x0000036c, 0x00000000 },
    { 0x00000370, 0x00000000 },
    { 0x00000374, 0x00000000 },
    { 0x00000378, 0x00000000 },
    { 0x0000037c, 0x00000000 },
    { 0x00000380, 0x00000000 },
    { 0x00000384, 0x00000000 },
    { 0x00000388, 0x00000000 },
    { 0x0000038c, 0x00000000 },
    { 0x00000390, 0x00000000 },
    { 0x00000394, 0x00000000 },
    { 0x00000398, 0x00000000 },
    { 0x0000039c, 0x00000000 },
    { 0x000003a0, 0x00000000 },
    { 0x000003a4, 0x00000000 },
    { 0x000003a8, 0x00000000 },
    { 0x000003ac, 0x00000000 },
    { 0x000003b0, 0x00000000 },
    { 0x000003b4, 0x00000000 },
    { 0x000003b8, 0x00000000 },
    { 0x000003bc, 0x00000000 },
    { 0x000003c0, 0x00000000 },
    { 0x000003c4, 0x00000000 },
    { 0x000003c8, 0x00000000 },
    { 0x000003cc, 0x00000000 },
    { 0x000003d0, 0x00000000 },
    { 0x000003d4, 0x00000000 },
    { 0x000003d8, 0x00000000 },
    { 0x000003dc, 0x00000000 },
    { 0x000003e0, 0x00000000 },
    { 0x000003e4, 0x00000000 },
    { 0x000003e8, 0x00000000 },
    { 0x000003ec, 0x00000000 },
    { 0x000003f0, 0x00000000 },
    { 0x000003f4, 0x00000000 },
    { 0x000003f8, 0x00000000 },
    { 0x000003fc, 0x00000000 },
    { 0x00000400, 0x00000000 },
    { 0x00000404, 0x00000000 },
    { 0x00000408, 0x00000000 },
    { 0x0000040c, 0x00000000 },
    { 0x00000410, 0x00000000 },
    { 0x00000414, 0x00000000 },
    { 0x00000418, 0x00000000 },
    { 0x0000041c, 0x00000000 },
    { 0x00000420, 0x00000000 },
    { 0x00000424, 0x00000000 },
    { 0x00000428, 0x00000000 },
    { 0x0000042c, 0x00000000 },
    { 0x00000430, 0x00000000 },
    { 0x00000434, 0x00000000 },
    { 0x00000438, 0x00000000 },
    { 0x0000043c, 0x00000000 },
    { 0x00000440, 0x00000000 },
    { 0x00000444, 0x00000000 },
    { 0x00000448, 0x00000000 },
    { 0x0000044c, 0x00000000 },
    { 0x00000450, 0x00000000 },
    { 0x00000454, 0x00000000 },
    { 0x00000458, 0x00000000 },
    { 0x0000045c, 0x00000000 },
    { 0x00000460, 0x00000000 },
    { 0x00000464, 0x00000000 },
    { 0x00000468, 0x00000000 },
    { 0x0000046c, 0x00000000 },
    { 0x00000470, 0x00000000 },
    { 0x00000474, 0x00000000 },
    { 0x00000478, 0x00000000 },
    { 0x0000047c, 0x00000000 },
    { 0x00000480, 0x00000000 },
    { 0x00000484, 0x00000000 },
    { 0x00000488, 0x00000000 },
    { 0x0000048c, 0x00000000 },
    { 0x00000490, 0x00000000 },
    { 0x00000494, 0x00000000 },
    { 0x00000498, 0x00000000 },
    { 0x0000049c, 0x00000000 },
    { 0x000004a0, 0x00000000 },
    { 0x000004a4, 0x00000000 },
    { 0x000004a8, 0x00000000 },
    { 0x000004ac, 0x00000000 },
    { 0x000004b0, 0x00000000 },
    { 0x000004b4, 0x00000000 },
    { 0x000004b8, 0x00000000 },
    { 0x000004bc, 0x00000000 },
    { 0x000004c0, 0x00000000 },
    { 0x000004c4, 0x00000000 },
    { 0x000004c8, 0x00000000 },
    { 0x000004cc, 0x00000000 },
    { 0x000004d0, 0x00000000 },
    { 0x000004d4, 0x00000000 },
    { 0x000004d8, 0x00000000 },
    { 0x000004dc, 0x00000000 },
    { 0x000004e0, 0x00000000 },
    { 0x000004e4, 0x00000000 },
    { 0x000004e8, 0x00000000 },
    { 0x000004ec, 0x00000000 },
    { 0x000004f0, 0x00000000 },
    { 0x000004f4, 0x00000000 },
    { 0x000004f8, 0x00000000 },
    { 0x000004fc, 0x00000000 },
    { 0x00000500, 0x00000000 },
    { 0x00000504, 0x00000000 },
    { 0x00000508, 0x00000000 },
    { 0x0000050c, 0x00000000 },
    { 0x00000510, 0x00000000 },
    { 0x00000514, 0x00000000 },
    { 0x00000518, 0x00000000 },
    { 0x0000051c, 0x00000000 },
    { 0x00000520, 0x00000000 },
    { 0x00000524, 0x00000000 },
    { 0x00000528, 0x00000000 },
    { 0x0000052c, 0x00000000 },
    { 0x00000530, 0x00000000 },
    { 0x00000534, 0x00000000 },
    { 0x00000538, 0x00000000 },
    { 0x0000053c, 0x00000000 },
    { 0x00000540, 0x00000000 },
    { 0x00000544, 0x00000000 },
    { 0x00000548, 0x00000000 },
    { 0x0000054c, 0x00000000 },
    { 0x00000550, 0x00000000 },
    { 0x00000554, 0x00000000 },
    { 0x00000558, 0x00000000 },
    { 0x0000055c, 0x00000000 },
    { 0x00000560, 0x00000000 },
    { 0x00000564, 0x00000000 },
    { 0x00000568, 0x00000000 },
    { 0x0000056c, 0x00000000 },
    { 0x00000570, 0x00000000 },
    { 0x00000574, 0x00000000 },
    { 0x00000578, 0x00000000 },
    { 0x0000057c, 0x00000000 },
    { 0x00000580, 0x00000000 },
    { 0x00000584, 0x00000000 },
    { 0x00000588, 0x00000000 },
    { 0x0000058c, 0x00000000 },
    { 0x00000590, 0x00000000 },
    { 0x00000594, 0x00000000 },
    { 0x00000598, 0x00000000 },
    { 0x0000059c, 0x00000000 },
    { 0x000005a0, 0x00000000 },
    { 0x000005a4, 0x00000000 },
    { 0x000005a8, 0x00000000 },
    { 0x000005ac, 0x00000000 },
    { 0x000005b0, 0x00000000 },
    { 0x000005b4, 0x00000000 },
    { 0x000005b8, 0x00000000 },
    { 0x000005bc, 0x00000000 },
    { 0x000005c0, 0x00000000 },
    { 0x000005c4, 0x00000000 },
    { 0x000005c8, 0x00000000 },
    { 0x000005cc, 0x00000000 },
    { 0x000005d0, 0x00000000 },
    { 0x000005d4, 0x00000000 },
    { 0x000005d8, 0x00000000 },
    { 0x000005dc, 0x00000000 },
    { 0x000005e0, 0x00000000 },
    { 0x000005e4, 0x00000000 },
    { 0x000005e8, 0x00000000 },
    { 0x000005ec, 0x00000000 },
    { 0x000005f0, 0x00000000 },
    { 0x000005f4, 0x00000000 },
    { 0x000005f8, 0x00000000 },
    { 0x000005fc, 0x00000000 },
    { 0x00000600, 0x00000000 },
    { 0x00000604, 0x00000000 },
    { 0x00000608, 0x00000000 },
    { 0x0000060c, 0x00000000 },
    { 0x00000610, 0x00000000 },
    { 0x00000614, 0x00000000 },
    { 0x00000618, 0x00000000 },
    { 0x0000061c, 0x00000000 },
    { 0x00000620, 0x00000000 },
    { 0x00000624, 0x00000000 },
    { 0x00000628, 0x00000000 },
    { 0x0000062c, 0x00000000 },
    { 0x00000630, 0x00000000 },
    { 0x00000634, 0x00000000 },
    { 0x00000638, 0x00000000 },
    { 0x0000063c, 0x00000000 },
    { 0x00000640, 0x00000000 },
    { 0x00000644, 0x00000000 },
    { 0x00000648, 0x00000000 },
    { 0x0000064c, 0x00000000 },
    { 0x00000650, 0x00000000 },
    { 0x00000654, 0x00000000 },
    { 0x00000658, 0x00000000 },
    { 0x0000065c, 0x00000000 },
    { 0x00000660, 0x00000000 },
    { 0x00000664, 0x00000000 },
    { 0x00000668, 0x00000000 },
    { 0x0000066c, 0x00000000 },
    { 0x00000670, 0x00000000 },
    { 0x00000674, 0x00000000 },
    { 0x00000678, 0x00000000 },
    { 0x0000067c, 0x00000000 },
    { 0x00000680, 0x00000000 },
    { 0x00000684, 0x00000000 },
    { 0x00000688, 0x00000000 },
    { 0x0000068c, 0x00000000 },
    { 0x00000690, 0x00000000 },
    { 0x00000694, 0x00000000 },
    { 0x00000698, 0x00000000 },
    { 0x0000069c, 0x00000000 },
    { 0x000006a0, 0x00000000 },
    { 0x000006a4, 0x00000000 },
    { 0x000006a8, 0x00000000 },
    { 0x000006ac, 0x00000000 },
    { 0x000006b0, 0x00000000 },
    { 0x000006b4, 0x00000000 },
    { 0x000006b8, 0x00000000 },
    { 0x000006bc, 0x00000000 },
    { 0x000006c0, 0x00000000 },
    { 0x000006c4, 0x00000000 },
    { 0x000006c8, 0x00000000 },
    { 0x000006cc, 0x00000000 },
    { 0x000006d0, 0x00000000 },
    { 0x000006d4, 0x00000000 },
    { 0x000006d8, 0x00000000 },
    { 0x000006dc, 0x00000000 },
    { 0x000006e0, 0x00000000 },
    { 0x000006e4, 0x00000000 },
    { 0x000006e8, 0x00000000 },
    { 0x000006ec, 0x00000000 },
    { 0x000006f0, 0x00000000 },
    { 0x000006f4, 0x00000000 },
    { 0x000006f8, 0x00000000 },
    { 0x000006fc, 0x00000000 },
    { 0x00000700, 0x00000000 },
    { 0x00000704, 0x00000000 },
    { 0x00000708, 0x00000000 },
    { 0x0000070c, 0x00000000 },
    { 0x00000710, 0x00000000 },
    { 0x00000714, 0x00000000 },
    { 0x00000718, 0x00000000 },
    { 0x0000071c, 0x00000000 },
    { 0x00000720, 0x00000000 },
    { 0x00000724, 0x00000000 },
    { 0x00000728, 0x00000000 },
    { 0x0000072c, 0x00000000 },
    { 0x00000730, 0x00000000 },
    { 0x00000734, 0x00000000 },
    { 0x00000738, 0x00000000 },
    { 0x0000073c, 0x00000000 },
    { 0x00000740, 0x00000000 },
    { 0x00000744, 0x00000000 },
    { 0x00000748, 0x00000000 },
    { 0x0000074c, 0x00000000 },
    { 0x00000750, 0x00000000 },
    { 0x00000754, 0x00000000 },
    { 0x00000758, 0x00000000 },
    { 0x0000075c, 0x00000000 },
    { 0x00000760, 0x00000000 },
    { 0x00000764, 0x00000000 },
    { 0x00000768, 0x00000000 },
    { 0x0000076c, 0x00000000 },
    { 0x00000770, 0x00000000 },
    { 0x00000774, 0x00000000 },
    { 0x00000778, 0x00000000 },
    { 0x0000077c, 0x00000000 },
    { 0x00000780, 0x00000000 },
    { 0x00000784, 0x00000000 },
    { 0x00000788, 0x00000000 },
    { 0x0000078c, 0x00000000 },
    { 0x00000790, 0x00000000 },
    { 0x00000794, 0x00000000 },
    { 0x00000798, 0x00000000 },
    { 0x0000079c, 0x00000000 },
    { 0x000007a0, 0x00000000 },
    { 0x000007a4, 0x00000000 },
    { 0x000007a8, 0x00000000 },
    { 0x000007ac, 0x00000000 },
    { 0x000007b0, 0x00000000 },
    { 0x000007b4, 0x00000000 },
    { 0x000007b8, 0x00000000 },
    { 0x000007bc, 0x00000000 },
    { 0x000007c0, 0x00000000 },
    { 0x000007c4, 0x00000000 },
    { 0x000007c8, 0x00000000 },
    { 0x000007cc, 0x00000000 },
    { 0x000007d0, 0x00000000 },
    { 0x000007d4, 0x00000000 },
    { 0x000007d8, 0x00000000 },
    { 0x000007dc, 0x00000000 },
    { 0x000007e0, 0x00000000 },
    { 0x000007e4, 0x00000000 },
    { 0x000007e8, 0x00000000 },
    { 0x000007ec, 0x00000000 },
    { 0x000007f0, 0x00000000 },
    { 0x000007f4, 0x00000000 },
    { 0x000007f8, 0x00000000 },
    { 0x000007fc, 0x00000000 },
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

    /* Load embedded data (replaces devmem load scripts) */
    { /* Data: base=0xc0080000, 512 writes */
        uint8_t* dpage = (uint8_t*)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0080000);
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

    { /* Data: base=0xc0000000, 512 writes */
        uint8_t* dpage = (uint8_t*)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xc0000000);
        if (dpage != MAP_FAILED) {
            const uint32_t (*dp)[2] = embed_1_data;
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

    /* No CRC checks found in .cfg */
    printf("No output verification available\n");

    printf("Results: %d failures\n", failures);
    munmap(dla_mmio, NVDLA_MMIO_SIZE);
    close(fd);
    return failures;
}