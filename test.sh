#!/bin/bash

# 1. Проверка разделения памяти
gcc clone_thread.c -o clone_thread
./clone_thread

# 2. Проверка бенчмарка через perf stat (по 5 запусков)
gcc bench.c -o bench -pthread

perf stat -r 5 -e task-clock,context-switches ./bench fork 1000
perf stat -r 5 -e task-clock,context-switches ./bench pthread 1000
perf stat -r 5 -e task-clock,context-switches ./bench clone 1000

# 3. Тест скорости конвейера в мини-шел itsel
gcc minishell.c -o minishell -pthread
# Передаем команды внутрь minishell автоматически и меряем время
echo -e "set pipeline-mode pthread\nexit" | perf stat -e task-clock ./minishell
echo -e "set pipeline-mode fork\nexit" | perf stat -e task-clock ./minishell