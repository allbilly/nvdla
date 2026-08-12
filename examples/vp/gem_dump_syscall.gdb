set pagination off
set confirm off
set logging file /mnt/vp/gdb_gem_dump.log
set logging overwrite on
set logging on
set environment LD_LIBRARY_PATH /mnt/vp

set $drm_fd = -1
set $in_ioctl = 0
set $ifd = 0
set $ireq = 0
set $iarg = 0
set $in_mmap = 0
set $mfd = 0
set $mlen = 0
set $moff = 0

set $g1_size = 0
set $g2_size = 0
set $g3_size = 0
set $g4_size = 0
set $g5_size = 0
set $g6_size = 0
set $g7_size = 0
set $g8_size = 0
set $g9_size = 0
set $g1_off = 0
set $g2_off = -1
set $g3_off = -1
set $g4_off = -1
set $g5_off = -1
set $g6_off = -1
set $g7_off = -1
set $g8_off = -1
set $g9_off = -1
set $g1_addr = 0
set $g2_addr = 0
set $g3_addr = 0
set $g4_addr = 0
set $g5_addr = 0
set $g6_addr = 0
set $g7_addr = 0
set $g8_addr = 0
set $g9_addr = 0
set $g1_fd = -1
set $g2_fd = -1
set $g3_fd = -1
set $g4_fd = -1
set $g5_fd = -1
set $g6_fd = -1
set $g7_fd = -1
set $g8_fd = -1
set $g9_fd = -1

catch syscall 29
commands
  silent
  if $in_ioctl == 0
    set $in_ioctl = 1
    set $ifd = $x0
    set $ireq = $x1 & 0xffffffff
    set $iarg = $x2
    if $ireq == 0xc0106440
      disable 1
      disable 2
      printf "SUBMIT_BREAK fd=%d arg=0x%x\n", $ifd, $iarg
      printf "GEM_ADDR handle=1 addr=0x%x size=%u off=0x%x\n", $g1_addr, $g1_size, $g1_off
      printf "GEM_ADDR handle=2 addr=0x%x size=%u off=0x%x\n", $g2_addr, $g2_size, $g2_off
      printf "GEM_ADDR handle=3 addr=0x%x size=%u off=0x%x\n", $g3_addr, $g3_size, $g3_off
      printf "GEM_ADDR handle=4 addr=0x%x size=%u off=0x%x\n", $g4_addr, $g4_size, $g4_off
      printf "GEM_ADDR handle=5 addr=0x%x size=%u off=0x%x\n", $g5_addr, $g5_size, $g5_off
      printf "GEM_ADDR handle=6 addr=0x%x size=%u off=0x%x\n", $g6_addr, $g6_size, $g6_off
      printf "GEM_ADDR handle=7 addr=0x%x size=%u off=0x%x\n", $g7_addr, $g7_size, $g7_off
      printf "GEM_ADDR handle=8 addr=0x%x size=%u off=0x%x\n", $g8_addr, $g8_size, $g8_off
      printf "GEM_ADDR handle=9 addr=0x%x size=%u off=0x%x\n", $g9_addr, $g9_size, $g9_off
      eval "shell /usr/bin/python3 /mnt/vp/dump_nvdla_gem.py /dev/dri/renderD128 %d %u /mnt/vp/gem1.bin %d %u /mnt/vp/gem2.bin %d %u /mnt/vp/gem3.bin %d %u /mnt/vp/gem4.bin %d %u /mnt/vp/gem5.bin %d %u /mnt/vp/gem6.bin %d %u /mnt/vp/gem7.bin %d %u /mnt/vp/gem8.bin %d %u /mnt/vp/gem9.bin", $g1_fd, $g1_size, $g2_fd, $g2_size, $g3_fd, $g3_size, $g4_fd, $g4_size, $g5_fd, $g5_size, $g6_fd, $g6_size, $g7_fd, $g7_size, $g8_fd, $g8_size, $g9_fd, $g9_size
      quit
    end
  else
    set $in_ioctl = 0
    if $x0 == 0 && $ireq == 0xc0106441
      set $h = *(unsigned int *)$iarg
      set $sz = *(unsigned long long *)($iarg + 8)
      set $drm_fd = $ifd
      if $h == 1
        set $g1_size = $sz
      end
      if $h == 2
        set $g2_size = $sz
      end
      if $h == 3
        set $g3_size = $sz
      end
      if $h == 4
        set $g4_size = $sz
      end
      if $h == 5
        set $g5_size = $sz
      end
      if $h == 6
        set $g6_size = $sz
      end
      if $h == 7
        set $g7_size = $sz
      end
      if $h == 8
        set $g8_size = $sz
      end
      if $h == 9
        set $g9_size = $sz
      end
      printf "GEM_CREATE handle=%u size=%u\n", $h, $sz
    end
    if $x0 == 0 && $ireq == 0xc00c642d
      set $h = *(unsigned int *)$iarg
      set $pfd = *(int *)($iarg + 8)
      if $h == 1
        set $g1_fd = $pfd
      end
      if $h == 2
        set $g2_fd = $pfd
      end
      if $h == 3
        set $g3_fd = $pfd
      end
      if $h == 4
        set $g4_fd = $pfd
      end
      if $h == 5
        set $g5_fd = $pfd
      end
      if $h == 6
        set $g6_fd = $pfd
      end
      if $h == 7
        set $g7_fd = $pfd
      end
      if $h == 8
        set $g8_fd = $pfd
      end
      if $h == 9
        set $g9_fd = $pfd
      end
      printf "PRIME handle=%u fd=%d\n", $h, $pfd
    end
    if $x0 == 0 && $ireq == 0xc0106442
      set $h = *(unsigned int *)$iarg
      set $off = *(unsigned long long *)($iarg + 8)
      if $h == 1
        set $g1_off = $off
      end
      if $h == 2
        set $g2_off = $off
      end
      if $h == 3
        set $g3_off = $off
      end
      if $h == 4
        set $g4_off = $off
      end
      if $h == 5
        set $g5_off = $off
      end
      if $h == 6
        set $g6_off = $off
      end
      if $h == 7
        set $g7_off = $off
      end
      if $h == 8
        set $g8_off = $off
      end
      if $h == 9
        set $g9_off = $off
      end
      printf "GEM_MMAP handle=%u offset=0x%x\n", $h, $off
    end
  end
  continue
end

catch syscall 222
commands
  silent
  if $in_mmap == 0
    set $in_mmap = 1
    set $mlen = $x1
    set $mfd = $x4
    set $moff = $x5
  else
    set $in_mmap = 0
    if $x0 > 0 && $drm_fd >= 0 && $mfd == $drm_fd
      set $low = $moff & 0xffffffff
      if $low == 0x0
        set $g1_addr = $x0
      end
      if $low == 0x1000
        set $g2_addr = $x0
      end
      if $low == 0x2000
        set $g3_addr = $x0
      end
      if $low == 0x3000
        set $g4_addr = $x0
      end
      if $low == 0x4000
        set $g5_addr = $x0
      end
      if $low == 0x5000
        set $g6_addr = $x0
      end
      if $low == 0x6000
        set $g7_addr = $x0
      end
      if $low == 0x7000
        set $g8_addr = $x0
      end
      if $low == 0x8000
        set $g9_addr = $x0
      end
      printf "MMAP fd=%d addr=0x%x len=%u off=0x%x low=0x%x\n", $mfd, $x0, $mlen, $moff, $low
    end
  end
  continue
end

run --loadable /mnt/vp/test_Add.nvdla --image /mnt/vp/input1x5x7.pgm --rawdump
