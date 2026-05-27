#!/bin/sh
# Batch runner for nv_small tests - runs inside QEMU on VP
TESTS_DIR="/mnt/nv_small_tests"
RESULTS="/mnt/nv_small_results.log"
RUN_LOAD_SCRIPTS="${NVDLA_RUN_LOAD_SCRIPTS:-0}"
LOAD_KMODS="${NVDLA_LOAD_KMODS:-0}"

echo "NVDLA nv_small test runner" > $RESULTS
date >> $RESULTS
echo "load_scripts=$RUN_LOAD_SCRIPTS" >> $RESULTS
echo "load_kmods=$LOAD_KMODS" >> $RESULTS
echo "----------------------------------------" >> $RESULTS

if [ "$LOAD_KMODS" = "1" ]; then
  insmod /mnt/drm.ko 2>&1
  if [ -f /mnt/opendla_small.ko ]; then
    insmod /mnt/opendla_small.ko 2>&1
  else
    insmod /mnt/opendla.ko 2>&1
  fi
  sleep 1
fi

cd $TESTS_DIR
PASS=0; FAIL=0; TIMEOUT=0; TOTAL=0

for test_dir in */; do
  test_name="${test_dir%/}"
  test_bin="${test_name}_test"

  if [ ! -f "${test_name}/${test_bin}" ]; then
    continue
  fi
  
  TOTAL=$((TOTAL + 1))
  printf "%-50s " "${test_name}" | tee -a $RESULTS
  
  # Current generated nv_small_tests binaries embed .dat payloads and load them
  # through /dev/mem mmap. Keep devmem scripts only for legacy comparison.
  if [ "$RUN_LOAD_SCRIPTS" = "1" ]; then
    for script in ${test_name}/*_load.sh; do
      [ -f "$script" ] && sh $script 2>/dev/null
    done
  fi
  
  # Run test with timeout
  (cd ${test_name} && timeout 30 ./${test_bin} 2>&1) > /tmp/test_out.txt
  rc=$?
  
  if grep -q FAIL /tmp/test_out.txt; then
    echo "FAIL" | tee -a $RESULTS
    FAIL=$((FAIL + 1))
  elif [ $rc -eq 0 ]; then
    echo "PASS" | tee -a $RESULTS
    PASS=$((PASS + 1))
  elif [ $rc -eq 124 ]; then
    echo "TIMEOUT" | tee -a $RESULTS
    TIMEOUT=$((TIMEOUT + 1))
  else
    echo "FAIL" | tee -a $RESULTS
    FAIL=$((FAIL + 1))
  fi
  
  cat /tmp/test_out.txt >> $RESULTS
  echo "" >> $RESULTS
done

echo "----------------------------------------" >> $RESULTS
echo "PASS=$PASS FAIL=$FAIL TIMEOUT=$TIMEOUT TOTAL=$TOTAL" | tee -a $RESULTS
date >> $RESULTS
poweroff
