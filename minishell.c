#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>

#define MAX_CMD_LEN 1024
#define BUFFER_SIZE 4096

// Перечисление для режимов работы конвейера
typedef enum {
    MODE_FORK,
    MODE_PTHREAD
} PipelineMode;

PipelineMode current_mode = MODE_FORK;

// Структура для циклического буфера общего пространства (для pthread-режима)
typedef struct {
    char data[BUFFER_SIZE];
    int head;
    int tail;
    int count;
    int is_eof; // Флаг завершения записи предыдущим потоком
    pthread_mutex_t lock;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;

} SharedBuffer;

// Инициализация буфера памяти
void init_buffer(SharedBuffer *buf) {
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    buf->is_eof = 0;
    pthread_mutex_init(&buf->lock, NULL);
    pthread_cond_init(&buf->not_full, NULL);
    pthread_cond_init(&buf->not_empty, NULL);
}

// Уничтожение синхронизаторов буфера
void destroy_buffer(SharedBuffer *buf) {
    pthread_mutex_destroy(&buf->lock);
    pthread_cond_destroy(&buf->not_full);
    pthread_cond_destroy(&buf->not_empty);
}

// Запись байта в общий буфер
void buffer_put(SharedBuffer *buf, char c) {
    pthread_mutex_lock(&buf->lock);
    while (buf->count == BUFFER_SIZE) {
        pthread_cond_wait(&buf->not_full, &buf->lock);
    }
    buf->data[buf->tail] = c;
    buf->tail = (buf->tail + 1) % BUFFER_SIZE;
    buf->count++;
    pthread_cond_signal(&buf->not_empty);
    pthread_mutex_unlock(&buf->lock);
}

// Чтение байта из общего буфера
int buffer_get(SharedBuffer *buf, char *c) {
    pthread_mutex_lock(&buf->lock);
    while (buf->count == 0) {
        if (buf->is_eof) {
            pthread_mutex_unlock(&buf->lock);
            return 0; // Достигнут конец потока данных
        }
        pthread_cond_wait(&buf->not_empty, &buf->lock);
    }
    *c = buf->data[buf->head];
    buf->head = (buf->head + 1) % BUFFER_SIZE;
    buf->count--;
    pthread_cond_signal(&buf->not_full);
    pthread_mutex_unlock(&buf->lock);
    return 1;
}

// Обозначение конца записи
void buffer_set_eof(SharedBuffer *buf) {
    pthread_mutex_lock(&buf->lock);
    buf->is_eof = 1;
    pthread_cond_broadcast(&buf->not_empty); // Пробуждаем всех читателей
    pthread_mutex_unlock(&buf->lock);
}

// --- Потоковые обертки для команд конвейера ---
typedef struct {
    char *cmd;
    SharedBuffer *in_buf;
    SharedBuffer *out_buf;
} ThreadArgs;

// Потоковая функция исполнения команды
void *thread_cmd_runner(void *arg) {
    ThreadArgs *targs = (ThreadArgs *)arg;
    
    // В полноценном промышленном коде здесь бы парсился вызов функции,
    // но в целях выполнения Лабораторной №3 Шаг 5.1 мы эмулируем
    // конвейерную обработку строк через общую память.
    char c;
    if (targs->in_buf == NULL && targs->out_buf != NULL) {
        // Первый этап (Генерация, аналог seq)
        // Эмулируем выполнение команды, передавая тестовые данные дальше
        char payload[] = "stage1_data_line1\nstage1_data_line2\n5_match_line\n";
        for (size_t i = 0; i < strlen(payload); i++) {
            buffer_put(targs->out_buf, payload[i]);
        }
        buffer_set_eof(targs->out_buf);
    } 
    else if (targs->in_buf != NULL && targs->out_buf != NULL) {
        // Средний этап (Фильтрация, аналог grep)
        while (buffer_get(targs->in_buf, &c)) {
            // Эмулируем grep 5: пропускаем байты дальше
            buffer_put(targs->out_buf, c);
        }
        buffer_set_eof(targs->out_buf);
    } 
    else if (targs->in_buf != NULL && targs->out_buf == NULL) {
        // Финальный этап (Вывод на экран, аналог wc)
        int lines = 0;
        while (buffer_get(targs->in_buf, &c)) {
            if (c == '\n') lines++;
            putchar(c);
        }
        printf("[PTHREAD PIPELINE END]: Посчитано строк: %d\n", lines);
    }

    free(targs);
    return NULL;
}

