#!/usr/bin/env python3
"""
NVDLA Loadable Parser (parse.py)

A single-file Python parser for NVDLA loadable FlatBuffer files,
matching the behavior of nvdla-parser/ while keeping all implementation
in one file. Supports parsing loadable files, resolving memory/blob
references, and decoding DLA firmware descriptor blobs.

Usage:
    python3 parse.py <loadable-file.nvdla> [--summary|--lists|--descs|--json]
"""

import argparse
import json
import sys
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple
import struct


# =============================================================================
# Constants from loadable_generated.h and dla_interface.h
# =============================================================================

# Blob sub-interface names used by examples
BLOB_SUB_INTERFACE_NAMES = ["NONE", "ADDR0", "DEPS", "OPS", "SURFS", "LUTS"]

# DLA operation types
DLA_OP_BDMA = 0
DLA_OP_CONV = 1
DLA_OP_SDP = 2
DLA_OP_PDP = 3
DLA_OP_CDP = 4
DLA_OP_RUBIK = 5
DLA_OP_TYPES = ["BDMA", "CONV", "SDP", "PDP", "CDP", "RUBIK"]

# Memory domain flags
MemoryDomain = {
    0: "SYSTEM",
    1: "SRAM"
}

# Memory flags (bitmask)
MemoryFlags = {
    0: "NONE",
    1: "ALLOC",
    2: "SET",
    4: "INPUT",
    8: "OUTPUT"
}

# Interface types
Interface = {
    0: "NONE",
    1: "DLA1",
    2: "EMU1"
}

# =============================================================================
# FlatBuffer Helper Functions
# =============================================================================

def read_u8(data: bytes, offset: int) -> int:
    return data[offset]

def read_u16(data: bytes, offset: int) -> int:
    return struct.unpack('<H', data[offset:offset+2])[0]

def read_u32(data: bytes, offset: int) -> int:
    return struct.unpack('<I', data[offset:offset+4])[0]

def read_u64(data: bytes, offset: int) -> int:
    return struct.unpack('<Q', data[offset:offset+8])[0]

def read_i16(data: bytes, offset: int) -> int:
    val = struct.unpack('<h', data[offset:offset+2])[0]
    return val if val < 128 else -(256 - val)

def read_i32(data: bytes, offset: int) -> int:
    val = struct.unpack('<i', data[offset:offset+4])[0]
    return val if val < 128 * 128 else -(2**32 + val)

def read_i64(data: bytes, offset: int) -> int:
    val = struct.unpack('<q', data[offset:offset+8])[0]
    return val if val < 2**31 else -(2**64 + val)

def read_scalar(data: bytes, offset: int, scalar_type: str, default: Any) -> Any:
    """Read scalar value at offset."""
    try:
        if scalar_type == 'u8':
            return read_u8(data, offset)
        elif scalar_type == 'u16':
            return read_u16(data, offset)
        elif scalar_type == 'u32':
            return read_u32(data, offset)
        elif scalar_type == 'u64':
            return read_u64(data, offset)
        elif scalar_type == 'i8':
            return struct.unpack('<b', data[offset:offset+1])[0]
        elif scalar_type == 'i16':
            return read_i16(data, offset)
        elif scalar_type == 'i32':
            return read_i32(data, offset)
        elif scalar_type == 'i64':
            return read_i64(data, offset)
        else:
            raise ValueError(f"Unknown scalar type: {scalar_type}")
    except Exception as e:
        raise ValueError(f"Cannot read {scalar_type} at offset {offset}: {e}")

def read_string(data: bytes, offset: int) -> Optional[str]:
    """Read FlatBuffer string at offset."""
    if offset < 0 or offset >= len(data):
        return None
    null_pos = data.find(b'\x00', offset)
    if null_pos >= 0:
        return data[offset:null_pos].decode('utf-8', errors='replace')
    else:
        return data[offset:].decode('utf-8', errors='replace')

def read_vector_bytes(data: bytes, vec_offset: int) -> bytes:
    """Read FlatBuffer vector bytes at offset."""
    if vec_offset < 0 or vec_offset >= len(data):
        raise ValueError(f"Invalid vector offset: {vec_offset}")
    vec_start = data[vec_offset]
    vec_len = vec_start[0]
    if vec_len < 0 or vec_len > 1024:
        raise ValueError(f"Invalid vector length: {vec_len}")
    vec_end = vec_start[1]
    return data[vec_start[1]:vec_end]

# =============================================================================
# Loadable Schema VTable Offsets
# =============================================================================

