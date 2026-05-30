#!/usr/bin/env python3
"""
Convert UVM trace test (.cfg + .dat) to VP test:
  - C source with register writes (addresses relocated to VP DRAM range)
  - devmem shell scripts for loading data/weights
"""
import os
import sys
import glob
import re
import struct

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REG_HEADER_DIR = os.path.join(SCRIPT_DIR,
    "../../../../ref/hw/outdir/nv_small/spec/manual")

# VP DRAM range (from conf/aarch64_nvdla.lua)
# Must relocate UVM test addresses (0x8xxx/0x9xxx/0xAxxx) into 0xCxxxxxxx-0xFxxxxxxx
VP_DATA_BASE   = 0xc0000000
VP_WEIGHT_BASE = 0xd0000000
VP_OUT_BASE    = 0xf0000000

# Build register name -> offset map
reg_map = {}
for hfile in glob.glob(os.path.join(REG_HEADER_DIR, "NVDLA_*.h")):
    with open(hfile) as f:
        for line in f:
            m = re.match(r'#define\s+(NVDLA_\S+_0)\s+.*?_MK_ADDR_CONST\((0x[0-9a-fA-F]+)\)', line)
            if m:
                reg_map[m.group(1)] = int(m.group(2), 16)

def cfg_name_to_macro(name):
    return name.replace('.', '_')

def parse_dat_file(dat_path):
    """Parse UVM trace payload format:
    {
    {offset:0x0, size:4, payload:0xXX 0xXX 0xXX 0xXX} ,
    }
    Returns list of (offset, bytes_list)
    """
    with open(dat_path, 'rb') as f:
        text = f.read().decode('utf-8', errors='replace')
    entries = []
    for m in re.finditer(r'\{offset:(0x[0-9a-fA-F]+),\s*size:(\d+),\s*payload:((?:0x[0-9a-fA-F]+\s*)+)\}', text):
        offset = int(m.group(1), 16)
        size = int(m.group(2))
        payload_str = m.group(3).strip()
        payload_bytes = [int(b, 16) for b in payload_str.replace('0x', '').split()]
        entries.append((offset, payload_bytes))
    return entries