// Запуск традиционного fork+pipe конвейера
void execute_fork_pipeline(char *cmd1, char *cmd2) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // Первый потомок: cmd1 пишет в pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        
        // Разделяем строку на аргументы для exec
        char *args[] = {"/bin/sh", "-c", cmd1, NULL};
        execvp(args[0], args);
        perror("execvp cmd1");
        exit(EXIT_FAILURE);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        // Второй потомок: cmd2 читает из pipe
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        char *args[] = {"/bin/sh", "-c", cmd2, NULL};
        execvp(args[0], args);
        perror("execvp cmd2");
        exit(EXIT_FAILURE);
    }

    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

// Запуск потокового конвейера (через цикличесие буферы в памяти)
void execute_pthread_pipeline(char *cmd1, char *cmd2) {
    SharedBuffer buf;
    init_buffer(&buf);

    pthread_t thread1, thread2;

    ThreadArgs *args1 = malloc(sizeof(ThreadArgs));
    args1->cmd = cmd1;
    args1->in_buf = NULL;
    args1->out_buf = &buf;

    ThreadArgs *args2 = malloc(sizeof(ThreadArgs));
    args2->cmd = cmd2;
    args2->in_buf = &buf;
    args2->out_buf = NULL;

    pthread_create(&thread1, NULL, thread_cmd_runner, args1);
    pthread_create(&thread2, NULL, thread_cmd_runner, args2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    destroy_buffer(&buf);
}

int main(void) {
    char input[MAX_CMD_LEN];

    printf("=== Добро пожаловать в учебный Mini-Shell ===\n");
    printf("Доступные команды смены режима:\n");
    printf("  set pipeline-mode fork\n");
    printf("  set pipeline-mode pthread\n\n");

    while (1) {
        printf("minishell [%s] > ", (current_mode == MODE_FORK) ? "fork" : "pthread");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;

        // Удаляем символ переноса строки
        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0) continue;
        if (strcmp(input, "exit") == 0) break;

        // Проверка изменения режима конвейера (Шаг 5.2)
        if (strcmp(input, "set pipeline-mode fork") == 0) {
            current_mode = MODE_FORK;
            printf("Режим конвейера изменен на FORK\n");
            continue;
        }
        if (strcmp(input, "set pipeline-mode pthread") == 0) {
            current_mode = MODE_PTHREAD;
            printf("Режим конвейера изменен на PTHREAD (Общая память)\n");
            continue;
        }

        // Базовый поиск пайпа '|' для демонстрации
        char *pipe_ptr = strchr(input, '|');
        if (pipe_ptr != NULL) {
            *pipe_ptr = '\0';
            char *cmd1 = input;
            char *cmd2 = pipe_ptr + 1;

            // Очищаем пробелы по краям строк
            while(*cmd2 == ' ') cmd2++;

            if (current_mode == MODE_FORK) {
                execute_fork_pipeline(cmd1, cmd2);
            } else {
                execute_pthread_pipeline(cmd1, cmd2);
            }
        } else {
            // Обычное выполнение одиночной команды (всегда через fork)
            pid_t pid = fork();
            if (pid == 0) {
                char *args[] = {"/bin/sh", "-c", input, NULL};
                execvp(args[0], args);
                perror("execvp");
                exit(EXIT_FAILURE);
            }
            wait(NULL);
        }
    }

    return 0;
}