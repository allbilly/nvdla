#!/usr/bin/env python3
"""Parse KMD dmesg blob dumps, identify register blobs, decode SDP config.

Usage:
  python3 parse_capture.py <dmesg_log_file>
  python3 parse_capture.py --blob <blob_index> <dmesg_log_file>
  python3 parse_capture.py --decode-all <dmesg_log_file>
  python3 parse_capture.py --decode-writes <dmesg_log_file>
  python3 parse_capture.py --cfg-writes <dmesg_log_file>

The dmesg log should contain nvdla_blob[N] hex dumps and nvdla_address[N] entries,
or nvdla_reg_write offset/value lines for direct register-write decoding.
"""
import re, struct, sys, os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")))
import parse as p

try:
    from REG_MAP import REG_MAP
except ImportError:
    REG_MAP = {}

def load_header_reg_map():
    root = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    header = os.path.join(root, "ref", "sw", "kmd", "firmware", "include", "opendla_initial.h")
    regs = {}
    pattern = re.compile(r'^#define\s+([A-Z0-9_]+_0)\s+\(_MK_ADDR_CONST\((0x[0-9a-fA-F]+)\)\)')
    try:
        with open(header) as f:
            for line in f:
                m = pattern.match(line)
                if m:
                    regs["NVDLA_" + m.group(1)] = int(m.group(2), 16)
    except OSError:
        pass
    return regs

REG_MAP = {**REG_MAP, **load_header_reg_map()}
REG_BY_OFFSET = {v: k for k, v in REG_MAP.items()}
REG_BLOCKS = (
    "NVDLA_SDP_RDMA", "NVDLA_PDP_RDMA", "NVDLA_CDP_RDMA",
    "NVDLA_CMAC_A", "NVDLA_CMAC_B", "NVDLA_CDMA", "NVDLA_CSC",
    "NVDLA_CACC", "NVDLA_SDP", "NVDLA_PDP", "NVDLA_CDP", "NVDLA_BDMA",
    "NVDLA_RBK", "NVDLA_GLB", "NVDLA_MCIF", "NVDLA_CVIF", "NVDLA_GEC",
)

# NVDLA data type names
DATA_TYPES = {0: "UNKNOWN", 1: "FLOAT", 2: "HALF", 3: "INT16", 4: "INT8"}
DATA_FORMATS = {0: "UNKNOWN", 1: "NCHW", 2: "NHWC", 3: "NCxHWx"}
PIXEL_FORMATS = {36: "FEATURE", 37: "FEATURE_X8"}
KNOWN_BLOB_NAMES = {40: "network_descriptor", 36: "dependency_graph", 116: "operation_list", 644: "surface_list"}

def parse_kmd_log(text):
    """Parse KMD dmesg log into blob data and address info."""
    blob_entries = {}  # index -> [hex_lines, header_info]
    address_entries = {}

    blob_start = re.compile(r'nvdla_blob\[(\d+)\]\s+(.*)')
    blob_hex = re.compile(r'nvdla_blob\s+([0-9a-f]+):\s*((?:[0-9a-f]{2}\s)*[0-9a-f]{2})')
    address_line = re.compile(r'nvdla_address\[(\d+)\]\s+handle=(\d+)\s+offset=(0x[0-9a-f]+)')
    blob_header = re.compile(r'nvdla_blob\[(\d+)\]\s+prime_fd=(\d+)\s+handle=(\d+)\s+size=(\d+)\s+dma=(0x[0-9a-f]+)')

    current_blob_idx = None

    for line in text.split('\n'):
        m = blob_header.search(line)
        if m:
            idx = int(m.group(1))
            current_blob_idx = idx
            blob_entries[idx] = {"prime_fd": int(m.group(2)), "kmd_handle": int(m.group(3)),
                                 "size": int(m.group(4)), "dma": m.group(5), "hex_lines": []}
            continue

        m = address_line.search(line)
        if m:
            idx = int(m.group(1))
            address_entries[idx] = {"handle": int(m.group(2)), "offset": m.group(3)}
            continue

        m = blob_hex.search(line)
        if m and current_blob_idx is not None:
            addr = int(m.group(1), 16)
            hex_bytes = m.group(2).replace(' ', '')
            data = bytes.fromhex(hex_bytes)
            blob_entries[current_blob_idx]["hex_lines"].append(data)
            continue

    blobs = {}
    for idx, entry in blob_entries.items():
        data = b''.join(entry["hex_lines"])
        expected = entry.get("size", len(data))
        blobs[idx] = {"data": data[:expected] if len(data) > expected else data,
                      "size": entry.get("size", len(data)),
                      "dma": entry.get("dma", "?"),
                      "prime_fd": entry.get("prime_fd", -1),
                      "kmd_handle": entry.get("kmd_handle", -1)}
    return blobs, address_entries