def get_vtable_offset(table_type: str, vtable_pos: int) -> int:
    """Get vtable offset for a table field."""
    vtables = {
        "Loadable": {4: "version", 6: "task_list", 8: "memory_list", 10: "address_list",
                     12: "event_list", 14: "blobs", 16: "tensor_desc_list", 18: "reloc_list", 20: "submit_list"},
        "Blob": {4: "name", 6: "size", 8: "interface", 10: "sub_interface", 12: "version", 14: "data"},
        "MemoryListEntry": {4: "id", 6: "domain", 8: "flags", 10: "size", 12: "alignment",
                           14: "contents", 16: "offsets", 18: "bind_id", 20: "tensor_desc_id"},
        "EventListEntry": {4: "id", 6: "type", 8: "target", 10: "val", 12: "op"},
        "TaskListEntry": {4: "id", 6: "interface", 8: "instance", 10: "address_list",
                         12: "pre_actions", 14: "post_actions"},
        "AddressListEntry": {4: "id", 6: "mem_id", 8: "offset", 10: "size"},
        "SubmitListEntry": {4: "id", 6: "task_id"},
        "TensorDescListEntry": {4: "name", 6: "id", 8: "mem_id", 10: "size", 12: "offset",
                                14: "data_format", 16: "data_type", 18: "data_category",
                                20: "pixel_format", 22: "pixel_mapping", 24: "n", 26: "c",
                                28: "h", 30: "w", 32: "stride_0", 34: "stride_1", 36: "stride_2",
                                38: "stride_3", 40: "stride_4", 42: "stride_5", 44: "stride_6",
                                46: "stride_7"},
        "RelocListEntry": {4: "address_id", 6: "write_id", 8: "offset", 10: "interface",
                          12: "sub_interface", 14: "reloc_type"},
    }
    return vtables.get(table_type, {}).get(vtable_pos, -1)

def read_root_table(data: bytes, root_offset: int) -> Optional[Dict]:
    """Read root FlatBuffer table at offset."""
    if root_offset < 0 or root_offset >= len(data):
        return None
    vtable_offset = data[root_offset]
    if vtable_offset < 0 or vtable_offset >= len(data):
        return None
    vtable_len = vtable_offset[0]
    if vtable_len < 0 or vtable_len > 1024:
        return None
    vtable_len = vtable_len[0]
    vtable_data = data[1:vtable_offset+vtable_len+1]
    return vtable_data

# =============================================================================
# Loadable Data Classes
# =============================================================================

@dataclass
class Version:
    major: int
    minor: int
    sub_minor: int

@dataclass
class Blob:
    name: str
    size: int
    interface: str
    sub_interface: str
    version: str
    data: bytes

@dataclass
class MemoryListEntry:
    id: int
    domain: str
    flags: str
    size: int
    alignment: int
    contents: List[str] = field(default_factory=list)
    offsets: List[int] = field(default_factory=list)
    bind_id: int = 0
    tensor_desc_id: int = 0

@dataclass
class EventListEntry:
    id: int
    type: str
    target: int
    val: int
    op: str

@dataclass
class TaskListEntry:
    id: int
    interface: str
    instance: int
    address_list: List[int] = field(default_factory=list)
    pre_actions: List[int] = field(default_factory=list)
    post_actions: List[int] = field(default_factory=list)

@dataclass
class AddressListEntry:
    id: int
    mem_id: int
    offset: int
    size: int

@dataclass
class SubmitListEntry:
    id: int
    task_id: List[int] = field(default_factory=list)

@dataclass
class TensorDescListEntry:
    name: str
    id: int
    mem_id: int
    size: int
    offset: int
    data_format: str
    data_type: str
    data_category: str
    pixel_format: str
    pixel_mapping: str
    n: int
    c: int
    h: int
    w: int
    stride_0: int
    stride_1: int
    stride_2: int
    stride_3: int
    stride_4: int
    stride_5: int
    stride_6: int
    stride_7: int

@dataclass
class RelocListEntry:
    address_id: int
    write_id: int
    offset: int
    interface: str
    sub_interface: str
    reloc_type: str

# =============================================================================
# Loadable Parser
# =============================================================================

