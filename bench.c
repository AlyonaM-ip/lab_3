#define _GNU_SOURCE
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <pthread.h>

#define STACK_SIZE (64 * 1024) // Оптимальный стек для легковесного теста

long global_counter = 0;
pthread_mutex_t bench_mutex = PTHREAD_MUTEX_INITIALIZER;

// Минимальная полезная нагрузка потока/процесса (Шаг 2.2)
void do_work(void) {
    for (volatile int i = 0; i < 10000; i++) {
        // Минимальный инкремент без блокировки для чистоты замера context-switch
    }
}

// Стартовая функция для clone
int clone_worker(void *arg) {
    do_work();
    return 0;
}

// Стартовая функция для pthread
void* pthread_worker(void *arg) {
    do_work();
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Использование: %s <fork|pthread|clone> <число_задач>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *mode = argv[1];
    int n = atoi(argv[2]);

    if (strcmp(mode, "fork") == 0) {
        for (int i = 0; i < n; i++) {
            pid_t pid = fork();
            if (pid == 0) {
                do_work();
                exit(EXIT_SUCCESS);
            } else if (pid < 0) {
                perror("fork");
                return EXIT_FAILURE;
            }
        }
        // Ожидание всех дочерних процессов
        while (wait(NULL) > 0);

    } else if (strcmp(mode, "pthread") == 0) {
        pthread_t *threads = malloc(n * sizeof(pthread_t));
        for (int i = 0; i < n; i++) {
            if (pthread_create(&threads[i], NULL, pthread_worker, NULL) != 0) {
                perror("pthread_create");
                return EXIT_FAILURE;
            }
        }
        for (int i = 0; i < n; i++) {
            pthread_join(threads[i], NULL);
        }
        free(threads);

    } else if (strcmp(mode, "clone") == 0) {
        // Выделяем массив стеков, чтобы избежать race за память стека
        char **stacks = malloc(n * sizeof(char*));
        pid_t *tids = malloc(n * sizeof(pid_t));
        int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND; 
        // Примечание: исключаем CLONE_THREAD, чтобы иметь возможность ждать каждый TID через waitpid()

        for (int i = 0; i < n; i++) {
            stacks[i] = malloc(STACK_SIZE);
            if (!stacks[i]) {
                perror("malloc stack");
                return EXIT_FAILURE;
            }
            tids[i] = clone(clone_worker, stacks[i] + STACK_SIZE, flags | SIGCHLD, NULL);
            if (tids[i] == -1) {
                perror("clone");
                return EXIT_FAILURE;
            }
        }

        for (int i = 0; i < n; i++) {
            waitpid(tids[i], NULL, 0);
            free(stacks[i]);
        }
        free(stacks);
        free(tids);

    } else {
        fprintf(stderr, "Неизвестный режим: %s\n", mode);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
