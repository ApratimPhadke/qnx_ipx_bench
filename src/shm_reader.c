#include <stdio.h>
#include <stdlib.h>
#include<stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <time.h>

#define SHM_NAME  "/bench_shm"
#define SEM_WRITE "/bench_sem_w"
#define SEM_READ  "/bench_sem_r"
#define SHM_SIZE  4096
#define NUM_ITERS 10000
#define WARMUP_ITERS 200

typedef struct {
    uint32_t seq;
    char     data[64];
    uint64_t write_ns;
} SharedBlock;

int main(void)
{
    int          shm_fd;
    SharedBlock *shm;
    sem_t       *sem_w, *sem_r;

    /* Open shared memory and semaphores (writer creates them) */
    shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
    if (shm_fd == -1) { perror("shm_open (reader)"); return EXIT_FAILURE; }

    shm = mmap(NULL, SHM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    sem_w = sem_open(SEM_WRITE, 0);
    sem_r = sem_open(SEM_READ, 0);

    printf("[reader] Waiting for writer...\n");

    /* Warmup: mirror the writer's warmup loop */
    for (int i = 0; i < WARMUP_ITERS; i++) {
        sem_wait(sem_w);    /* wait: writer signals data ready */
        volatile uint32_t dummy = shm->seq;  /* actually read it */
        (void)dummy;
        sem_post(sem_r);    /* signal: I read it */
    }

    for (int i = 0; i < NUM_ITERS + 1; i++) {
        sem_wait(sem_w);
        if (shm->seq == 0xFFFFFFFF) break;  /* stop sentinel */
        volatile char *p = shm->data;       /* force actual read */
        (void)p;
        sem_post(sem_r);
    }

    printf("[reader] Done.\n");
    munmap(shm, SHM_SIZE);
    return EXIT_SUCCESS;
}