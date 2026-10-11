# Определяем компилятор и флаги
CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -Wpedantic -O2 -pthread

# Объекты для сборки пространства пользователя
TARGETS = clone_thread bench minishell

# Объекты для сборки модуля ядра Linux
obj-m += clone_probe.o
KDIR ?= /lib/modules/$(shell uname -r)/build

all: $(TARGETS) kernel_module

clone_thread: clone_thread.c
	$(CC) $(CFLAGS) clone_thread.c -o clone_thread

bench: bench.c
	$(CC) $(CFLAGS) bench.c -o bench

minishell: minishell.c
	$(CC) $(CFLAGS) minishell.c -o minishell

kernel_module:
	make -C $(KDIR) M=$(PWD) modules

clean:
	rm -f $(TARGETS)
	make -C $(KDIR) M=$(PWD) clean

.PHONY: all kernel_module clean