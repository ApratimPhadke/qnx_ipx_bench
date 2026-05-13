#include<stdio.h>
#include<stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>   /* shm_open, mmap */
#include <semaphore.h>  /* sem_open, sem_post, sem_wait */
#include <time.h>

#define SHM_NAME     "/bench_shm"
#define SEM_WRITE    "/bench_sem_w"   /* writer signals reader: "data ready" */
#define SEM_READ     "/bench_sem_r"   /* reader signals writer: "I read it" */
#define SHM_SIZE     4096
#define NUM_ITERS    10000
#define WARMUP_ITERS 200

typedef struct {
    uint32_t seq;
    char     data[64];
    uint64_t write_ns;   /* timestamp when writer wrote */
} SharedBlock;

static inline uint64_t get_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main(){
    int          shm_fd;
    SharedBlock *shm;
    sem_t       *sem_w, *sem_r;
    uint64_t     latencies[NUM_ITERS];
    uint64_t     t_start;
    FILE        *fp;

    /* Create shared memory region.*/
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) { perror("shm_open"); return EXIT_FAILURE; }
    ftruncate(shm_fd, SHM_SIZE);

    /* Map it into our address space.*/
    shm = mmap(NULL, SHM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); return EXIT_FAILURE; }

     /* Create semaphores. Initial value=0 means "not signaled yet". */
    sem_unlink(SEM_WRITE); sem_unlink(SEM_READ);  /* clean up old ones */
    sem_w = sem_open(SEM_WRITE, O_CREAT, 0666, 0);
    sem_r = sem_open(SEM_READ,  O_CREAT, 0666, 0);
    if (sem_w == SEM_FAILED || sem_r == SEM_FAILED) {
        perror("sem_open"); return EXIT_FAILURE;
    }

    printf("[writer] Ready. Starting benchmark...\n");

    for (int i = 0; i < WARMUP_ITERS; i++) {
        shm->seq = i;
        sem_post(sem_w);    /* signal reader: data is ready */
        sem_wait(sem_r);    /* wait for reader to acknowledge */
    }

    for (int i = 0; i < NUM_ITERS; i++) {
        shm->seq = i;
        strncpy(shm->data, "SHM_PAYLOAD", sizeof(shm->data));

        t_start = get_ns();
        shm->write_ns = t_start;

        sem_post(sem_w);    /* "I wrote, you can read now" */
        sem_wait(sem_r);    /* wait: "OK I read it" */

        latencies[i] = get_ns() - t_start;
    }

    /* Signal reader to stop (use seq = 0xFFFFFFFF as sentinel) */
    shm->seq = 0xFFFFFFFF;
    sem_post(sem_w);

    fp = fopen("/tmp/shm_results.csv", "w");
    fprintf(fp, "iteration,latency_ns\n");
    uint64_t sum=0, mn=latencies[0], mx=latencies[0];
    for (int i = 0; i < NUM_ITERS; i++) {
        fprintf(fp, "%d,%llu\n", i, (unsigned long long)latencies[i]);
        sum += latencies[i];
        if (latencies[i]<mn) mn=latencies[i];
        if (latencies[i]>mx) mx=latencies[i];
    }
    fclose(fp);

    printf("[writer] avg=%.1f us  min=%.1f us  max=%.1f us\n",
           (double)sum/NUM_ITERS/1000.0,
           (double)mn/1000.0, (double)mx/1000.0);
    printf("[writer] Saved: /tmp/shm_results.csv\n");

    munmap(shm, SHM_SIZE);
    shm_unlink(SHM_NAME);
    sem_close(sem_w); sem_close(sem_r);
    sem_unlink(SEM_WRITE); sem_unlink(SEM_READ);
    return EXIT_SUCCESS;



}