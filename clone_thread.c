#define _GNU_SOURCE
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>

#define STACK_SIZE (1024 * 1024)

// Разделяемая переменная (общий счётчик)
long shared_counter = 0;
// Мьютекс для предотвращения Race Condition (используется для проверки синхронизации)
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

// Системная обёртка для получения реального TID потока в ядре Linux
pid_t gettid(void) {
    return syscall(SYS_gettid);
}

// Функция-точка входа для нового потока/процесса через clone()
int thread_func(void *arg) {
    long iterations = (long)arg;
    
    printf("[CHILD] Запущен: PID=%ld, TID=%ld\n", (long)getpid(), (long)gettid());
    printf("[CHILD] Адрес разделяемой переменной: %p\n", (void *)&shared_counter);
    
    for (long i = 0; i < iterations; i++) {
        pthread_mutex_lock(&counter_mutex);
        shared_counter++;
        pthread_mutex_unlock(&counter_mutex);
    }
    
    printf("[CHILD] Завершил инкремент счётчика.\n");
    return 0;
}

int main(int argc, char *argv[]) {
    long iterations = 10000;
    if (argc > 1) {
        iterations = atol(argv[1]);
    }

    printf("[MAIN] Стартовал: PID=%ld, TID=%ld\n", (long)getpid(), (long)gettid());
    printf("[MAIN] Исходное значение счётчика: %ld, адрес: %p\n", shared_counter, (void *)&shared_counter);

    // Шаг 1.1: Выделение стека для дочерней задачи вручную
    char *stack = malloc(STACK_SIZE);
    if (!stack) {
        perror("malloc");
        return EXIT_FAILURE;
    }
    // На x86-64 стек растёт сверху вниз, передаём указатель на конец выделенной области
    char *stack_top = stack + STACK_SIZE;

    // Шаг 1.2: Вызов clone() с флагами, полностью дублирующими поведение pthread
    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | SIGCHLD;
    
    printf("[MAIN] Вызов clone() с флагами потока...\n");
    pid_t tid = clone(thread_func, stack_top, flags, (void *)iterations);
    
    if (tid == -1) {
        perror("clone");
        free(stack);
        return EXIT_FAILURE;
    }

    // Демонстрация разделения памяти (Шаг 1.4)
    // Ждём, пока поток выполнит работу (так как CLONE_THREAD запрещает waitpid() на этот TID)
    printf("[MAIN] Ожидание выполнения дочернего потока через sleep...\n");
    sleep(1); 

    pthread_mutex_lock(&counter_mutex);
    printf("[MAIN] Финальное значение счётчика: %ld (Ожидалось: %ld)\n", shared_counter, iterations);
    pthread_mutex_unlock(&counter_mutex);

    free(stack);
    return EXIT_SUCCESS;
}
