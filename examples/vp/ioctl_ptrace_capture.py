#!/usr/bin/env python3
"""Capture ioctl arguments from nvdla_runtime with ptrace on aarch64."""
from __future__ import annotations

import ctypes
import ctypes.util
import mmap
import os
import signal
import struct
import sys


PTRACE_TRACEME = 0
PTRACE_PEEKDATA = 2
PTRACE_SYSCALL = 24
PTRACE_GETREGSET = 0x4204
PTRACE_O_TRACESYSGOOD = 1
PTRACE_SETOPTIONS = 0x4200
NT_PRSTATUS = 1

SYS_IOCTL_AARCH64 = 29
SYS_MMAP_AARCH64 = 222
SYS_MMAP_ARM = 192
SYS_MMAP2_ARM = 192

DRM_IOCTL_NVDLA_SUBMIT = 0xC0106440
DRM_IOCTL_NVDLA_GEM_CREATE = 0xC0106441
DRM_IOCTL_NVDLA_GEM_MMAP = 0xC0106442
DRM_IOCTL_PRIME_HANDLE_TO_FD = 0xC00C642D

libc = ctypes.CDLL(ctypes.util.find_library("c") or "libc.so.6", use_errno=True)


class IOVec(ctypes.Structure):
    _fields_ = [("iov_base", ctypes.c_void_p), ("iov_len", ctypes.c_size_t)]


libc.process_vm_readv.argtypes = [ctypes.c_int, ctypes.POINTER(IOVec), ctypes.c_ulong, ctypes.POINTER(IOVec), ctypes.c_ulong, ctypes.c_ulong]
libc.process_vm_readv.restype = ctypes.c_ssize_t


def ptrace(request: int, pid: int, addr: int = 0, data: int | ctypes.c_void_p = 0) -> int:
    ret = libc.ptrace(request, pid, ctypes.c_void_p(addr), data)
    if ret == -1:
        err = ctypes.get_errno()
        raise OSError(err, os.strerror(err), f"ptrace({request})")
    return ret


def get_regs(pid: int) -> list[int]:
    buf = ctypes.create_string_buffer(34 * 8)
    iov = IOVec(ctypes.cast(buf, ctypes.c_void_p), ctypes.sizeof(buf))
    ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, ctypes.byref(iov))
    return list(struct.unpack("<34Q", buf.raw[:34 * 8]))


def peek(pid: int, addr: int, size: int) -> bytes:
    out = bytearray()
    word = ctypes.sizeof(ctypes.c_long)
    for off in range(0, size, word):
        ctypes.set_errno(0)
        val = libc.ptrace(PTRACE_PEEKDATA, pid, ctypes.c_void_p(addr + off), 0)
        err = ctypes.get_errno()
        if val == -1 and err:
            break
        out.extend(int(val & ((1 << (word * 8)) - 1)).to_bytes(word, "little"))
    return bytes(out[:size])


def peek_user(pid: int, addr: int, size: int) -> bytes:
    data = peek(pid, addr, size)
    if data or (addr >> 32) != 0xFFFFFFFF:
        return data
    return peek(pid, addr & 0xFFFFFFFF, size)


def vm_read(pid: int, addr: int, size: int) -> bytes:
    buf = ctypes.create_string_buffer(size)
    local = IOVec(ctypes.cast(buf, ctypes.c_void_p), size)
    remote = IOVec(ctypes.c_void_p(addr), size)
    n = libc.process_vm_readv(pid, ctypes.byref(local), 1, ctypes.byref(remote), 1, 0)
    if n <= 0:
        return b""
    return buf.raw[:n]


def ioc_size(req: int) -> int:
    return (req >> 16) & 0x3FFF


def should_log_ioctl(req: int, submit_only: bool, task_buffers: bool) -> bool:
    if not submit_only:
        return True
    if req == DRM_IOCTL_NVDLA_SUBMIT:
        return True
    if task_buffers and req in (DRM_IOCTL_NVDLA_GEM_CREATE, DRM_IOCTL_NVDLA_GEM_MMAP, DRM_IOCTL_PRIME_HANDLE_TO_FD):
        return False
    return False


