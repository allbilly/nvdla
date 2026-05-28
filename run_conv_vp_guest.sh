#!/bin/bash
set -e

VP_PID=2311657
REPO_ROOT="/home/fedora/nvdla"
GUEST_USER="root"
GUEST_PASS="nvdla"
TEST_SCRIPT="/home/fedora/nvdla/examples/conv.py"

wait_for_boot() {
    echo "Waiting for VP guest to boot..."
    for i in {1..60}; do
        if lsof -P -u $VP_PID >/dev/null 2>&1; then
            echo "VP guest process found (PID $VP_PID)"
            return 0
        fi
        sleep 2
        if [ $i -eq 60 ]; then
            echo "ERROR: VP guest failed to boot within 2 minutes"
            return 1
        fi
    done
}

mount_repo() {
    echo "Mounting repository via 9p..."
    sudo mkdir -p /mnt/nvdla
    sudo mount -t 9p $REPO_ROOT /mnt/nvdla -o trans,uid=0,gid=0,version=9p2000a 2>/dev/null || \
    sudo mount -t 9p $REPO_ROOT /mnt/nvdla -o trans,uid=0,gid=0,version=9p2000 2>/dev/null || \
    sudo mount -t 9p $REPO_ROOT /mnt/nvdla -o trans,uid=0,gid=0 2>/dev/null
    if [ -d /mnt/nvdla/ref/vp/nv_small_tests ]; then
        echo "Repository mounted successfully at /mnt/nvdla"
    else
        echo "WARNING: Repository mount may have issues"
    fi
}

run_test_in_guest() {
    echo "Logging in and running conv.py test..."
    sudo -i -u ${GUEST_USER} \
        bash -c "cd /mnt/nvdla/examples && python3 ${TEST_SCRIPT} --list"
}

main() {
    echo "=========================================="
    echo "NVDLA VP Guest Test Runner"
    echo "=========================================="
    
    if ! wait_for_boot; then
        echo "ERROR: Cannot proceed - VP guest not running"
        exit 1
    fi
    
    if ! mount_repo; then
        echo "ERROR: Failed to mount repository"
        exit 1
    fi
    
    run_test_in_guest
    echo "=========================================="
    echo "Test completed"
    echo "=========================================="
}

main "$@"