#!/usr/bin/env python3
import struct
import sys


class ParseError(Exception):
    pass


INTERFACE = {0: "NONE", 1: "DLA1", 2: "EMU1"}
MEM_DOMAIN = {0: "SYSTEM", 1: "SRAM"}
BLOB_SUBIF = {0: "NONE", 1: "ADDR0", 2: "DEPS", 3: "OPS", 4: "SURFS", 5: "LUTS"}
EVENT_TYPE = {0: "EVENTTYPE0", 1: "EVENTTYPE1", 2: "EVENTTYPE2"}
EVENT_OP = {0: "WAIT", 1: "SIGNAL"}
OP_TYPE = {0: "BDMA", 1: "CONV", 2: "SDP", 3: "PDP", 4: "CDP", 5: "RUBIK"}

DATA_CUBE_SIZE = 32
DATA_CUBE_COMPACT_SIZE = 28
DATA_CUBE_ONNC_SIZE = 28
COMMON_OP_SIZE = 36
SDP_SURFACE_SIZE = DATA_CUBE_SIZE * 5
SDP_OP_SIZE = 116
SURFACE_CONTAINER_SIZE = 644
OP_CONTAINER_SIZE = 880

SDP_REGS = {
    "D_OP_ENABLE": 0xB038,
    "D_DATA_CUBE_WIDTH": 0xB03C,
    "D_DATA_CUBE_HEIGHT": 0xB040,
    "D_DATA_CUBE_CHANNEL": 0xB044,
    "D_DST_BASE_ADDR_LOW": 0xB048,
    "D_DST_BASE_ADDR_HIGH": 0xB04C,
    "D_DST_LINE_STRIDE": 0xB050,
    "D_DST_SURFACE_STRIDE": 0xB054,
    "D_DP_BS_CFG": 0xB058,
    "D_DP_BS_ALU_CFG": 0xB05C,
    "D_DP_BS_ALU_SRC_VALUE": 0xB060,
    "D_DP_BS_MUL_CFG": 0xB064,
    "D_DP_BS_MUL_SRC_VALUE": 0xB068,
    "D_DP_BN_CFG": 0xB06C,
    "D_DP_BN_ALU_CFG": 0xB070,
    "D_DP_BN_ALU_SRC_VALUE": 0xB074,
    "D_DP_BN_MUL_CFG": 0xB078,
    "D_DP_BN_MUL_SRC_VALUE": 0xB07C,
    "D_DP_EW_CFG": 0xB080,
    "D_DP_EW_ALU_CFG": 0xB084,
    "D_DP_EW_ALU_SRC_VALUE": 0xB088,
    "D_DP_EW_ALU_CVT_OFFSET_VALUE": 0xB08C,
    "D_DP_EW_ALU_CVT_SCALE_VALUE": 0xB090,
    "D_DP_EW_ALU_CVT_TRUNCATE_VALUE": 0xB094,
    "D_DP_EW_MUL_CFG": 0xB098,
    "D_DP_EW_MUL_SRC_VALUE": 0xB09C,
    "D_DP_EW_MUL_CVT_OFFSET_VALUE": 0xB0A0,
    "D_DP_EW_MUL_CVT_SCALE_VALUE": 0xB0A4,
    "D_DP_EW_MUL_CVT_TRUNCATE_VALUE": 0xB0A8,
    "D_DP_EW_TRUNCATE_VALUE": 0xB0AC,
    "D_FEATURE_MODE_CFG": 0xB0B0,
    "D_DST_DMA_CFG": 0xB0B4,
    "D_DST_BATCH_STRIDE": 0xB0B8,
    "D_DATA_FORMAT": 0xB0BC,
    "D_CVT_OFFSET": 0xB0C0,
    "D_CVT_SCALE": 0xB0C4,
    "D_CVT_SHIFT": 0xB0C8,
}


def need(buf, off, size, what):
    if off < 0 or off + size > len(buf):
        raise ParseError(f"{what} out of range at {off}, need {size}, file {len(buf)}")


