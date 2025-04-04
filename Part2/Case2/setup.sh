#!/bin/bash

echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

# Create the mount point for hugetlbfs and mount it
echo "Creating mount point for hugetlbfs..."
mkdir -p /mnt/huge
mount -t hugetlbfs nodev /mnt/huge

# Load necessary kernel modules
echo "Loading uio and igb_uio kernel modules..."
modprobe uio
insmod /data/f-stack/dpdk/build/kernel/linux/igb_uio/igb_uio.ko

# Load the rte_kni kernel module with carrier=on option
echo "Loading rte_kni kernel module with carrier=on..."
insmod /data/f-stack/dpdk/build/kernel/linux/kni/rte_kni.ko carrier=on

# Check DPDK device status
echo "Checking DPDK device bindings..."
python3 /data/f-stack/dpdk/usertools/dpdk-devbind.py --status

# Bring down enp2s0 interface and bind to igb_uio
echo "Bringing down enp2s0 and binding to igb_uio..."
ifconfig enp2s0 down
python3 /data/f-stack/dpdk/usertools/dpdk-devbind.py --bind=igb_uio enp2s0

echo "Setup completed successfully!"
