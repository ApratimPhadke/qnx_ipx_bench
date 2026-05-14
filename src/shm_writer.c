#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <time.h>
#include <stdint.h>

#define MAX_PAYLOAD_BYTES 16384
#define SHM_SIZE (MAX_PAYLOAD_BYTES + 64)
#define SHM_NAME "/bench_shm"
#define SEM_WRITE "/bench_sem_w"
#define SEM_READ "/bench_sem_r"
#define NUM_ITERATIONS 10000
#define WARMUP_ITERS 200

static const size_t PAYLOAD_SIZES[] = {8, 64, 512, 4096, 16384};
#define NUM_PAYLOAD_SIZES 5

typedef struct
{
    uint32_t seq;
    uint32_t payload_size;
    uint32_t stop;
    char data[MAX_PAYLOAD_BYTES];
} SharedBlock;

static inline uint64_t get_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static double percentile(uint64_t *sorted, int n, double pct)
{
    int idx = (int)(pct / 100.0 * n);
    if (idx >= n)
        idx = n - 1;
    return (double)sorted[idx] / 1000.0;
}

static void run_one_size(SharedBlock *shm, sem_t *sem_w, sem_t *sem_r,
                         size_t psize, FILE *summary_fp)
{
    uint64_t *latencies;
    uint64_t t_start;
    uint64_t sum = 0;
    int i;
    char fname[64];
    FILE *fp;

    latencies = malloc(sizeof(uint64_t) * NUM_ITERATIONS);
    if (!latencies)
    {
        perror("malloc");
        return;
    }

    shm->payload_size = (uint32_t)psize;
    shm->stop = 0;
    memset(shm->data, 0xCD, psize);

    for (i = 0; i < WARMUP_ITERS; i++)
    {
        shm->seq = (uint32_t)i;
        sem_post(sem_w);
        sem_wait(sem_r);
    }

    /* timed loop */
    for (i = 0; i < NUM_ITERATIONS; i++)
    {
        shm->seq = (uint32_t)i;
        t_start = get_ns();
        sem_post(sem_w); /* "data ready" */
        sem_wait(sem_r); /* "reader ack" */
        latencies[i] = get_ns() - t_start;
        sum += latencies[i];
    }

    double avg_us = (double)sum / NUM_ITERATIONS / 1000.0;

    uint64_t *sorted = malloc(sizeof(uint64_t) * NUM_ITERATIONS);
    memcpy(sorted, latencies, sizeof(uint64_t) * NUM_ITERATIONS);
    qsort(sorted, NUM_ITERATIONS, sizeof(uint64_t), cmp_u64);

    double p50 = percentile(sorted, NUM_ITERATIONS, 50.0);
    double p95 = percentile(sorted, NUM_ITERATIONS, 95.0);
    double p99 = percentile(sorted, NUM_ITERATIONS, 99.0);
    double p999 = percentile(sorted, NUM_ITERATIONS, 99.9);
    double p_min = (double)sorted[0] / 1000.0;
    double p_max = (double)sorted[NUM_ITERATIONS - 1] / 1000.0;

    free(sorted);

    snprintf(fname, sizeof(fname), "/tmp/shm_results_%zuB.csv", psize);
    fp = fopen(fname, "w");
    if (fp)
    {
        fprintf(fp, "iteration,latency_ns\n");
        for (i = 0; i < NUM_ITERATIONS; i++)
            fprintf(fp, "%d,%llu\n", i, (unsigned long long)latencies[i]);
        fclose(fp);
    }

    fprintf(summary_fp,
            "%zu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
            psize, avg_us, p_min, p50, p95, p99, p999, p_max);

    printf("  [shm] %6zuB | avg=%7.2f us  p99=%7.2f us  max=%7.2f us\n",
           psize, avg_us, p99, p_max);

    free(latencies);
}

int main(void)
{
    int shm_fd;
    SharedBlock *shm;
    sem_t *sem_w, *sem_r;
    FILE *summary_fp;

    /* clean up stale objects from any previous crashed run */
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_WRITE);
    sem_unlink(SEM_READ);

    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open");
        return EXIT_FAILURE;
    }
    ftruncate(shm_fd, SHM_SIZE);

    shm = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED)
    {
        perror("mmap");
        return EXIT_FAILURE;
    }

    sem_w = sem_open(SEM_WRITE, O_CREAT, 0666, 0);
    sem_r = sem_open(SEM_READ, O_CREAT, 0666, 0);
    if (sem_w == SEM_FAILED || sem_r == SEM_FAILED)
    {
        perror("sem_open");
        return EXIT_FAILURE;
    }

    summary_fp = fopen("/tmp/shm_summary.csv", "w");
    if (!summary_fp)
    {
        perror("fopen summary");
        return EXIT_FAILURE;
    }
    fprintf(summary_fp,
            "payload_bytes,avg_us,min_us,p50_us,p95_us,p99_us,p999_us,max_us\n");

    printf("[shm_writer] Payload sweep — %d iterations each\n", NUM_ITERATIONS);
    printf("  %-8s | %-14s %-14s %-14s\n",
           "Size", "avg (us)", "p99 (us)", "max (us)");
    printf("  %s\n", "------------------------------------------------------");

    for (int i = 0; i < NUM_PAYLOAD_SIZES; i++)
        run_one_size(shm, sem_w, sem_r, PAYLOAD_SIZES[i], summary_fp);

    /* signal reader to stop */
    shm->stop = 1;
    sem_post(sem_w);

    fclose(summary_fp);
    printf("\n[shm_writer] Summary saved: /tmp/shm_summary.csv\n");

    munmap(shm, SHM_SIZE);
    shm_unlink(SHM_NAME);
    sem_close(sem_w);
    sem_close(sem_r);
    sem_unlink(SEM_WRITE);
    sem_unlink(SEM_READ);
    return EXIT_SUCCESS;
}