def parse_reg_writes(text):
    """Parse direct KMD register writes and group them by scheduler operation."""
    reg_write = re.compile(r'nvdla_reg_write\s+offset=(0x[0-9a-f]+)\s+value=(0x[0-9a-f]+)', re.I)
    prepare = re.compile(r'Prepare\s+(.+?)\s+operation\s+index\s+(-?\d+)\s+ROI\s+(\d+)\s+dep_count\s+(\d+)', re.I)
    complete = re.compile(r'Completed\s+(.+?)\s+operation\s+index\s+(-?\d+)\s+ROI\s+(\d+)', re.I)

    groups = []
    current = None
    all_writes = []

    for line in text.split('\n'):
        m = prepare.search(line)
        if m:
            current = {"processor": m.group(1), "index": int(m.group(2)),
                       "roi": int(m.group(3)), "dep_count": int(m.group(4)),
                       "writes": []}
            groups.append(current)
            continue

        m = complete.search(line)
        if m and current and current["processor"].lower() == m.group(1).lower():
            current = None
            continue

        m = reg_write.search(line)
        if not m:
            continue

        write = {"offset": int(m.group(1), 16), "value": int(m.group(2), 16)}
        write["name"] = REG_BY_OFFSET.get(write["offset"])
        all_writes.append(write)
        if current is not None:
            current["writes"].append(write)

    return groups, all_writes


def cfg_reg_name(name, offset):
    if not name:
        return f"UNKNOWN_0x{offset:04x}"
    for prefix in REG_BLOCKS:
        if name.startswith(prefix + "_"):
            return f"{prefix}.{name[len(prefix) + 1:]}"
    return name


def format_reg_writes(groups, all_writes, as_cfg=False):
    lines = []
    if groups:
        for group in groups:
            if as_cfg:
                lines.append(f"// {group['processor']} operation index {group['index']} ROI {group['roi']} dep_count {group['dep_count']}")
            else:
                lines.append(f"% {group['processor']} operation index={group['index']} ROI={group['roi']} dep_count={group['dep_count']} writes={len(group['writes'])}")
            for write in group["writes"]:
                name = write["name"] or f"UNKNOWN_0x{write['offset']:04x}"
                if as_cfg:
                    lines.append(f"reg_write({cfg_reg_name(write['name'], write['offset'])}, 0x{write['value']:08x});")
                else:
                    lines.append(f"  {name} @ 0x{write['offset']:04x} = 0x{write['value']:08x}")
            lines.append("")
    elif all_writes:
        for write in all_writes:
            name = write["name"] or f"UNKNOWN_0x{write['offset']:04x}"
            if as_cfg:
                lines.append(f"reg_write({cfg_reg_name(write['name'], write['offset'])}, 0x{write['value']:08x});")
            else:
                lines.append(f"{name} @ 0x{write['offset']:04x} = 0x{write['value']:08x}")
    return '\n'.join(lines)


