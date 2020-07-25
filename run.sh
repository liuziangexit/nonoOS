set -e
./build.sh
qemu-system-i386 -d int -no-reboot -no-shutdown -cdrom nonoOS.iso
#qemu-system-i386 -cdrom nonoOS.iso
