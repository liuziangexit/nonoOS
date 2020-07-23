#!/bin/sh
set -e

make

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp src/kernel/nonoOS.kernel isodir/boot/nonoOS.kernel
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "nonoOS" {
	multiboot /boot/nonoOS.kernel
}
EOF
i386-elf-grub-mkrescue -o nonoOS.iso isodir