def unpack(buf, off, fmt, what):
    size = struct.calcsize(fmt)
    need(buf, off, size, what)
    return struct.unpack_from(fmt, buf, off)[0]


def uoffset(buf, off, what):
    return unpack(buf, off, "<I", what)


def field_pos(buf, table, vt):
    need(buf, table, 4, "table")
    vtable = table - unpack(buf, table, "<i", "vtable offset")
    need(buf, vtable, 4, "vtable")
    vlen = unpack(buf, vtable, "<H", "vtable length")
    if vt >= vlen:
        return None
    rel = unpack(buf, vtable + vt, "<H", "field offset")
    return table + rel if rel else None


def scalar(buf, table, vt, fmt, default=0):
    pos = field_pos(buf, table, vt)
    if pos is None:
        return default
    return unpack(buf, pos, fmt, f"field {vt}")


def struct_pos(buf, table, vt):
    return field_pos(buf, table, vt)


def vector_pos(buf, table, vt):
    pos = field_pos(buf, table, vt)
    if pos is None:
        return None
    vec = pos + uoffset(buf, pos, f"vector field {vt}")
    need(buf, vec, 4, "vector")
    return vec


def string_at(buf, pos):
    if pos is None:
        return ""
    start = pos + uoffset(buf, pos, "string offset")
    length = uoffset(buf, start, "string length")
    need(buf, start + 4, length, "string bytes")
    return buf[start + 4:start + 4 + length].decode("utf-8", "replace")


def vec_len(buf, vec):
    return 0 if vec is None else uoffset(buf, vec, "vector length")


def vec_table(buf, vec, i):
    elem = vec + 4 + i * 4
    return elem + uoffset(buf, elem, "table vector element")


def vec_scalar(buf, vec, i, fmt):
    size = struct.calcsize(fmt)
    return unpack(buf, vec + 4 + i * size, fmt, "scalar vector element")


def vec_string(buf, vec, i):
    return string_at(buf, vec + 4 + i * 4)


def vec_bytes(buf, vec):
    n = vec_len(buf, vec)
    need(buf, vec + 4, n, "byte vector")
    return bytes(buf[vec + 4:vec + 4 + n])


def version(buf, pos):
    if pos is None:
        raise ParseError("missing required loadable version")
    need(buf, pos, 3, "version")
    return {"major": buf[pos], "minor": buf[pos + 1], "sub_minor": buf[pos + 2]}


def enum_name(names, value):
    return f"{value}:{names.get(value, 'UNKNOWN')}"


def mem_flags(value):
    parts = [name for bit, name in ((1, "ALLOC"), (2, "SET"), (4, "INPUT"), (8, "OUTPUT")) if value & bit]
    return "|".join(parts) if parts else "NONE"


def s16(v):
    return v - 0x10000 if v & 0x8000 else v


def u32(v):
    return v & 0xFFFFFFFF


def parse_network_desc(data):
    if len(data) < 40:
        raise ParseError("network descriptor too small")
    vals = struct.unpack_from("<8h6h4HhBB", data, 0)
    return {"operation_desc_index": vals[0], "surface_desc_index": vals[1],
            "dependency_graph_index": vals[2], "lut_data_index": vals[3],
            "roi_array_index": vals[4], "surface_index": vals[5], "stat_list_index": vals[6],
            "op_head": list(vals[8:14]), "num_rois": vals[14], "num_operations": vals[15],
            "num_luts": vals[16], "num_addresses": vals[17], "input_layer": vals[18],
            "dynamic_roi": vals[19]}


def parse_common_op(data, idx):
    off = idx * COMMON_OP_SIZE
    if off + COMMON_OP_SIZE > len(data):
        raise ParseError("common op descriptor array too small")
    index, roi_index, op_type, dep_count = struct.unpack_from("<hbBB", data, off)
    consumers = []
    pos = off + 8
    for _ in range(6):
        ci, ev, _res = struct.unpack_from("<hBB", data, pos)
        consumers.append({"index": ci, "event": ev})
        pos += 4
    fi, fev, _fres = struct.unpack_from("<hBB", data, pos)
    return {"index": index, "roi_index": roi_index, "op_type": op_type,
            "dependency_count": dep_count, "consumers": consumers,
            "fused_parent": {"index": fi, "event": fev}}


