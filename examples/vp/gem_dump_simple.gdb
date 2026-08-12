set pagination off
set confirm off
set breakpoint pending on
set environment LD_LIBRARY_PATH /mnt/vp
set logging file /mnt/vp/gdb_gem_dump.log
set logging overwrite on
set logging on

set $drm_fd = -1
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
set $g2_off = 0
set $g3_off = 0
set $g4_off = 0
set $g5_off = 0
set $g6_off = 0
set $g7_off = 0
set $g8_off = 0
set $g9_off = 0
set $g1_addr = 0
set $g2_addr = 0
set $g3_addr = 0
set $g4_addr = 0
set $g5_addr = 0
set $g6_addr = 0
set $g7_addr = 0
set $g8_addr = 0
set $g9_addr = 0

start --loadable /mnt/vp/test_Add.nvdla --image /mnt/vp/input1x5x7.pgm --rawdump

break ioctl
commands
  silent
  set $fd = $x0
  set $req = $x1 & 0xffffffff
  set $arg = $x2
  finish
  if $req == 0xc0106441 && $x0 == 0
    set $h = *(unsigned int *)$arg
    set $sz = *(unsigned long long *)($arg + 8)
    set $drm_fd = $fd
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
    printf "GEM_CREATE handle=%u size=%llu\n", $h, $sz
  end
  if $req == 0xc0106442 && $x0 == 0
    set $h = *(unsigned int *)$arg
    set $off = *(unsigned long long *)($arg + 8)
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
    printf "GEM_MMAP handle=%u offset=0x%llx\n", $h, $off
  end
  if $req == 0xc0106440
    printf "SUBMIT_BREAK\n"
    printf "GEM_ADDR handle=1 addr=0x%llx size=%llu off=0x%llx\n", $g1_addr, $g1_size, $g1_off
    printf "GEM_ADDR handle=2 addr=0x%llx size=%llu off=0x%llx\n", $g2_addr, $g2_size, $g2_off
    printf "GEM_ADDR handle=3 addr=0x%llx size=%llu off=0x%llx\n", $g3_addr, $g3_size, $g3_off
    printf "GEM_ADDR handle=4 addr=0x%llx size=%llu off=0x%llx\n", $g4_addr, $g4_size, $g4_off
    printf "GEM_ADDR handle=5 addr=0x%llx size=%llu off=0x%llx\n", $g5_addr, $g5_size, $g5_off
    printf "GEM_ADDR handle=6 addr=0x%llx size=%llu off=0x%llx\n", $g6_addr, $g6_size, $g6_off
    printf "GEM_ADDR handle=7 addr=0x%llx size=%llu off=0x%llx\n", $g7_addr, $g7_size, $g7_off
    printf "GEM_ADDR handle=8 addr=0x%llx size=%llu off=0x%llx\n", $g8_addr, $g8_size, $g8_off
    printf "GEM_ADDR handle=9 addr=0x%llx size=%llu off=0x%llx\n", $g9_addr, $g9_size, $g9_off
    if $g1_addr != 0
      dump binary memory /mnt/vp/gem1.bin $g1_addr ($g1_addr + $g1_size)
    end
    if $g2_addr != 0
      dump binary memory /mnt/vp/gem2.bin $g2_addr ($g2_addr + $g2_size)
    end
    if $g3_addr != 0
      dump binary memory /mnt/vp/gem3.bin $g3_addr ($g3_addr + $g3_size)
    end
    if $g4_addr != 0
      dump binary memory /mnt/vp/gem4.bin $g4_addr ($g4_addr + $g4_size)
    end
    if $g5_addr != 0
      dump binary memory /mnt/vp/gem5.bin $g5_addr ($g5_addr + $g5_size)
    end
    if $g6_addr != 0
      dump binary memory /mnt/vp/gem6.bin $g6_addr ($g6_addr + $g6_size)
    end
    if $g7_addr != 0
      dump binary memory /mnt/vp/gem7.bin $g7_addr ($g7_addr + $g7_size)
    end
    if $g8_addr != 0
      dump binary memory /mnt/vp/gem8.bin $g8_addr ($g8_addr + $g8_size)
    end
    if $g9_addr != 0
      dump binary memory /mnt/vp/gem9.bin $g9_addr ($g9_addr + $g9_size)
    end
    quit
  end
  continue
end

break __mmap
commands
  silent
  set $mlen = $x1
  set $mfd = $x4
  set $moff = $x5
  finish
  if $x0 > 0 && $mfd == $drm_fd
    set $low = $moff & 0xffffffff
    if $low == $g1_off
      set $g1_addr = $x0
    end
    if $low == $g2_off
      set $g2_addr = $x0
    end
    if $low == $g3_off
      set $g3_addr = $x0
    end
    if $low == $g4_off
      set $g4_addr = $x0
    end
    if $low == $g5_off
      set $g5_addr = $x0
    end
    if $low == $g6_off
      set $g6_addr = $x0
    end
    if $low == $g7_off
      set $g7_addr = $x0
    end
    if $low == $g8_off
      set $g8_addr = $x0
    end
    if $low == $g9_off
      set $g9_addr = $x0
    end
    printf "MMAP fd=%lld addr=0x%llx len=%llu off=0x%llx low=0x%llx\n", $mfd, $x0, $mlen, $moff, $low
  end
  continue
end

continue
