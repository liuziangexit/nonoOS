set -e

source ./clean.sh
source ./build.sh

CPU=1
MEM=386

qemu-system-i386 -d int -no-reboot -no-shutdown -serial mon:stdio\
  -drive file=../fs.img,index=1,media=disk,format=raw -drive file=../nonoOS.img,index=0,media=disk,format=raw -smp $CPU -m $MEM