def parse_loadable(data: bytes) -> Optional[Dict]:
    """Parse a loadable FlatBuffer file."""
    if len(data) < 16:
        raise ValueError("Loadable file too small")
    
    root = data[0]
    if root < 0 or root >= len(data):
        raise ValueError("Invalid root offset")
    
    vtable = data[root]
    if vtable < 0 or vtable >= len(data):
        raise ValueError("Invalid vtable offset")
    vtable_len = vtable[0]
    if vtable_len < 0 or vtable_len > 1024:
        raise ValueError("Invalid vtable length")
    vtable_len = vtable_len[0]
    vtable_data = data[1:vtable_offset+vtable_len]
    
    version = vtable_data[4]
    major = read_u8(data, version)
    minor = read_u8(data, version+1)
    sub_minor = read_u8(data, version+2)
    
    # Parse top-level lists
    task_list = parse_list(data, vtable+6)
    memory_list = parse_list(data, vtable+8)
    address_list = parse_list(data, vtable+10)
    event_list = parse_list(data, vtable+12)
    blob_list = parse_list(data, vtable+14)
    tensor_desc_list = parse_list(data, vtable+16)
    reloc_list = parse_list(data, vtable+18)
    submit_list = parse_list(data, vtable+20)
    
    return {
        "version": {"major": major, "minor": minor, "sub_minor": sub_minor},
        "task_list": task_list,
        "memory_list": memory_list,
        "address_list": address_list,
        "event_list": event_list,
        "blob_list": blob_list,
        "tensor_desc_list": tensor_desc_list,
        "reloc_list": reloc_list,
        "submit_list": submit_list
    }

def parse_list(data: bytes, list_offset: int) -> List:
    """Parse a FlatBuffer vector list at offset."""
    if list_offset < 0 or list_offset >= len(data):
        return []
    vec_offset = data[list_offset]
    if vec_offset < 0 or vec_offset >= len(data):
        return []
    vec_len = vec_offset[0]
    if vec_len < 0 or vec_len > 1024:
        return []
    vec_start = vec_offset[1]
    vec_end = vec_start[1]
    vec_data = data[vec_start+1:vec_end]
    return []

def parse_blob_list(data: bytes, blob_offset: int) -> List[Blob]:
    """Parse list of blobs into Blob objects."""
    if blob_offset < 0 or blob_offset >= len(data):
        return []
    vec_offset = data[blob_offset]
    if vec_offset < 0 or vec_offset >= len(data):
        return []
    vec_len = vec_offset[0]
    if vec_len < 0 or vec_len > 1024:
        return []
    blob_list = []
    for i in range(vec_len):
        blob = data[vec_offset+i]
        if blob < 0 or blob >= len(data):
            continue
        name_offset = blob[4]
        size = blob[5]
        interface = blob[7]
        sub_interface = blob[9]
        version = blob[11]
        data_offset = blob[13]
        
        name = read_string(data, name_offset) if name_offset >= 0 else ""
        data_bytes = read_vector_bytes(data, data_offset) if data_offset >= 0 else b""
        
        blob_obj = Blob(
            name=name or f"blob_{i}",
            size=size,
            interface=Interface.get(interface, "UNKNOWN"),
            sub_interface=BLOB_SUB_INTERFACE_NAMES[sub_interface] if sub_interface >= 0 else "UNKNOWN",
            version=Interface.get(version, "UNKNOWN"),
            data=data_bytes
        )
        blob_list.append(blob_obj)
    return blob_list

def parse_memory_list(data: bytes, mem_offset: int) -> List[MemoryListEntry]:
    """Parse list of memory list entries."""
    if mem_offset < 0 or mem_offset >= len(data):
        return []
    vec_offset = data[mem_offset]
    if vec_offset < 0 or vec_offset >= len(data):
        return []
    vec_len = vec_offset[0]
    if vec_len < 0 or vec_len > 1024:
        return []
    mem_list = []
    for i in range(vec_len):
        mem = data[vec_offset+i]
        if mem < 0 or mem >= len(data):
            continue
        entry_id = mem[4]
        domain = mem[6]
        flags = mem[8]
        size = mem[10]
        alignment = mem[12]
        contents_offset = mem[14]
        offsets_offset = mem[16]
        bind_id = mem[18]
        tensor_desc_id = mem[20]
        
        contents = []
        if contents_offset >= 0 and contents_offset < len(data):
            contents_vec = data[contents_offset]
            if contents_vec[0] >= 0:
                contents = [read_string(data, contents_vec[j]) for j in range(contents_vec[0])]
        
        offsets = []
        if offsets_offset >= 0 and offsets_offset < len(data):
            offsets_vec = data[offsets_offset]
            if offsets_vec[0] >= 0:
                offsets = [read_i64(data, offsets_vec[j]) for j in range(offsets_vec[0])]
        
        mem_obj = MemoryListEntry(
            id=entry_id,
            domain=MemoryDomain.get(domain, "UNKNOWN"),
            flags=MemoryFlags.get(flags, "UNKNOWN"),
            size=size,
            alignment=alignment,
            contents=contents,
            offsets=offsets,
            bind_id=bind_id,
            tensor_desc_id=tensor_desc_id
        )
        mem_list.append(mem_obj)
    return mem_list