def identify_blob(blob_data):
    """Identify what a blob is based on size and content."""
    sz = len(blob_data)
    name = KNOWN_BLOB_NAMES.get(sz, f"data_{sz}")
    if sz == 40:
        return "network_descriptor (task-0-addr0)"
    elif sz == 36:
        return "dependency_graph (task-0-dep_graph)"
    elif sz == 116:
        return "operation_list (task-0-op_list)"
    elif sz == 644:
        return "surface_list (task-0-surf_list)"
    elif sz == 1120:
        nz = sum(1 for b in blob_data if b)
        first_halfs = struct.unpack_from('<560H', blob_data[:1120])
        nz_halfs = sum(1 for v in first_halfs if v)
        if nz_halfs <= 35:
            return f"input_tensor (FP16 NCxHWx, 1120B, {nz_halfs} non-zero halfs)"
        if all(v == 0x3C00 or v == 0 for v in first_halfs[:64]):
            return "x1_data (tb-0 probe, FP16 NCxHWx)"
        return f"tensor_data (1120B, {nz} non-zero bytes)"
    elif sz == 4096:
        nz = sum(1 for b in blob_data if b)
        if nz == 0:
            return "scratch (all zeros)"
        return f"scratch_with_data ({nz} non-zero bytes)"
    return f"unknown ({sz}B)"


def decode_register_blobs(blobs):
    """Try to decode SDP registers if we have the right blobs."""
    net_desc = None
    op_list = None
    surf_list = None

    for idx, blob in blobs.items():
        sz = len(blob["data"])
        if sz == 40:
            net_desc = blob["data"]
        elif sz == 116:
            op_list = blob["data"]
        elif sz == 644:
            surf_list = blob["data"]

    if not (net_desc and op_list and surf_list):
        missing = []
        if not net_desc: missing.append("network_descriptor (40B)")
        if not op_list: missing.append("operation_list (116B)")
        if not surf_list: missing.append("surface_list (644B)")
        return {"error": f"Missing register blobs: {', '.join(missing)}"}

    net = p.parse_network_desc(net_desc)
    op = p.parse_sdp_op_desc(op_list, 0)
    surf = p.parse_sdp_surface(surf_list, 0)

    result = {"network": net, "surface": surf, "operation": op, "registers": []}
    for name, value in p.sdp_op_regs(surf, op):
        reg_off = p.SDP_REGS[name]
        result["registers"].append({"name": name, "offset": reg_off, "value": value})
    return result


