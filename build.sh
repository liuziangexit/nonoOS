mkdir -p build
i686-elf-as src/boot/boot.s -o build/boot.o
i686-elf-gcc -c src/boot/kentry.c -o build/kentry.o -std=c11 -ffreestanding -O2 -Wall -Wextra
i686-elf-gcc -T src/boot/linker.ld -o build/nonoOS.bin -ffreestanding -O2 -nostdlib build/boot.o build/kentry.o -lgcc

if i386-elf-grub-file --is-x86-multiboot build/nonoOS.bin; then
  echo multiboot confirmed
else
  echo bad file
fi

rm -rf build/image

mkdir build/image
mkdir build/image/boot
mkdir build/image/boot/grub

cp -f build/nonoOS.bin build/image
cp -f src/boot/grub.cfg build/image/boot/grub

i386-elf-grub-mkrescue -o build/nonoOS.iso build/image
