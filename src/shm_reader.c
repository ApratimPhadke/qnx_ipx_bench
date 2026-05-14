#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>

#define MAX_PAYLOAD_BYTES 16384
#define SHM_SIZE (MAX_PAYLOAD_BYTES + 64)
#define SHM_NAME "/bench_shm"
#define SEM_WRITE "/bench_sem_w"
#define SEM_READ "/bench_sem_r"
#define NUM_ITERATIONS 10000
#define WARMUP_ITERS 200

typedef struct
{
    uint32_t seq;
    uint32_t payload_size;
    uint32_t stop;
    char data[MAX_PAYLOAD_BYTES];
} SharedBlock;

int main(void)
{
    int shm_fd;
    SharedBlock *shm;
    sem_t *sem_w, *sem_r;
    int total_rounds = 0;

    shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
    if (shm_fd == -1)
    {
        perror("shm_open (reader) — start shm_writer first");
        return EXIT_FAILURE;
    }

    shm = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    sem_w = sem_open(SEM_WRITE, 0);
    sem_r = sem_open(SEM_READ, 0);

    if (shm == MAP_FAILED || sem_w == SEM_FAILED || sem_r == SEM_FAILED)
    {
        perror("mmap/sem_open");
        return EXIT_FAILURE;
    }

    printf("[shm_reader] Ready, waiting for writer...\n");
    fflush(stdout);

    while (1)
    {
        sem_wait(sem_w);

        if (shm->stop)
            break;

        volatile char *p = shm->data;
        volatile char dummy = p[0];
        if (shm->payload_size > 1)
            dummy = p[shm->payload_size - 1];
        (void)dummy;

        sem_post(sem_r);
        total_rounds++;
    }

    printf("[shm_reader] Done. Processed %d rounds.\n", total_rounds);

    munmap(shm, SHM_SIZE);
    sem_close(sem_w);
    sem_close(sem_r);
    return EXIT_SUCCESS;
}