def parse_cube(data, off):
    vals = struct.unpack_from("<HhIIHHHHIII", data, off)
    return {"type": vals[0], "address": vals[1], "offset": vals[2], "size": vals[3],
            "width": vals[4], "height": vals[5], "channel": vals[6],
            "line_stride": vals[8], "surf_stride": vals[9], "plane_stride": vals[10]}


def parse_cube_compact(data, off):
    vals = struct.unpack_from("<HhIHHHHIII", data, off)
    return {"type": vals[0], "address": vals[1], "offset": 0, "size": vals[2],
            "width": vals[3], "height": vals[4], "channel": vals[5],
            "line_stride": vals[7], "surf_stride": vals[8], "plane_stride": vals[9]}


def parse_sdp_surface(data, idx):
    compact_size = DATA_CUBE_COMPACT_SIZE * 5
    if len(data) % compact_size == 0 or len(data) == 644:
        off = idx * compact_size
        names = ("src_data", "x1_data", "x2_data", "y_data", "dst_data")
        return {name: parse_cube_compact(data, off + i * DATA_CUBE_COMPACT_SIZE) for i, name in enumerate(names)}
    off = idx * SDP_SURFACE_SIZE if len(data) == SDP_SURFACE_SIZE else idx * SURFACE_CONTAINER_SIZE
    if off + SDP_SURFACE_SIZE > len(data):
        raise ParseError("SDP surface descriptor array too small")
    names = ("src_data", "x1_data", "x2_data", "y_data", "dst_data")
    return {name: parse_cube(data, off + i * DATA_CUBE_SIZE) for i, name in enumerate(names)}


def parse_cvt_param(data, off):
    scale, truncate, enable, offset = struct.unpack_from("<hBBi", data, off)
    return {"scale": scale, "truncate": truncate, "enable": enable, "offset": offset}


def parse_sdp_op_unit(data, off):
    enable, alu_type, typ, mode, act, shift, truncate, precision, alu_operand, mul_operand = struct.unpack_from("<BBBBBBBBii", data, off)
    return {"enable": enable, "alu_type": alu_type, "type": typ, "mode": mode,
            "act": act, "shift_value": shift, "truncate": truncate, "precision": precision,
            "alu_operand": alu_operand, "mul_operand": mul_operand,
            "alu_cvt": parse_cvt_param(data, off + 16), "mul_cvt": parse_cvt_param(data, off + 24)}


def parse_sdp_op_desc(data, idx):
    off = idx * SDP_OP_SIZE if len(data) == SDP_OP_SIZE else idx * OP_CONTAINER_SIZE
    if off + SDP_OP_SIZE > len(data):
        raise ParseError("SDP op descriptor array too small")
    src_precision, dst_precision, lut_index = struct.unpack_from("<BBh", data, off)
    return {"src_precision": src_precision, "dst_precision": dst_precision, "lut_index": lut_index,
            "out_cvt": parse_cvt_param(data, off + 4),
            "conv_mode": data[off + 12], "batch_num": data[off + 13],
            "batch_stride": struct.unpack_from("<I", data, off + 16)[0],
            "x1_op": parse_sdp_op_unit(data, off + 20),
            "x2_op": parse_sdp_op_unit(data, off + 52),
            "y_op": parse_sdp_op_unit(data, off + 84)}


def sdp_cfg(op):
    bypass = 0 if op["enable"] else 1
    alu_bypass = 0 if op["type"] in (2, 3) else 1
    mul_bypass = 0 if op["type"] in (1, 3) else 1
    return bypass | (alu_bypass << 1) | (op["alu_type"] << 2) | (mul_bypass << 4) | ((1 if op["act"] else 0) << 6)


