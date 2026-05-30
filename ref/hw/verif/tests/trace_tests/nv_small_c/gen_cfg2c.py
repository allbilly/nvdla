#!/usr/bin/env python3
"""
Convert UVM trace test (.cfg + .dat) to VP test:
  - C source with register writes (addresses relocated to VP DRAM range)
  - devmem shell scripts for loading data/weights

Usage:
  python3 gen_cfg2c.py path/to/test.cfg
  Output: path/to/test_test.c, path/to/*_load.sh, path/to/*_wt_load.sh
"""
import os
import sys
import glob
import re
import struct

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# VP DRAM range (from conf/aarch64_nvdla.lua)
VP_DRAM_BASE = 0xC0000000
VP_DRAM_SIZE = 0x40000000  # 1 GB

def relocate_uvm_addr(addr):
    """Relocate UVM DRAM address to VP DRAM range.
    UVM DRAM:  0x80000000-0x8FFFFFFF, 0x90000000-0x9FFFFFFF, 0xA0000000-0xAFFFFFFF
    VP  DRAM:  0xC0000000-0xCFFFFFFF, 0xD0000000-0xDFFFFFFF, 0xF0000000-0xFFFFFFFF
    """
    region = (addr >> 24) & 0xFF
    if region == 0x80:
        return addr - 0x80000000 + 0xC0000000
    elif region == 0x90:
        return addr - 0x90000000 + 0xD0000000
    elif region == 0xA0:
        return addr - 0xA0000000 + 0xF0000000
    return addr  # not in DRAM range, keep as-is

def parse_dat_file(dat_path):
    """Parse UVM trace payload format:
    {offset:0x0, size:4, payload:0xXX 0xXX 0xXX 0xXX}
    Returns list of (offset, bytes_list)
    """
    with open(dat_path, 'rb') as f:
        text = f.read().decode('utf-8', errors='replace')
    entries = []
    for m in re.finditer(
        r'\{offset:(0x[0-9a-fA-F]+),\s*size:(\d+),\s*payload:((?:0x[0-9a-fA-F]+\s*)+)\}',
        text
    ):
        offset = int(m.group(1), 16)
        size = int(m.group(2))
        payload_str = m.group(3).strip()
        payload_bytes = [int(b, 16) for b in payload_str.replace('0x', '').split()]
        entries.append((offset, payload_bytes[:size]))
    return entries