def generate_devmem_script(entries, base_addr, sh_path):
    """Generate devmem shell script from parsed entries."""
    lines = ['#!/bin/sh', '']
    for offset, payload_bytes in entries:
        # Group bytes into 32-bit little-endian words
        for i in range(0, len(payload_bytes), 4):
            chunk = payload_bytes[i:i+4]
            while len(chunk) < 4:
                chunk.append(0)
            addr = base_addr + offset + i
            val = struct.unpack('<I', bytes(chunk))[0]  # LE 32-bit
            lines.append(f'devmem {hex(addr)} 32 {hex(val)}')
    with open(sh_path, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    os.chmod(sh_path, 0o755)
    print(f"Generated {sh_path}")

def generate_c_source(cfg_path):
    test_name = os.path.splitext(os.path.basename(cfg_path))[0]
    test_dir = os.path.dirname(cfg_path)

    # Parse .cfg
    with open(cfg_path) as f:
        cfg_lines = f.readlines()

    # Extract address info
    data_addr_uvm = None
    weight_addr_uvm = None
    out_addr_uvm = None
    for i, line in enumerate(cfg_lines):
        m = re.match(r'mem_load\(pri_mem,\s*(0x[0-9a-fA-F]+).*?_dt\.dat', line)
        if m:
            data_addr_uvm = int(m.group(1), 16)
        m = re.match(r'mem_load\(pri_mem,\s*(0x[0-9a-fA-F]+).*?_wt\.dat', line)
        if m:
            weight_addr_uvm = int(m.group(1), 16)
        m = re.match(r'mem_init\(pri_mem,\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),\s*ALL_ZERO\)', line)
        if m:
            addr = int(m.group(1), 16)
            if i + 1 < len(cfg_lines):
                nm = re.match(r'mem_load', cfg_lines[i+1])
                if not nm:
                    out_addr_uvm = addr
            else:
                out_addr_uvm = addr

    print(f"UVM addresses: data=0x{data_addr_uvm:08x}, wt=0x{weight_addr_uvm:08x}, out=0x{out_addr_uvm:08x}")

    # Parse register writes, tracking phase (before/after CBUF flush poll)
    # TODO: explain saw_cbuf_poll
    reg_writes = []  # (name, val, is_after_poll)
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

    # Relocate addresses:
    # Find registers that reference original UVM addresses and update them
    addr_fixups = {}
    if data_addr_uvm is not None:
        addr_fixups[data_addr_uvm] = VP_DATA_BASE
    if weight_addr_uvm is not None:
        addr_fixups[weight_addr_uvm] = VP_WEIGHT_BASE
    if out_addr_uvm is not None:
        addr_fixups[out_addr_uvm] = VP_OUT_BASE

    # CDMA and SDP registers that contain address values
    # TODO: 64bit unit rk3588?
    addr_reg_names = [
        'NVDLA_CDMA_D_DAIN_ADDR_LOW_0_0',
        'NVDLA_CDMA_D_DAIN_ADDR_HIGH_0_0',
        'NVDLA_CDMA_D_WEIGHT_ADDR_LOW_0',
        'NVDLA_CDMA_D_WEIGHT_ADDR_HIGH_0',
        'NVDLA_CDMA_D_WMB_ADDR_LOW_0',
        'NVDLA_CDMA_D_WMB_ADDR_HIGH_0',
        'NVDLA_CDMA_D_WGS_ADDR_LOW_0',
        'NVDLA_CDMA_D_WGS_ADDR_HIGH_0',
        'NVDLA_CDMA_D_DAIN_ADDR_LOW_1_0',
        'NVDLA_CDMA_D_DAIN_ADDR_HIGH_1_0',
        'NVDLA_SDP_D_DST_BASE_ADDR_LOW_0',
        'NVDLA_SDP_D_DST_BASE_ADDR_HIGH_0',
        'NVDLA_CACC_D_DATAOUT_ADDR_0',
    ]

    # Separate initial vs enable regs
    all_reg_writes = []
    enable_reg_writes = []

    for name, val, is_after_poll in reg_writes:
        # Relocate address
        old_val = val
        if name in addr_reg_names:
            for uvm_addr, vp_addr in addr_fixups.items():
                low_mask = uvm_addr & 0xffffffff
                if val == low_mask:
                    val = vp_addr & 0xffffffff
                    print(f"  Relocated {name}: 0x{old_val:08x} -> 0x{val:08x}")
                    break
                high_part = (uvm_addr >> 32) & 0xffffffff
                if val == high_part:
                    val = (vp_addr >> 32) & 0xffffffff
                    print(f"  Relocated {name}: 0x{old_val:08x} -> 0x{val:08x}")
                    break

        offset = reg_map.get(name, 0)
        if not offset:
            print(f"Warning: unknown register {name}", file=sys.stderr)

        if is_after_poll:
            enable_reg_writes.append((offset, val, name))
        else:
            all_reg_writes.append((offset, val, name))

    # Generate C file
    lines = []
    lines.append('#include <sys/mman.h>')
    lines.append('#include <unistd.h>')
    lines.append('#include <stdio.h>')
    lines.append('#include <stdlib.h>')
    lines.append('#include <stdint.h>')
    lines.append('#include <fcntl.h>')
    lines.append('#include <errno.h>')
    lines.append('')
    lines.append(f'#define DATA_BASE        0x{VP_DATA_BASE:08x}')
    lines.append(f'#define WEIGHT_BASE      0x{VP_WEIGHT_BASE:08x}')
    lines.append(f'#define OUT_BASE         0x{VP_OUT_BASE:08x}')
    lines.append(f'#define NVDLA_MMIO_BASE  0x10200000')
    lines.append(f'#define NVDLA_MMIO_SIZE  (0x10220000 - 0x10200000)')
    lines.append('')
    lines.append('struct dla_reg {')
    lines.append('    int32_t  offset;')
    lines.append('    uint32_t value;')
    lines.append('};')
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
    lines.append('#define CBUF_FLUSH_STATUS_OFFSET 0x{:04x}'.format(reg_map.get('NVDLA_CDMA_S_CBUF_FLUSH_STATUS_0', 0x300c)))
    lines.append('#define INTR_STATUS_OFFSET       0x{:04x}'.format(0x100c))
    lines.append('#define SDP_DONE_SHIFT            0')
    lines.append('')
    lines.append('int main(int argc, char* argv[])')
    lines.append('{')
    lines.append('    int fd = open("/dev/mem", O_RDWR);')
    lines.append('    if (fd < 0) { perror("open /dev/mem"); return 1; }')
    lines.append('    uint8_t* dla_mmio = (uint8_t*)mmap(NULL, NVDLA_MMIO_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, NVDLA_MMIO_BASE);')
    lines.append('    if (dla_mmio == MAP_FAILED) { perror("mmap dla"); return 1; }')
    lines.append('')
    lines.append('    printf("Programming registers...\\n");')
    lines.append('    const struct dla_reg* p = program_initial;')
    lines.append('    while (p->offset != -1) {')
    lines.append('        *(volatile uint32_t*)(dla_mmio + p->offset) = p->value;')
    lines.append('        p++;')
    lines.append('    }')
    lines.append('    printf("Done programming initial registers.\\n");')
    lines.append('')
    lines.append('    volatile uint32_t* cbuf_status = (volatile uint32_t*)(dla_mmio + CBUF_FLUSH_STATUS_OFFSET);')
    lines.append('    int loop = 1000000;')
    lines.append('    while (--loop > 0) {')
    lines.append('        if (*cbuf_status & 0x1) {')
    lines.append('            printf("CDMA -> CBUF flush done\\n");')
    lines.append('            break;')
    lines.append('        }')
    lines.append('    }')
    lines.append('    if (loop <= 0) { printf("CBUF flush timeout\\n"); }')
    lines.append('')
    lines.append('    printf("Firing OP_ENABLE...\\n");')
    lines.append('    p = program_enable;')
    lines.append('    while (p->offset != -1) {')
    lines.append('        *(volatile uint32_t*)(dla_mmio + p->offset) = p->value;')
    lines.append('        p++;')
    lines.append('    }')
    lines.append('')
    lines.append('    volatile uint32_t* intr_status = (volatile uint32_t*)(dla_mmio + INTR_STATUS_OFFSET);')
    lines.append('    loop = 100000000;')
    lines.append('    while (--loop > 0) {')
    lines.append('        if ((*intr_status >> SDP_DONE_SHIFT) & 1) {')
    lines.append('            printf("SDP done interrupt received!\\n");')
    lines.append('            break;')
    lines.append('        }')
    lines.append('    }')
    lines.append('    if (loop <= 0) { printf("SDP timeout\\n"); return 1; }')
    lines.append('')
    lines.append('    /* Read and dump output from VP DRAM via mmap */')
    lines.append('    printf("Reading output at OUT_BASE=0x%x...\\n", OUT_BASE);')
    lines.append('    uint32_t* out_mem = (uint32_t*)mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, OUT_BASE);')
    lines.append('    if (out_mem == MAP_FAILED) { perror("mmap output"); return 1; }')
    lines.append('    FILE* fp = fopen("sdp2mcif_output.dat", "w");')
    lines.append('    if (!fp) { perror("fopen"); return 1; }')
    lines.append('    for (int i = 0; i < 2; i++) {')
    lines.append('        fprintf(fp, "%08x\\n", out_mem[i]);')
    lines.append('    }')
    lines.append('    fclose(fp);')
    lines.append('    printf("Output written to sdp2mcif_output.dat\\n");')
    lines.append('')
    lines.append('    munmap(out_mem, 4096);')
    lines.append('    munmap(dla_mmio, NVDLA_MMIO_SIZE);')
    lines.append('    close(fd);')
    lines.append('    return 0;')
    lines.append('}')

    c_path = os.path.join(SCRIPT_DIR, f'{test_name}_test.c')
    with open(c_path, 'w') as f:
        f.write('\n'.join(lines))
    print(f"Generated {c_path}")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: gen_cfg2c.py <test.cfg>")
        sys.exit(1)
    cfg_path = sys.argv[1]
    test_dir = os.path.dirname(cfg_path)
    test_name = os.path.splitext(os.path.basename(cfg_path))[0]

    generate_c_source(cfg_path)

    # Generate devmem scripts from .dat files
    dt_dat = os.path.join(test_dir, f'{test_name}_dt.dat')
    wt_dat = os.path.join(test_dir, f'{test_name}_wt.dat')

    if os.path.exists(dt_dat):
        entries = parse_dat_file(dt_dat)
        generate_devmem_script(entries, VP_DATA_BASE, os.path.join(SCRIPT_DIR, 'dat_load.sh'))
    if os.path.exists(wt_dat):
        entries = parse_dat_file(wt_dat)
        generate_devmem_script(entries, VP_WEIGHT_BASE, os.path.join(SCRIPT_DIR, 'wt_load.sh'))