def should_trace_ioctl(req: int, submit_only: bool, task_buffers: bool) -> bool:
    if not submit_only:
        return True
    if req == DRM_IOCTL_NVDLA_SUBMIT:
        return True
    return task_buffers and req in (DRM_IOCTL_NVDLA_GEM_CREATE, DRM_IOCTL_NVDLA_GEM_MMAP, DRM_IOCTL_PRIME_HANDLE_TO_FD)


def dump_submit_nested(log, pid: int, data: bytes) -> None:
    if len(data) < 16:
        return
    tasks_ptr, num_tasks, flags, version = struct.unpack("<QHHI", data[:16])
    log.write(f"SUBMIT tasks=0x{tasks_ptr:x} num_tasks={num_tasks} flags=0x{flags:x} version={version}\n")
    for i in range(num_tasks):
        task_addr = tasks_ptr + i * 16
        task = peek_user(pid, task_addr, 16)
        log.write(f"TASK[{i}] addr=0x{task_addr:x} data={task.hex()}\n")
        if len(task) < 16:
            continue
        num_addr, timeout, addr_list = struct.unpack("<IIQ", task)
        log.write(f"TASK[{i}] num_addresses={num_addr} timeout=0x{timeout:x} address_list=0x{addr_list:x}\n")
        addr_bytes = peek_user(pid, addr_list, num_addr * 16)
        log.write(f"ADDR_LIST[{i}] addr=0x{addr_list:x} count={num_addr} data={addr_bytes.hex()}\n")


def submit_handles(pid: int, data: bytes) -> list[int]:
    if len(data) < 16:
        return []
    tasks_ptr, num_tasks, _flags, _version = struct.unpack("<QHHI", data[:16])
    out: list[int] = []
    for i in range(num_tasks):
        task = peek_user(pid, tasks_ptr + i * 16, 16)
        if len(task) < 16:
            continue
        num_addr, _timeout, addr_list = struct.unpack("<IIQ", task)
        addr_bytes = peek_user(pid, addr_list, num_addr * 16)
        for off in range(0, len(addr_bytes), 16):
            if off + 16 <= len(addr_bytes):
                handle, _reserved, _offset = struct.unpack_from("<IIQ", addr_bytes, off)
                out.append(handle)
    return out


def read_child_fd(pid: int, fd: int, size: int) -> bytes:
    try:
        with open(f"/proc/{pid}/fd/{fd}", "rb", buffering=0) as f:
            return f.read(size)
    except OSError:
        return b""


def mmap_child_fd(pid: int, fd: int, size: int) -> bytes:
    try:
        dup_fd = os.open(f"/proc/{pid}/fd/{fd}", os.O_RDONLY)
    except OSError:
        return b""
    try:
        with mmap.mmap(dup_fd, size, prot=mmap.PROT_READ, flags=mmap.MAP_SHARED) as mm:
            return mm[:]
    except (OSError, ValueError):
        return b""
    finally:
        os.close(dup_fd)