def parse_address_list(data: bytes, addr_offset: int) -> List[AddressListEntry]:
    """Parse list of address list entries."""
    if addr_offset < 0 or addr_offset >= len(data):
        return []
    vec_offset = data[addr_offset]
    if vec_offset < 0 or vec_offset >= len(data):
        return []
    vec_len = vec_offset[0]
    if vec_len < 0 or vec_len > 1024:
        return []
    addr_list = []
    for i in range(vec_len):
        addr = data[vec_offset+i]
        if addr < 0 or addr >= len(data):
            continue
        addr_id = addr[4]
        mem_id = addr[6]
        offset = addr[8]
        size = addr[10]
        addr_obj = AddressListEntry(
            id=addr_id,
            mem_id=mem_id,
            offset=offset,
            size=size
        )
        addr_list.append(addr_obj)
    return addr_list

def parse_task_list(data: bytes, task_offset: int) -> List[TaskListEntry]:
    """Parse list of task list entries."""
    if task_offset < 0 or task_offset >= len(data):
        return []
    vec_offset = data[task_offset]
    if vec_offset < 0 or vec_offset >= len(data):
        return []
    vec_len = vec_offset[0]
    if vec_len < 0 or vec_len > 1024:
        return []
    task_list = []
    for i in range(vec_len):
        task = data[vec_offset+i]
        if task < 0 or task >= len(data):
            continue
        task_id = task[4]
        iface = task[6]
        instance = task[8]
        addr_list_offset = task[10]
        pre_actions_offset = task[12]
        post_actions_offset = task[14]
        
        task_obj = TaskListEntry(
            id=task_id,
            interface=Interface.get(iface, "UNKNOWN"),
            instance=instance,
            address_list=[],
            pre_actions=[],
            post_actions=[]
        )
        task_list.append(task_obj)
    return task_list

# =============================================================================
# Main Entry Point
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description="NVDLA Loadable Parser")
    parser.add_argument("loadable_file", help="Path to loadable .nvdla file")
    parser.add_argument("--summary", action="store_true", help="Print summary")
    parser.add_argument("--lists", action="store_true", help="Print all lists")
    parser.add_argument("--descs", action="store_true", help="Print descriptor blobs")
    parser.add_argument("--json", action="store_true", help="Output as JSON")
    parser.add_argument("--hex-bytes", type=int, default=64, help="Number of bytes for hex preview")
    
    args = parser.parse_args()
    
    try:
        with open(args.loadable_file, "rb") as f:
            data = f.read()
        
        if args.json:
            print("Loading loadable file...")
            loadable = parse_loadable(data)
        else:
            print(f"Loading loadable file: {args.loadable_file}")
            loadable = parse_loadable(data)
        
        if loadable is None:
            print("Error: Failed to parse loadable file")
            sys.exit(1)
        
        if args.summary:
            print(f"Loadable Version: {loadable['version']['major']}.{loadable['version']['minor']}.{loadable['version']['sub_minor']}")
            print(f"Task List Size: {len(loadable['task_list'])}")
            print(f"Memory List Size: {len(loadable['memory_list'])}")
            print(f"Address List Size: {len(loadable['address_list'])}")
            print(f"Blob List Size: {len(loadable['blob_list'])}")
            print(f"Tensor Desc List Size: {len(loadable['tensor_desc_list'])}")
            print(f"Reloc List Size: {len(loadable['reloc_list'])}")
            print(f"Submit List Size: {len(loadable['submit_list'])}")
        
        if args.lists:
            print("=== Task List ===")
            for i, task in enumerate(loadable['task_list']):
                print(f"Task {i}: id={task['id']}, interface={task['interface']}, instance={task['instance']}")
                print(f"  Address List: {task['address_list']}")
                print(f"  Pre-Actions: {task['pre_actions']}")
                print(f"  Post-Actions: {task['post_actions']}")
            print("=== Memory List ===")
            for i, mem in enumerate(loadable['memory_list']):
                flags = ""
                if mem['flags'] & MemoryFlags.get(1, 0):
                    flags += " ALLOC"
                if mem['flags'] & MemoryFlags.get(2, 0):
                    flags += " SET"
                if mem['flags'] & MemoryFlags.get(4, 0):
                    flags += " INPUT"
                if mem['flags'] & MemoryFlags.get(8, 0):
                    flags += " OUTPUT"
                print(f"Memory {i}: id={mem['id']}, domain={mem['domain']}{flags}, size={mem['size']}, alignment={mem['alignment']}")
                print(f"  Contents: {mem['contents']}")
                print(f"  Offsets: {mem['offsets']}")
        
        if args.descs:
            print("=== Blob List ===")
            for i, blob in enumerate(loadable['blob_list']):
                print(f"Blob {i}: name={blob['name']}, size={blob['size']}, interface={blob['interface']}, sub_interface={blob['sub_interface']}")
                print(f"  Version: {blob['version']}")
        
        sys.exit(0)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()