def format_registers(reg_result, indent=""):
    """Format register decode result as string."""
    if "error" in reg_result:
        return f"{indent}Error: {reg_result['error']}"
    lines = []
    for reg in reg_result["registers"]:
        lines.append(f"{indent}NVDLA_SDP_{reg['name']}_0 @ 0x{reg['offset']:04x} = 0x{reg['value'] & 0xFFFFFFFF:08x} ({reg['value']})")
    return '\n'.join(lines)


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ('-h', '--help'):
        print(__doc__)
        return 0

    decode_all = False
    decode_writes = False
    cfg_writes = False
    filter_blob = None
    args = [a for a in sys.argv[1:] if not a.startswith('--')]

    for a in sys.argv[1:]:
        if a == '--decode-all':
            decode_all = True
        elif a == '--decode-writes':
            decode_writes = True
        elif a == '--cfg-writes':
            cfg_writes = True
        elif a.startswith('--blob='):
            filter_blob = int(a.split('=')[1])
        elif sys.argv[1] == '--blob' and len(sys.argv) > 2:
            filter_blob = int(sys.argv[2])
            args = [a for a in args if a != sys.argv[1] and a != sys.argv[2]]

    if not args:
        print("Error: no log file specified")
        return 1

    with open(args[0]) as f:
        text = f.read()

    blobs, addresses = parse_kmd_log(text)
    reg_groups, reg_writes = parse_reg_writes(text)

    if cfg_writes:
        print(format_reg_writes(reg_groups, reg_writes, as_cfg=True))
        return 0

    if decode_writes:
        print(format_reg_writes(reg_groups, reg_writes, as_cfg=False))
        return 0

    if not blobs:
        if reg_writes:
            print(f"No blob data found in log, but found {len(reg_writes)} register writes. Use --decode-writes or --cfg-writes.")
        else:
            print("No blob data found in log")
        return 1

    print(f"Found {len(blobs)} blobs, {len(addresses)} address entries\n")
    if reg_writes:
        print(f"Found {len(reg_writes)} register writes in {len(reg_groups)} operation groups. Use --decode-writes or --cfg-writes to decode them.\n")

    if filter_blob is not None:
        if filter_blob not in blobs:
            print(f"Blob index {filter_blob} not found (available: {sorted(blobs.keys())})")
            return 1
        indices = [filter_blob]
    else:
        indices = sorted(blobs.keys())

    for idx in indices:
        blob = blobs[idx]
        data = blob["data"]
        sz = len(data)
        ident = identify_blob(data)
        print(f"% blob[{idx}] dma={blob['dma']} size={blob['size']} actual={sz} {ident}")

        if idx in addresses:
            a = addresses[idx]
            print(f"  address: handle={a['handle']} offset={a['offset']}")

        if sz <= 64:
            hexl = data.hex()
            line = ' '.join(f'{data[i]:02x}' for i in range(sz))
            print(f"  data: {line}")
        else:
            print(f"  first 64B: {' '.join(f'{data[i]:02x}' for i in range(64))}")

        if sz == 40:
            try:
                net = p.parse_network_desc(data)
                print(f"  network: op_desc_idx={net['operation_desc_index']} surf_desc_idx={net['surface_desc_index']} num_ops={net['num_operations']}")
            except Exception as e:
                print(f"  net parse error: {e}")

        elif sz == 1120:
            halfs = struct.unpack_from('<560H', data[:1120])
            nz_positions = [(i, v) for i, v in enumerate(halfs) if v]
            if 0 < len(nz_positions) <= 35:
                print(f"  FP16 values: {nz_positions[:10]}{'...' if len(nz_positions) > 10 else ''}")
                print(f"  -> input tensor with {len(nz_positions)} non-zero pixels")
            elif all(v == 0x3C00 or v == 0 for v in halfs):
                nz = sum(1 for v in halfs if v)
                print(f"  -> tb-0 probe data ({nz} positions with FP16 1.0)")

        elif sz == 644:
            try:
                surf = p.parse_sdp_surface(data, 0)
                for name, c in surf.items():
                    if c["size"]:
                        print(f"  cube {name}: address={c['address']} dims={c['width']}x{c['height']}x{c['channel']} line={c['line_stride']} surf={c['surf_stride']}")
            except Exception as e:
                print(f"  surf parse error: {e}")

        elif sz == 116:
            try:
                op = p.parse_sdp_op_desc(data, 0)
                print(f"  SDP: src_prec={op['src_precision']}({DATA_TYPES.get(op['src_precision'],'?')}) dst_prec={op['dst_precision']}({DATA_TYPES.get(op['dst_precision'],'?')}) conv_mode={op['conv_mode']}")
                for unit_name, unit_key in [("x1_op/BS", "x1_op"), ("x2_op/BN", "x2_op"), ("y_op/EW", "y_op")]:
                    u = op[unit_key]
                    status = "enabled" if u["enable"] else "bypassed"
                    alu_info = f"alu_type={u['alu_type']} "
                    print(f"  {unit_name}: {status} {alu_info}(mode={u['mode']}, act={u['act']}, shift={u['shift_value']})")
            except Exception as e:
                print(f"  op parse error: {e}")
        print()

    if decode_all:
        print("=" * 60)
        print("Full register decode:")
        print("=" * 60)
        reg_result = decode_register_blobs(blobs)
        print(format_registers(reg_result))
        print()

        if "surface" in reg_result:
            print("Surface mapping:")
            for name, c in reg_result["surface"].items():
                if c["size"]:
                    print(f"  {name}: address={c['address']} -> address_list slot {c['address']}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
