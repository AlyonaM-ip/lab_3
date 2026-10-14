#!/bin/bash

echo "=== 1. Компиляция программ (статически для QEMU) ==="
gcc -std=gnu11 -Wall -Wextra -O2 -static clone_thread.c -o clone_thread -pthread
gcc -std=gnu11 -Wall -Wextra -O2 -static bench.c -o bench -pthread
gcc -std=gnu11 -Wall -Wextra -O2 -static minishell.c -o minishell -pthread

echo "=== 2. Сборка модуля ядра clone_probe.ko ==="
# Полностью очищаем старые файлы, используя исходники кастомного ядра
make -C /home/yona/linux-4.19.322 M=$PWD clean

# Собираем модуль строго под ядро 4.19.322
make -C /home/yona/linux-4.19.322 M=$PWD modules

echo "=== 3. Копирование всего в папку initramfs ==="
mkdir -p ~/initramfs/root
cp clone_thread bench minishell clone_probe.ko ~/initramfs/root/

echo "=== 4. Упаковка файловой системы в образ initramfs.img ==="
cd ~/initramfs
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ~/initramfs.img
cd -

echo "=== 5. ЗАПУСК QEMU ==="
qemu-system-x86_64 \
  -kernel /home/yona/linux-4.19.322/arch/x86/boot/bzImage \
  -initrd /home/yona/initramfs.img \
  -append "console=ttyS0" \
  -nographic