def sdp_op_regs(surface, op):
    dst = surface["dst_data"]
    regs = []
    add = regs.append
    add(("D_DATA_CUBE_WIDTH", dst["width"]))
    add(("D_DATA_CUBE_HEIGHT", dst["height"]))
    add(("D_DATA_CUBE_CHANNEL", dst["channel"]))
    add(("D_DST_BASE_ADDR_LOW", u32(dst["offset"])))
    add(("D_DST_BASE_ADDR_HIGH", 0))
    add(("D_DST_LINE_STRIDE", dst["line_stride"]))
    add(("D_DST_SURFACE_STRIDE", dst["surf_stride"]))
    add(("D_DP_BS_CFG", sdp_cfg(op["x1_op"])))
    add(("D_DP_BS_ALU_CFG", op["x1_op"]["shift_value"]))
    add(("D_DP_BS_ALU_SRC_VALUE", u32(op["x1_op"]["alu_operand"])))
    add(("D_DP_BS_MUL_CFG", op["x1_op"]["truncate"]))
    add(("D_DP_BS_MUL_SRC_VALUE", u32(op["x1_op"]["mul_operand"])))
    add(("D_DP_BN_CFG", sdp_cfg(op["x2_op"])))
    add(("D_DP_BN_ALU_CFG", op["x2_op"]["shift_value"]))
    add(("D_DP_BN_ALU_SRC_VALUE", u32(op["x2_op"]["alu_operand"])))
    add(("D_DP_BN_MUL_CFG", op["x2_op"]["truncate"]))
    add(("D_DP_BN_MUL_SRC_VALUE", u32(op["x2_op"]["mul_operand"])))
    add(("D_DP_EW_CFG", sdp_cfg(op["y_op"])))
    add(("D_DP_EW_ALU_CFG", op["y_op"]["mode"] | (op["y_op"]["alu_cvt"]["enable"] << 8)))
    add(("D_DP_EW_ALU_SRC_VALUE", u32(op["y_op"]["alu_operand"])))
    add(("D_DP_EW_ALU_CVT_OFFSET_VALUE", u32(op["y_op"]["alu_cvt"]["offset"])))
    add(("D_DP_EW_ALU_CVT_SCALE_VALUE", u32(op["y_op"]["alu_cvt"]["scale"])))
    add(("D_DP_EW_ALU_CVT_TRUNCATE_VALUE", op["y_op"]["alu_cvt"]["truncate"]))
    add(("D_DP_EW_MUL_CFG", op["y_op"]["mode"] | (op["y_op"]["mul_cvt"]["enable"] << 8)))
    add(("D_DP_EW_MUL_SRC_VALUE", u32(op["y_op"]["mul_operand"])))
    add(("D_DP_EW_MUL_CVT_OFFSET_VALUE", u32(op["y_op"]["mul_cvt"]["offset"])))
    add(("D_DP_EW_MUL_CVT_SCALE_VALUE", u32(op["y_op"]["mul_cvt"]["scale"])))
    add(("D_DP_EW_MUL_CVT_TRUNCATE_VALUE", op["y_op"]["mul_cvt"]["truncate"]))
    add(("D_DP_EW_TRUNCATE_VALUE", op["y_op"]["truncate"]))
    feature = (op["conv_mode"] << 0) | (0 << 1) | (op["batch_num"] << 8)
    add(("D_FEATURE_MODE_CFG", feature))
    add(("D_DST_DMA_CFG", dst["type"]))
    add(("D_DST_BATCH_STRIDE", op["batch_stride"]))
    add(("D_DATA_FORMAT", op["src_precision"] | (op["dst_precision"] << 2)))
    add(("D_CVT_OFFSET", u32(op["out_cvt"]["offset"])))
    add(("D_CVT_SCALE", u32(op["out_cvt"]["scale"])))
    add(("D_CVT_SHIFT", op["out_cvt"]["truncate"]))
    add(("D_OP_ENABLE", 1))
    return regs