def mmap_drm_fd(pid: int, drm_fd: int, offset: int, size: int) -> bytes:
    try:
        dup_fd = os.open(f"/proc/{pid}/fd/{drm_fd}", os.O_RDWR)
    except OSError:
        return b""
    try:
        with mmap.mmap(dup_fd, size, flags=mmap.MAP_SHARED, prot=mmap.PROT_READ | mmap.PROT_WRITE, offset=offset) as mm:
            return mm[:]
    except (OSError, ValueError):
        return b""
    finally:
        os.close(dup_fd)


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: ioctl_ptrace_capture.py [--submit-only|--task-buffers] OUT_LOG command ...", file=sys.stderr)
        return 2
    submit_only = False
    task_buffers = False
    buffer_limit = 0
    argi = 1
    if sys.argv[argi] == "--submit-only":
        submit_only = True
        argi += 1
    elif sys.argv[argi] == "--task-buffers":
        submit_only = True
        task_buffers = True
        buffer_limit = 8192
        argi += 1
    out_path = sys.argv[argi]
    cmd = sys.argv[argi + 1:]
    pid = os.fork()
    if pid == 0:
        libc.ptrace(PTRACE_TRACEME, 0, 0, 0)
        os.kill(os.getpid(), signal.SIGSTOP)
        os.execvp(cmd[0], cmd)

    _, status = os.waitpid(pid, 0)
    if not os.WIFSTOPPED(status):
        raise RuntimeError("child did not stop")
    ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD)

    entering = True
    current_syscall = None
    current_mmap = None
    last_create_size = 0
    drm_fd = -1
    handles: dict[int, dict[str, int]] = {}
    with open(out_path, "w") as log:
        while True:
            ptrace(PTRACE_SYSCALL, pid, 0, 0)
            _, status = os.waitpid(pid, 0)
            if os.WIFEXITED(status):
                log.write(f"EXIT {os.WEXITSTATUS(status)}\n")
                return os.WEXITSTATUS(status)
            if os.WIFSIGNALED(status):
                log.write(f"SIGNALED {os.WTERMSIG(status)}\n")
                return 128 + os.WTERMSIG(status)
            if not os.WIFSTOPPED(status) or os.WSTOPSIG(status) != (signal.SIGTRAP | 0x80):
                continue

            regs = get_regs(pid)
            syscall = regs[8]
            if entering:
                current_syscall = None
                current_mmap = None
            if syscall == SYS_IOCTL_AARCH64:
                fd, req, arg = regs[0], regs[1], regs[2]
                size = ioc_size(req)
                if entering:
                    current_syscall = (syscall, fd, req, arg, size)
                    if not should_trace_ioctl(req, submit_only, task_buffers):
                        entering = not entering
                        continue
                    data = peek_user(pid, arg, size) if arg and size else b""
                    if should_log_ioctl(req, submit_only, task_buffers):
                        log.write(f"ENTER fd={fd} req=0x{req:08x} arg=0x{arg:x} size={size} data={data.hex()}\n")
                    if req == DRM_IOCTL_NVDLA_SUBMIT:
                        if task_buffers:
                            log.write(f"KNOWN_HANDLES {handles}\n")
                            for handle, info in sorted(handles.items()):
                                addr = info.get("vaddr", 0)
                                buf_size = info.get("size", 0)
                                dump_size = min(buf_size, buffer_limit) if buffer_limit else buf_size
                                if addr and dump_size:
                                    blob = vm_read(pid, addr, dump_size) or peek(pid, addr, dump_size)
                                    if not blob:
                                        blob = mmap_drm_fd(pid, drm_fd, info.get("map_offset", 0), dump_size)
                                    if not blob:
                                        blob = mmap_drm_fd(pid, drm_fd, 0x100000000 | info.get("map_offset", 0), dump_size)
                                    log.write(
                                        f"GEM_BUFFER handle={handle} prime_fd={info.get('prime_fd', -1)} "
                                        f"size={buf_size} dumped={len(blob)} vaddr=0x{addr:x} "
                                        f"map_offset=0x{info.get('map_offset', 0):x} data={blob.hex()}\n"
                                    )
                            for prime_fd in submit_handles(pid, data):
                                for handle, info in sorted(handles.items()):
                                    if info.get("prime_fd") != prime_fd:
                                        continue
                                    addr = info.get("vaddr", 0)
                                    buf_size = info.get("size", 0)
                                    if buffer_limit and buf_size > buffer_limit:
                                        buf_size = buffer_limit
                                    blob = b""
                                    if addr and buf_size:
                                        blob = vm_read(pid, addr, buf_size) or peek(pid, addr, buf_size)
                                    elif buf_size:
                                        blob = read_child_fd(pid, prime_fd, buf_size)
                                    if not blob and buf_size:
                                        blob = mmap_child_fd(pid, prime_fd, buf_size)
                                    if blob:
                                        log.write(
                                            f"TASK_BUFFER handle={handle} prime_fd={prime_fd} "
                                            f"size={buf_size} vaddr=0x{addr:x} map_offset=0x{info.get('map_offset', 0):x} "
                                            f"data={blob.hex()}\n"
                                        )
                        elif not submit_only:
                            for handle, info in sorted(handles.items()):
                                addr = info.get("vaddr", 0)
                                buf_size = info.get("size", 0)
                                if addr and buf_size:
                                    blob = peek(pid, addr, buf_size)
                                    log.write(
                                        f"BUFFER handle={handle} prime_fd={info.get('prime_fd', -1)} "
                                        f"size={buf_size} vaddr=0x{addr:x} map_offset=0x{info.get('map_offset', 0):x} "
                                        f"data={blob.hex()}\n"
                                    )
                        dump_submit_nested(log, pid, data)
                else:
                    ret = ctypes.c_longlong(regs[0]).value
                    _, enter_fd, enter_req, enter_arg, enter_size = current_syscall or (None, fd, req, arg, size)
                    fd, req, arg, size = enter_fd, enter_req, enter_arg, enter_size
                    if not should_trace_ioctl(req, submit_only, task_buffers):
                        entering = not entering
                        continue
                    data = peek_user(pid, arg, size) if arg and size else b""
                    if should_log_ioctl(req, submit_only, task_buffers):
                        log.write(f"EXIT fd={fd} req=0x{req:08x} ret={ret} arg=0x{arg:x} size={size} data={data.hex()}\n")
                    if ret == 0 and req == DRM_IOCTL_NVDLA_GEM_CREATE and len(data) >= 16:
                        drm_fd = fd
                        handle, flags, last_create_size = struct.unpack("<IIQ", data[:16])
                        handles.setdefault(handle, {})["size"] = last_create_size
                    elif ret == 0 and req == DRM_IOCTL_PRIME_HANDLE_TO_FD and len(data) >= 12:
                        handle, flags, prime_fd = struct.unpack("<IIi", data[:12])
                        handles.setdefault(handle, {})["prime_fd"] = prime_fd
                    elif ret == 0 and req == DRM_IOCTL_NVDLA_GEM_MMAP and len(data) >= 16:
                        handle, reserved, map_offset = struct.unpack("<IIQ", data[:16])
                        handles.setdefault(handle, {})["map_offset"] = map_offset
                log.flush()
            elif syscall in (SYS_MMAP_AARCH64, SYS_MMAP_ARM, SYS_MMAP2_ARM):
                if entering:
                    current_mmap = (regs[0], regs[1], regs[2], regs[3], regs[4], regs[5])
                else:
                    if not current_mmap:
                        entering = not entering
                        continue
                    ret = ctypes.c_longlong(regs[0]).value
                    if current_mmap and ret > 0:
                        _, length, prot, flags, fd_arg, off = current_mmap
                        if task_buffers:
                            log.write(
                                f"MMAP_SEEN ret=0x{ret:x} length={length} prot=0x{prot:x} "
                                f"flags=0x{flags:x} fd={fd_arg} off=0x{off:x}\n"
                            )
                            log.flush()
                        handle = 0
                        if fd_arg == drm_fd:
                            for h, info in handles.items():
                                if info.get("map_offset") == (off & 0xFFFFFFFF):
                                    handle = h
                                    break
                        if not handle:
                            for h, info in handles.items():
                                if info.get("prime_fd") == fd_arg:
                                    handle = h
                                    break
                        if handle:
                            info = handles.setdefault(handle, {})
                            info["vaddr"] = ret
                            info.setdefault("size", length)
                            if task_buffers:
                                log.write(
                                    f"TRACK_MMAP handle={handle} ret=0x{ret:x} length={length} prot=0x{prot:x} "
                                    f"flags=0x{flags:x} fd={fd_arg} off=0x{off:x}\n"
                                )
                                log.flush()
                            elif not submit_only:
                                log.write(
                                    f"MMAP handle={handle} ret=0x{ret:x} length={length} prot=0x{prot:x} "
                                    f"flags=0x{flags:x} fd={fd_arg} off=0x{off:x}\n"
                                )
                                log.flush()
            entering = not entering


if __name__ == "__main__":
    raise SystemExit(main())
