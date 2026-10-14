# Переименовали переменную, чтобы не злить сборщик ядра Linux
CC = gcc
USER_CFLAGS = -std=gnu11 -Wall -Wextra -Wpedantic -O2 -pthread

# Список программ для сборки
TARGETS = clone_thread bench minishell

# Настройки для сборки модуля ядра
obj-m += clone_probe.o
KDIR = /home/yona/linux-4.19.322

all: $(TARGETS) kernel_module

# Используем USER_CFLAGS вместо CFLAGS
clone_thread: clone_thread.c
	$(CC) $(USER_CFLAGS) clone_thread.c -o clone_thread

bench: bench.c
	$(CC) $(USER_CFLAGS) bench.c -o bench

minishell: minishell.c
	$(CC) $(USER_CFLAGS) minishell.c -o minishell

kernel_module:
	make -C $(KDIR) M=$(PWD) modules

clean:
	rm -f $(TARGETS)
	make -C $(KDIR) M=$(PWD) clean

.PHONY: all kernel_module clean