def descriptor_context(l, task):
    memory_by_id = {m["id"]: m for m in l["memory_list"]}
    address_by_id = {a["id"]: a for a in l["address_list"]}
    blob_by_name = {b["name"]: b for b in l["blob_list"]}

    def blob_for_address_id(address_id):
        address = address_by_id.get(address_id)
        memory = memory_by_id.get(address["mem_id"]) if address else None
        return blob_by_name.get(memory["contents"][0]) if memory and memory["contents"] else None

    def blob_for_slot(slot):
        if slot < 0 or slot >= len(task["address_list"]):
            return None
        return blob_for_address_id(task["address_list"][slot])

    net_blob = blob_for_slot(0)
    if not net_blob:
        return None
    net = parse_network_desc(net_blob["data"])
    return {"network": net, "network_blob": net_blob,
            "common_blob": blob_for_slot(net["dependency_graph_index"]),
            "surface_blob": blob_for_slot(net["surface_desc_index"]),
            "operation_blob": blob_for_slot(net["operation_desc_index"])}


def parse_blob(buf, t):
    ver = version(buf, struct_pos(buf, t, 12)) if struct_pos(buf, t, 12) is not None else None
    data_vec = vector_pos(buf, t, 14)
    data = vec_bytes(buf, data_vec) if data_vec is not None else b""
    return {"name": string_at(buf, field_pos(buf, t, 4)), "size": scalar(buf, t, 6, "<Q"),
            "interface": scalar(buf, t, 8, "<I"), "sub_interface": scalar(buf, t, 10, "<I"),
            "version": ver, "data": data}


def parse_memory(buf, t):
    cvec = vector_pos(buf, t, 14)
    ovec = vector_pos(buf, t, 16)
    return {"id": scalar(buf, t, 4, "<H"), "domain": scalar(buf, t, 6, "<B"),
            "flags": scalar(buf, t, 8, "<H"), "size": scalar(buf, t, 10, "<Q"),
            "alignment": scalar(buf, t, 12, "<I"),
            "contents": [vec_string(buf, cvec, i) for i in range(vec_len(buf, cvec))],
            "offsets": [vec_scalar(buf, ovec, i, "<Q") for i in range(vec_len(buf, ovec))],
            "bind_id": scalar(buf, t, 18, "<H"), "tensor_desc_id": scalar(buf, t, 20, "<H")}


def parse_task(buf, t):
    avec = vector_pos(buf, t, 10)
    pre = vector_pos(buf, t, 12)
    post = vector_pos(buf, t, 14)
    return {"id": scalar(buf, t, 4, "<H"), "interface": scalar(buf, t, 6, "<I"),
            "instance": scalar(buf, t, 8, "<h"),
            "address_list": [vec_scalar(buf, avec, i, "<H") for i in range(vec_len(buf, avec))],
            "pre_actions": [vec_scalar(buf, pre, i, "<H") for i in range(vec_len(buf, pre))],
            "post_actions": [vec_scalar(buf, post, i, "<H") for i in range(vec_len(buf, post))]}


def parse_addr(buf, t):
    return {"id": scalar(buf, t, 4, "<H"), "mem_id": scalar(buf, t, 6, "<H"),
            "offset": scalar(buf, t, 8, "<Q"), "size": scalar(buf, t, 10, "<Q")}


def parse_event(buf, t):
    return {"id": scalar(buf, t, 4, "<H"), "type": scalar(buf, t, 6, "<B"),
            "target": scalar(buf, t, 8, "<H"), "val": scalar(buf, t, 10, "<I"),
            "op": scalar(buf, t, 12, "<B")}


def parse_submit(buf, t):
    tv = vector_pos(buf, t, 6)
    return {"id": scalar(buf, t, 4, "<H"), "task_id": [vec_scalar(buf, tv, i, "<H") for i in range(vec_len(buf, tv))]}


