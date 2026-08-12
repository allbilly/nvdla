set pagination off
set confirm off
set breakpoint pending on
set logging file /mnt/vp/gdb_gem_dump.log
set logging overwrite on
set logging on

python
import gdb

SUBMIT = 0xC0106440
GEM_CREATE = 0xC0106441
GEM_MMAP = 0xC0106442
PRIME_HANDLE_TO_FD = 0xC00C642D

gems = {}
pending_ioctl = None
pending_mmap = None
drm_fd = None

def u32(addr):
    return int.from_bytes(gdb.selected_inferior().read_memory(addr, 4).tobytes(), 'little')

def s32(addr):
    v = u32(addr)
    return v - 0x100000000 if v & 0x80000000 else v

def u64(addr):
    return int.from_bytes(gdb.selected_inferior().read_memory(addr, 8).tobytes(), 'little')

def read_bytes(addr, size):
    return gdb.selected_inferior().read_memory(addr, size).tobytes()

def dump_file(path, data):
    with open(path, 'wb') as f:
        f.write(data)

def dump_gems():
    for handle, info in sorted(gems.items()):
        vaddr = info.get('vaddr')
        size = int(info.get('size', 0))
        dump_size = min(size, 8192)
        if not vaddr or not dump_size:
            print('GEM_BUFFER handle=%d prime_fd=%s size=%d dumped=0 vaddr=%s map_offset=0x%x data=' % (
                handle, info.get('prime_fd', -1), size, hex(vaddr) if vaddr else '0x0', info.get('map_offset', 0)))
            continue
        try:
            data = read_bytes(vaddr, dump_size)
            dump_file('/mnt/vp/gem%d.bin' % handle, data)
            print('GEM_BUFFER handle=%d prime_fd=%s size=%d dumped=%d vaddr=0x%x map_offset=0x%x data=%s' % (
                handle, info.get('prime_fd', -1), size, len(data), vaddr, info.get('map_offset', 0), data.hex()))
        except Exception as e:
            print('GEM_BUFFER handle=%d prime_fd=%s size=%d dumped=ERR vaddr=0x%x map_offset=0x%x err=%s' % (
                handle, info.get('prime_fd', -1), size, vaddr, info.get('map_offset', 0), e))

class IoctlEntry(gdb.Breakpoint):
    def __init__(self):
        super(IoctlEntry, self).__init__('ioctl', internal=False)
    def stop(self):
        global pending_ioctl, drm_fd
        fd = int(gdb.parse_and_eval('$x0'))
        req = int(gdb.parse_and_eval('$x1')) & 0xffffffff
        arg = int(gdb.parse_and_eval('$x2'))
        pending_ioctl = (fd, req, arg)
        if req == SUBMIT:
            print('SUBMIT_BREAK fd=%d arg=0x%x' % (fd, arg))
            dump_gems()
            return True
        return False

class IoctlReturn(gdb.FinishBreakpoint):
    def __init__(self):
        super(IoctlReturn, self).__init__(internal=True)
    def stop(self):
        global pending_ioctl, drm_fd
        if not pending_ioctl:
            return False
        fd, req, arg = pending_ioctl
        ret = int(gdb.parse_and_eval('$x0'))
        if ret == 0 and req == GEM_CREATE:
            handle = u32(arg)
            size = u64(arg + 8)
            gems.setdefault(handle, {})['size'] = size
            drm_fd = fd
            print('GEM_CREATE handle=%d size=%d' % (handle, size))
        elif ret == 0 and req == PRIME_HANDLE_TO_FD:
            handle = u32(arg)
            prime_fd = s32(arg + 8)
            gems.setdefault(handle, {})['prime_fd'] = prime_fd
            print('PRIME handle=%d fd=%d' % (handle, prime_fd))
        elif ret == 0 and req == GEM_MMAP:
            handle = u32(arg)
            off = u64(arg + 8)
            gems.setdefault(handle, {})['map_offset'] = off
            print('GEM_MMAP handle=%d offset=0x%x' % (handle, off))
        pending_ioctl = None
        return False

class MmapEntry(gdb.Breakpoint):
    def __init__(self):
        super(MmapEntry, self).__init__('mmap', internal=False)
    def stop(self):
        global pending_mmap
        length = int(gdb.parse_and_eval('$x1'))
        fd = int(gdb.parse_and_eval('$x4'))
        off = int(gdb.parse_and_eval('$x5'))
        pending_mmap = (length, fd, off)
        return False

class MmapReturn(gdb.FinishBreakpoint):
    def __init__(self):
        super(MmapReturn, self).__init__(internal=True)
    def stop(self):
        global pending_mmap, drm_fd
        if not pending_mmap:
            return False
        length, fd, off = pending_mmap
        ret = int(gdb.parse_and_eval('$x0'))
        if ret > 0 and drm_fd is not None and fd == drm_fd:
            low = off & 0xffffffff
            for handle, info in gems.items():
                if info.get('map_offset') == low:
                    info['vaddr'] = ret
                    print('MMAP handle=%d vaddr=0x%x length=%d off=0x%x' % (handle, ret, length, off))
                    break
        pending_mmap = None
        return False

IoctlEntry()
MmapEntry()
end

commands 1
  silent
  python IoctlReturn()
  continue
end

commands 2
  silent
  python MmapReturn()
  continue
end

run --loadable /mnt/vp/test_Add.nvdla --image /mnt/vp/input1x5x7.pgm --rawdump