def generate_devmem_script(entries, uvm_load_addr, output_path):
    """Generate devmem shell script from parsed entries.
    Each entry is written at relocate(uvm_load_addr + entry_offset + byte_index).
    """
    lines = ['#!/bin/sh', '']
    for offset, payload_bytes in entries:
        for i in range(0, len(payload_bytes), 4):
            chunk = payload_bytes[i:i+4]
            while len(chunk) < 4:
                chunk.append(0)
            uvm_addr = uvm_load_addr + offset + i
            vp_addr = relocate_uvm_addr(uvm_addr)
            val = struct.unpack('<I', bytes(chunk))[0]
            lines.append(f'devmem {hex(vp_addr)} 32 {hex(val)}')
    with open(output_path, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    os.chmod(output_path, 0o755)
    print(f"  Generated {os.path.basename(output_path)}")

def cfg_name_to_macro(name):
    return name.replace('.', '_')

def generate_c_source(cfg_path):
    test_name = os.path.splitext(os.path.basename(cfg_path))[0]
    test_dir = os.path.dirname(cfg_path)

    # Parse .cfg
    with open(cfg_path) as f:
        cfg_text = f.read()
    cfg_lines = cfg_text.split('\n')

    # Extract mem_load / mem_init addresses
    data_addr_uvm = None
    weight_addr_uvm = None
    out_addr_uvm = None
    # Track ALL mem_load entries for load script generation
    mem_load_entries = []
    # Track regions that need zero-initialization (mem_init without following mem_load)
    zero_init_regions = []
    for i, line in enumerate(cfg_lines):
        m = re.match(r'mem_load\(pri_mem,\s*(0x[0-9a-fA-F]+).*?_dt\.dat', line)
        if m:
            data_addr_uvm = int(m.group(1), 16)
            mem_load_entries.append((data_addr_uvm, f'{test_name}_dt.dat'))
        m = re.match(r'mem_load\(pri_mem,\s*(0x[0-9a-fA-F]+).*?_wt\.dat', line)
        if m:
            weight_addr_uvm = int(m.group(1), 16)
            mem_load_entries.append((weight_addr_uvm, f'{test_name}_wt.dat'))
        m = re.match(r'mem_load\(pri_mem,\s*(0x[0-9a-fA-F]+).*?_in_wt\.dat', line)
        if m:
            weight_addr_uvm = int(m.group(1), 16)
            mem_load_entries.append((weight_addr_uvm, f'{test_name}_in_wt.dat'))
        m = re.match(r'mem_load\(pri_mem,\s*(0x[0-9a-fA-F]+).*?_in_feature\.dat', line)
        if m:
            data_addr_uvm = int(m.group(1), 16)
            mem_load_entries.append((data_addr_uvm, f'{test_name}_in_feature.dat'))
        m = re.match(r'mem_load\(pri_mem,\s*(0x[0-9a-fA-F]+).*?_in\.dat', line)
        if m:
            data_addr_uvm = int(m.group(1), 16)
            mem_load_entries.append((data_addr_uvm, f'{test_name}_in.dat'))
        m = re.match(r'mem_load\(pri_mem,\s*(0x[0-9a-fA-F]+).*?_output\.dat', line)
        if m:
            addr = int(m.group(1), 16)
            if data_addr_uvm is None:
                data_addr_uvm = addr
            fname_out = os.path.basename(re.search(r'"[^"]+\.dat"', line).group(0).strip('"'))
            mem_load_entries.append((addr, fname_out))
        # Generic .dat load
        m = re.match(r'mem_load\(pri_mem,\s*(0x[0-9a-fA-F]+),\s*"([^"]+\.dat)"\)', line)
        if m and '_wt.dat' not in line and '_dt.dat' not in line and '_in.dat' not in line and '_in_feature.dat' not in line and '_in_wt.dat' not in line and '_output.dat' not in line:
            addr = int(m.group(1), 16)
            fname = m.group(2)
            if data_addr_uvm is None:
                data_addr_uvm = addr
            mem_load_entries.append((addr, fname))
        m = re.match(r'mem_init\(pri_mem,\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),\s*ALL_ZERO\)', line)
        if m:
            addr = int(m.group(1), 16)
            size = int(m.group(2), 16)
            # Always zero-init ALL mem_init regions (including those followed by mem_load),
            # because VP memory starts with garbage and .dat files only contain non-zero chunks
            zero_init_regions.append((addr, size))
            if out_addr_uvm is None:
                out_addr_uvm = addr

    def _hex_or_none(v):
        return f"0x{v:08x}" if v is not None else "None"
    print(f"UVM addresses: data={_hex_or_none(data_addr_uvm)}, wt={_hex_or_none(weight_addr_uvm)}, out={_hex_or_none(out_addr_uvm)}")

    # Generate embedded data blocks from .dat files
    embedded_blocks = []
    for idx, (load_addr, fname) in enumerate(mem_load_entries):
        dat_path = os.path.join(test_dir, fname)
        if not os.path.exists(dat_path):
            print(f"  Warning: {dat_path} not found, skipping embedded data", file=sys.stderr)
            continue
        entries = parse_dat_file(dat_path)
        vp_load_addr = relocate_uvm_addr(load_addr)
        data_pairs = []
        for offset, payload_bytes in entries:
            for i in range(0, len(payload_bytes), 4):
                chunk = payload_bytes[i:i+4]
                while len(chunk) < 4:
                    chunk.append(0)
                full_offset = offset + i
                val = struct.unpack('<I', bytes(chunk))[0]
                data_pairs.append((full_offset, val))
        if not data_pairs:
            continue
        var_name = f'embed_{idx}'
        embedded_blocks.append((var_name, vp_load_addr, data_pairs))
        print(f"  Embedded data: {var_name}, {len(data_pairs)} words at 0x{vp_load_addr:08x} ({fname})")

    # Parse register writes, tracking phase (before/after CBUF flush poll)
    reg_writes = []
    saw_cbuf_poll = False
    for line in cfg_lines:
        ls = line.strip()
        if re.match(r'poll_reg_equal\(NVDLA_CDMA\.S_CBUF_FLUSH_STATUS_0', ls):
            saw_cbuf_poll = True
            continue
        m = re.match(r'reg_write\((\S+),\s*(0x[0-9a-fA-F]+)\)', ls)
        if m:
            name = cfg_name_to_macro(m.group(1))
            val = int(m.group(2), 16)
            reg_writes.append((name, val, saw_cbuf_poll))

    # Parse check_crc
    check_crcs = []
    for line in cfg_lines:
        m = re.match(r'check_crc\(.*?,\s*(\d+),\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\)', line)
        if m:
            crc_count = int(m.group(1))
            crc_addr = int(m.group(2), 16)
            crc_size = int(m.group(3), 16)
            crc_expected = int(m.group(4), 16)
            check_crcs.append((crc_addr, crc_size, crc_expected, crc_count))
            print(f"  CRC check: addr=0x{crc_addr:08x}, size=0x{crc_size:x}, expect=0x{crc_expected:08x}")

    # Relocate address values in register writes
    # Address registers that may contain DRAM addresses
    _addr_re = re.compile(r'(?:BASE_)?ADDR_(?:LOW|HIGH)|DATAOUT_ADDR')

    # Build register offset map from header files
    reg_map = {}
    for hfile in glob.glob(os.path.join(SCRIPT_DIR, "../../../../ref/hw/outdir/nv_small/spec/manual/NVDLA_*.h")):
        with open(hfile) as f:
            for line in f:
                m = re.match(r'#define\s+(NVDLA_\S+_0)\s+.*?_MK_ADDR_CONST\((0x[0-9a-fA-F]+)\)', line)
                if m:
                    reg_map[m.group(1)] = int(m.group(2), 16)

    # Separate initial vs enable regs
    all_reg_writes = []
    enable_reg_writes = []

    for name, val, is_after_poll in reg_writes:
        old_val = val
        if _addr_re.search(name):
            new_val = relocate_uvm_addr(val)
            if new_val != val:
                print(f"  Relocated {name}: 0x{old_val:08x} -> 0x{new_val:08x}")
                val = new_val
        offset = reg_map.get(name, 0)
        if not offset:
            print(f"  Warning: unknown register {name}", file=sys.stderr)
        if is_after_poll:
            enable_reg_writes.append((offset, val, name))
        else:
            all_reg_writes.append((offset, val, name))

    # Relocate CRC output addresses
    crc_infos = []
    for crc_addr, crc_size, crc_expected, crc_count in check_crcs:
        vp_addr = relocate_uvm_addr(crc_addr)
        crc_infos.append((vp_addr, crc_size, crc_expected, crc_count))

    # Generate C file
    lines = []
    lines.append('#include <sys/mman.h>')
    lines.append('#include <unistd.h>')
    lines.append('#include <stdio.h>')
    lines.append('#include <stdlib.h>')
    lines.append('#include <stdint.h>')
    lines.append('#include <fcntl.h>')
    lines.append('#include <errno.h>')
    lines.append('#include <string.h>')
    lines.append('')
    lines.append('static const uint32_t crc32_tab[256] = {')
    lines.append('    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,')
    lines.append('    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,')
    lines.append('    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,')
    lines.append('    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,')
    lines.append('    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,')
    lines.append('    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,')
    lines.append('    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,')
    lines.append('    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,')
    lines.append('    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,')
    lines.append('    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,')
    lines.append('    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,')
    lines.append('    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,')
    lines.append('    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,')
    lines.append('    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,')
    lines.append('    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,')
    lines.append('    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,')
    lines.append('    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,')
    lines.append('    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,')
    lines.append('    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,')
    lines.append('    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,')
    lines.append('    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,')
    lines.append('    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,')
    lines.append('    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,')
    lines.append('    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,')
    lines.append('    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,')
    lines.append('    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,')
    lines.append('    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,')
    lines.append('    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,')
    lines.append('    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,')
    lines.append('    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,')
    lines.append('    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,')
    lines.append('    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,')
    lines.append('};')
    lines.append('')
    lines.append('static uint32_t calc_crc32(const void* buf, size_t size) {')
    lines.append('    const uint8_t* p = (const uint8_t*)buf;')
    lines.append('    uint32_t crc = 0xffffffff;')
    lines.append('    for (size_t i = 0; i < size; i++)')
    lines.append('        crc = crc32_tab[(crc ^ p[i]) & 0xff] ^ (crc >> 8);')
    lines.append('    return crc ^ 0xffffffff;')
    lines.append('}')
    lines.append('')
    lines.append('struct dla_reg { int32_t offset; uint32_t value; };')
    lines.append('')
    lines.append('static const struct dla_reg program_initial[] = {')
    for offset, val, name in all_reg_writes:
        lines.append(f'    {{ 0x{offset:04x}, 0x{val:08x} }}, /* {name} */')
    lines.append('    {-1, 0}')
    lines.append('};')
    lines.append('')
    lines.append('static const struct dla_reg program_enable[] = {')
    for offset, val, name in enable_reg_writes:
        lines.append(f'    {{ 0x{offset:04x}, 0x{val:08x} }}, /* {name} */')
    lines.append('    {-1, 0}')
    lines.append('};')
    lines.append('')

    # Add embedded data arrays
    if embedded_blocks:
        lines.append('/* Embedded data blocks (loaded via mmap instead of devmem scripts) */')
        for var_name, vp_base, data_pairs in embedded_blocks:
            lines.append(f'static const uint32_t {var_name}_data[][2] = {{')
            for off, val in data_pairs:
                lines.append(f'    {{ 0x{off:08x}, 0x{val:08x} }},')
            lines.append(f'    {{ 0xFFFFFFFF, 0 }}')
            lines.append('};')
            lines.append('')

    lines.append('#define NVDLA_MMIO_BASE  0x10200000')
    lines.append('#define NVDLA_MMIO_SIZE (0x10220000 - 0x10200000)')
    lines.append('#define INTR_STATUS    0x100c')
    lines.append('#define SDP_DONE_SHIFT 0')
    lines.append('#define CDMA_DONE_SHIFT 2')
    lines.append('#define CSC_DONE_SHIFT  3')
    lines.append('#define CMAC_A_DONE_SHIFT 4')
    lines.append('#define CMAC_B_DONE_SHIFT 5')
    lines.append('#define CACC_DONE_SHIFT 6')
    lines.append('#define BDMA_DONE_SHIFT 7')
    lines.append('#define PDP_DONE_SHIFT  8')
    lines.append('#define CDP_DONE_SHIFT  9')
    lines.append('#define RUBIK_DONE_SHIFT 10')
    lines.append('')
    lines.append('int main(int argc, char* argv[])')
    lines.append('{')
    lines.append('    int failures = 0;')
    lines.append('    int loop;')
    lines.append('    int fd = open("/dev/mem", O_RDWR);')
    lines.append('    if (fd < 0) { perror("open /dev/mem"); return 1; }')
    lines.append('    uint8_t* dla_mmio = (uint8_t*)mmap(NULL, NVDLA_MMIO_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, NVDLA_MMIO_BASE);')
    lines.append('    if (dla_mmio == MAP_FAILED) { perror("mmap dla"); close(fd); return 1; }')
    lines.append('')

    # Zero-init ALL mem_init regions FIRST (VP memory starts with garbage)
    for zaddr, zsize in zero_init_regions:
        vp_zaddr = relocate_uvm_addr(zaddr)
        zpage_base = vp_zaddr & ~0xFFF
        zpage_off = vp_zaddr & 0xFFF
        zraw_size = zpage_off + zsize
        zmap_size = ((zraw_size + 4095) // 4096) * 4096
        lines.append(f'    {{ /* Zero-init region 0x{vp_zaddr:08x} size 0x{zsize:x} */')
        lines.append(f'        uint8_t* zpage = (uint8_t*)mmap(NULL, {zmap_size}, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x{zpage_base:08x});')
        lines.append(f'        if (zpage != MAP_FAILED) {{ memset(zpage + {zpage_off}, 0, {zsize}); munmap(zpage, {zmap_size}); }}')
        lines.append(f'    }}')
    if zero_init_regions:
        lines.append('')

    # Embedded data loading (loads non-zero data on top of zero-init regions)
    if embedded_blocks:
        lines.append('    /* Load embedded data (replaces devmem load scripts) */')
        for var_name, vp_base, data_pairs in embedded_blocks:
            if not data_pairs:
                continue
            base_page = vp_base & ~0xFFF
            max_off = max(off for off, _ in data_pairs) + 4
            map_size = (((vp_base & 0xFFF) + max_off + 4095) // 4096) * 4096
            lines.append(f'    {{ /* Data: base=0x{vp_base:08x}, {len(data_pairs)} writes */')
            lines.append(f'        uint8_t* dpage = (uint8_t*)mmap(NULL, {map_size}, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x{base_page:08x});')
            lines.append(f'        if (dpage != MAP_FAILED) {{')
            lines.append(f'            const uint32_t (*dp)[2] = {var_name}_data;')
            lines.append(f'            uint32_t base_off = 0x{vp_base & 0xFFF:x};')
            lines.append(f'            while ((*dp)[0] != 0xFFFFFFFF) {{')
            lines.append(f'                *(volatile uint32_t*)(dpage + base_off + (*dp)[0]) = (*dp)[1];')
            lines.append(f'                dp++;')
            lines.append(f'            }}')
            lines.append(f'            munmap(dpage, {map_size});')
            lines.append(f'        }} else {{ perror("mmap embed"); }}')
            lines.append(f'    }}')
            lines.append('')

    lines.append('    printf("Programming initial registers...\\n");')
    lines.append('    const struct dla_reg* p = program_initial;')
    lines.append('    while (p->offset != -1) {')
    lines.append('        *(volatile uint32_t*)(dla_mmio + p->offset) = p->value;')
    lines.append('        p++;')
    lines.append('    }')
    lines.append('    printf("Done initial registers.\\n");')

    # Check for CBUF flush poll
    if any(':NVDLA_CDMA_S_CBUF_FLUSH_STATUS_0' in name for _, _, name in all_reg_writes) or \
       any('CBUF' in name for _, _, name in all_reg_writes + enable_reg_writes):
        # Find CDMA CBUF flush status offset
        cbuf_offset = reg_map.get('NVDLA_CDMA_S_CBUF_FLUSH_STATUS_0', 0x300c)
        lines.append('')
        lines.append('    printf("Waiting for CBUF flush...\\n");')
        lines.append(f'    volatile uint32_t* cbuf_status = (volatile uint32_t*)(dla_mmio + 0x{cbuf_offset:04x});')
        lines.append('    loop = 1000000;')
        lines.append('    while (--loop > 0) {')
        lines.append('        if (*cbuf_status & 0x1) { printf("CBUF flush done\\n"); break; }')
        lines.append('    }')
        lines.append('    if (loop <= 0) printf("CBUF flush timeout\\n");')
    else:
        # Still wait briefly for CBUF
        pass

    # Zero-init CRC output regions to prevent false positives from pre-loaded golden data
    if crc_infos:
        lines.append('    /* Zero-init CRC output regions to prevent false positives */')
        for idx, (vp_addr, crc_size, crc_expected, crc_count) in enumerate(crc_infos):
            zpage_base = vp_addr & ~0xFFF
            zpage_off = vp_addr & 0xFFF
            zraw_size = zpage_off + crc_size
            zmap_size = ((zraw_size + 4095) // 4096) * 4096
            lines.append(f'    {{ /* CRC[{idx}] zero-init: addr=0x{vp_addr:08x} size=0x{crc_size:x} */')
            lines.append(f'        uint8_t* zpage = (uint8_t*)mmap(NULL, {zmap_size}, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x{zpage_base:08x});')
            lines.append(f'        if (zpage != MAP_FAILED) {{ memset(zpage + {zpage_off}, 0, {crc_size}); munmap(zpage, {zmap_size}); }}')
            lines.append(f'    }}')
        lines.append('')

    lines.append('    printf("Firing OP_ENABLE...\\n");')
    lines.append('    p = program_enable;')
    lines.append('    while (p->offset != -1) {')
    lines.append('        *(volatile uint32_t*)(dla_mmio + p->offset) = p->value;')
    lines.append('        p++;')
    lines.append('    }')
    lines.append('')
    lines.append('    printf("Waiting for done interrupt...\\n");')
    lines.append('    volatile uint32_t* intr = (volatile uint32_t*)(dla_mmio + INTR_STATUS);')
    lines.append('    loop = 100000000;')
    lines.append('    uint32_t done_mask = 0;')
    lines.append('    int extra_wait = 0;')
    lines.append('    while (--loop > 0) {')
    lines.append('        uint32_t s = *intr;')
    lines.append('        uint32_t new_bits = s & ~done_mask;')
    lines.append('        done_mask |= s;')
    lines.append('        if (new_bits & (1 << SDP_DONE_SHIFT)) { printf("SDP_DONE "); fflush(stdout); extra_wait = 10000; }')
    lines.append('        if (new_bits & (1 << CDMA_DONE_SHIFT)) { printf("CDMA_DONE "); fflush(stdout); extra_wait = 10000; }')
    lines.append('        if (new_bits & (1 << CSC_DONE_SHIFT)) { printf("CSC_DONE "); fflush(stdout); extra_wait = 10000; }')
    lines.append('        if (new_bits & (1 << CMAC_A_DONE_SHIFT)) { printf("CMAC_A_DONE "); fflush(stdout); extra_wait = 10000; }')
    lines.append('        if (new_bits & (1 << CMAC_B_DONE_SHIFT)) { printf("CMAC_B_DONE "); fflush(stdout); extra_wait = 10000; }')
    lines.append('        if (new_bits & (1 << CACC_DONE_SHIFT)) { printf("CACC_DONE "); fflush(stdout); extra_wait = 10000; }')
    lines.append('        if (new_bits & (1 << BDMA_DONE_SHIFT)) { printf("BDMA_DONE "); fflush(stdout); extra_wait = 10000; }')
    lines.append('        if (new_bits & (1 << PDP_DONE_SHIFT)) { printf("PDP_DONE "); fflush(stdout); extra_wait = 10000; }')
    lines.append('        if (new_bits & (1 << CDP_DONE_SHIFT)) { printf("CDP_DONE "); fflush(stdout); extra_wait = 10000; }')
    lines.append('        if (new_bits & (1 << RUBIK_DONE_SHIFT)) { printf("RUBIK_DONE "); fflush(stdout); extra_wait = 10000; }')
    lines.append('        if (extra_wait > 0 && --extra_wait <= 0) break;')
    lines.append('    }')
    lines.append('    printf("\\n");')
    lines.append('    if (loop <= 0) { printf("TIMEOUT\\n"); munmap(dla_mmio, NVDLA_MMIO_SIZE); close(fd); return 1; }')
    lines.append('')

    # Output CRC checks
    if crc_infos:
        lines.append('    /* Read output and verify CRC */')
        for idx, (vp_addr, crc_size, crc_expected, crc_count) in enumerate(crc_infos):
            page_base = vp_addr & ~0xFFF
            page_off = vp_addr & 0xFFF
            raw_size = page_off + crc_size
            map_size = ((raw_size + 4095) // 4096) * 4096  # round up to page boundary
            lines.append(f'    {{ /* CRC[{idx}]: addr=0x{vp_addr:08x}, size=0x{crc_size:x}, expect=0x{crc_expected:08x} */')
            lines.append(f'        uint8_t* page = (uint8_t*)mmap(NULL, {map_size}, PROT_READ, MAP_SHARED, fd, 0x{page_base:08x});')
            lines.append(f'        if (page != MAP_FAILED) {{')
            lines.append(f'            uint32_t crc = calc_crc32(page + {page_off}, {crc_size});')
            lines.append(f'            printf("CRC[{idx}] = 0x%08x (expect 0x{crc_expected:08x}) %s\\n", crc, crc == 0x{crc_expected:08x} ? "PASS" : "FAIL");')
            lines.append(f'            if (crc != 0x{crc_expected:08x}) failures++;')
            lines.append(f'            munmap(page, {map_size});')
            lines.append(f'        }} else {{ perror("mmap out"); }}')
            lines.append(f'    }}')
            lines.append('')
    else:
        lines.append('    /* No CRC checks found in .cfg */')
        lines.append('    printf("No output verification available\\n");')
        lines.append('')

    lines.append('    printf("Results: %d failures\\n", failures);')
    lines.append('    munmap(dla_mmio, NVDLA_MMIO_SIZE);')
    lines.append('    close(fd);')
    lines.append('    return failures;')
    lines.append('}')



    c_path = os.path.join(os.getcwd(), f'{test_name}_test.c')
    with open(c_path, 'w') as f:
        f.write('\n'.join(lines))
    print(f"Generated {c_path}")

    return data_addr_uvm, weight_addr_uvm, out_addr_uvm, mem_load_entries


def _load_script_suffix(fname, test_name):
    """Derive a load script suffix from the .dat filename and test name."""
    base = os.path.splitext(fname)[0]
    if base.startswith(test_name):
        suffix = base[len(test_name):]  # e.g., "_bn" from "test_bn.dat"
    else:
        suffix = '_' + base  # e.g., "_CDP_0_output" for unusual names
    # Normalize known aliases
    if suffix in ('', '_dt', '_in', '_in_feature'):
        return '_dt'
    if suffix == '_in_wt':
        return '_wt'
    return suffix


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: gen_cfg2c.py <test.cfg>")
        sys.exit(1)
    cfg_path = sys.argv[1]
    test_dir = os.path.dirname(cfg_path)
    test_name = os.path.splitext(os.path.basename(cfg_path))[0]

    data_addr_uvm, weight_addr_uvm, out_addr_uvm, mem_load_entries = generate_c_source(cfg_path)

    outdir = os.getcwd()

    # Generate load scripts from ALL mem_load entries
    for load_addr, fname in mem_load_entries:
        dat_path = os.path.join(test_dir, fname)
        if not os.path.exists(dat_path):
            print(f"  Warning: {dat_path} not found, skipping", file=sys.stderr)
            continue
        entries = parse_dat_file(dat_path)
        suffix = _load_script_suffix(fname, test_name)
        output_name = f'{test_name}{suffix}_load.sh'
        generate_devmem_script(entries, load_addr, os.path.join(outdir, output_name))