def parse_tensor(buf, t):
    d = {"name": string_at(buf, field_pos(buf, t, 4)), "id": scalar(buf, t, 6, "<H"), "mem_id": scalar(buf, t, 8, "<H"),
         "size": scalar(buf, t, 10, "<Q"), "offset": scalar(buf, t, 12, "<Q")}
    for name, vt, fmt in [("data_format",14,"<B"),("data_type",16,"<B"),("data_category",18,"<B"),("pixel_format",20,"<B"),("pixel_mapping",22,"<B"),("n",24,"<i"),("c",26,"<i"),("h",28,"<i"),("w",30,"<i")]:
        d[name] = scalar(buf, t, vt, fmt)
    for i, vt in enumerate(range(32, 48, 2)):
        d[f"stride_{i}"] = scalar(buf, t, vt, "<I")
    return d


def parse_reloc(buf, t):
    return {"address_id": scalar(buf, t, 4, "<H"), "write_id": scalar(buf, t, 6, "<H"),
            "offset": scalar(buf, t, 8, "<Q"), "interface": scalar(buf, t, 10, "<I"),
            "sub_interface": scalar(buf, t, 12, "<I"), "reloc_type": scalar(buf, t, 14, "<B")}


def parse_table_vec(buf, root, vt, fn):
    vec = vector_pos(buf, root, vt)
    return [fn(buf, vec_table(buf, vec, i)) for i in range(vec_len(buf, vec))]


def parse_loadable(buf):
    if len(buf) < 8:
        raise ParseError("file too small")
    root = uoffset(buf, 0, "root offset")
    need(buf, root, 4, "root table")
    return {"version": version(buf, struct_pos(buf, root, 4)),
            "task_list": parse_table_vec(buf, root, 6, parse_task),
            "memory_list": parse_table_vec(buf, root, 8, parse_memory),
            "address_list": parse_table_vec(buf, root, 10, parse_addr),
            "event_list": parse_table_vec(buf, root, 12, parse_event),
            "blob_list": parse_table_vec(buf, root, 14, parse_blob),
            "tensor_desc_list": parse_table_vec(buf, root, 16, parse_tensor),
            "reloc_list": parse_table_vec(buf, root, 18, parse_reloc),
            "submit_list": parse_table_vec(buf, root, 20, parse_submit)}


def print_regs(l):
    for task in l["task_list"]:
        if task["interface"] != 1:
            continue
        ctx = descriptor_context(l, task)
        if not ctx or not (ctx["common_blob"] and ctx["surface_blob"] and ctx["operation_blob"]):
            continue
        net = ctx["network"]
        print(f"task[{task['id']}] NVDLA decoded registers")
        for i in range(net["num_operations"]):
            common = parse_common_op(ctx["common_blob"]["data"], i)
            print(f"layer[{i}] op_type={enum_name(OP_TYPE, common['op_type'])}")
            if common["op_type"] != 2:
                print("  register decode for this op type is not implemented yet")
                continue
            surface = parse_sdp_surface(ctx["surface_blob"]["data"], i)
            op = parse_sdp_op_desc(ctx["operation_blob"]["data"], i)
            for cube_name in ("src_data", "x1_data", "x2_data", "y_data", "dst_data"):
                c = surface[cube_name]
                print(f"  cube {cube_name}: type={c['type']} address={c['address']} offset={c['offset']} size={c['size']} dims={c['width']}x{c['height']}x{c['channel']} line={c['line_stride']} surf={c['surf_stride']}")
            print("  regs:")
            for name, value in sdp_op_regs(surface, op):
                print(f"    NVDLA_SDP_{name}_0 @ 0x{SDP_REGS[name]:04x} = 0x{value & 0xFFFFFFFF:08x} ({value})")


def main():
    if len(sys.argv) < 2:
        print("Usage: python parse.py <loadable_file>", file=sys.stderr)
        return 1
    loadable_file = sys.argv[1]
    try:
        with open(loadable_file, "rb") as f:
            loadable = parse_loadable(f.read())
        print_regs(loadable)
    except (OSError, ParseError, struct.error) as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
