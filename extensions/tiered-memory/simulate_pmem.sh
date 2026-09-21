#!/bin/bash
# Simulate NVM (PMEM) using a loopback file and benchmark using dd

set -e

FILE=~/fake_pmem/fake_pmem.img
MOUNT_POINT=/mnt/fakepmem
LOOP_DEV=""

echo "Creating 1GB file to simulate NVM..."
mkdir -p ~/fake_pmem
fallocate -l 1G $FILE

echo "Setting up loop device..."
LOOP_DEV=$(sudo losetup -f --show $FILE)

echo "Formatting as ext4..."
sudo mkfs.ext4 $LOOP_DEV

echo "Mounting to $MOUNT_POINT..."
sudo mkdir -p $MOUNT_POINT
sudo mount $LOOP_DEV $MOUNT_POINT

echo "Running write benchmark using dd..."
sudo dd if=/dev/zero of=$MOUNT_POINT/testfile bs=4k count=256000

echo "Cleaning up..."
sudo umount $MOUNT_POINT
sudo losetup -d $LOOP_DEV
rm -rf ~/fake_pmem

echo "Simulation complete."
