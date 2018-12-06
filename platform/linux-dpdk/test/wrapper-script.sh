#!/bin/bash

export ODP_PLATFORM_PARAMS=${ODP_PLATFORM_PARAMS:---vdev="crypto_openssl" --vdev="crypto_null"}
# where to mount huge pages
export HUGEPAGEDIR=${HUGEPAGEDIR:-/mnt/huge}
# exit codes expected by automake for skipped tests
TEST_SKIPPED=77

# Make sure huge pages are released when a unit test crashes "make check"
trap ctrl_c INT

ctrl_c() {
	echo "** Trapped CTRL-C"
	if grep -qs "$HUGEPAGEDIR" /proc/mounts; then
		echo "** Umounting hugetlbfs"
		sleep 1 && sudo umount -a -t hugetlbfs
	fi
}

function mount_and_reserve() {
	echo 2000 > /proc/sys/vm/nr_hugepages
	cat /proc/sys/vm/nr_hugepages

	export PATH_NR="/sys/devices/system/node/node0/hugepages/hugepages-${SIZE_KB}kB/nr_hugepages"
	export PATH_FREE="/sys/devices/system/node/node0/hugepages/hugepages-${SIZE_KB}kB/free_hugepages"
	if grep -qs "$HUGEPAGEDIR" /proc/mounts; then
		echo "Umounting hugetlbfs from previous use!"
		sudo umount -a -t hugetlbfs
	fi
	echo "Trying $SIZE pages"
	sudo mount -t hugetlbfs -o pagesize=$SIZE nodev $HUGEPAGEDIR 2>/dev/null
	res=$?
	if [ $res -ne 0 ]; then
		echo "ERROR: can't mount hugepages"
		return $res
	fi
	sudo sh -c "echo $RESERVE > $PATH_NR"
	if [ `cat $PATH_NR` -lt 1 ]; then
		echo "Failed to reserve at least 1 huge page!"
		return 1
	else
		echo "Total number: `cat $PATH_NR`"
		echo "Free pages: `cat $PATH_FREE`"
		mount
		cat /proc/meminfo |grep Huge
	fi
}

if [ ! -d $HUGEPAGEDIR ]; then
	sudo mkdir -p $HUGEPAGEDIR
fi

# Need to be root to use DPDK
if [ "$(id -u)" != "0" ]; then
	echo "DPDK needs root privileges"
	exit $TEST_SKIPPED
fi

echo "Mounting hugetlbfs"
export SIZE=2MB
export SIZE_KB=2048
export RESERVE=1024
mount_and_reserve
res=$?
if [ $res -ne 0 ]; then
	echo "ERROR: can't mount hugepages"
	exit $res
fi
echo "running $1!"
$1
res=$?
echo "Unmounting hugetlbfs"
sleep 0.3 && sudo umount -a -t hugetlbfs
exit $res

