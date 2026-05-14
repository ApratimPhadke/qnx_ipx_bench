
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>

#define NUM_ITERATIONS 10000
#define WARMUP_ITERS 200

static const size_t PAYLOAD_SIZES[] = {8, 64, 512, 4096, 16384};
#define NUM_PAYLOAD_SIZES 5
#define MAX_PAYLOAD_BYTES 16384

/* ── FIX 1: removed duplicate char data[64] field ── */
typedef struct
{
    uint16_t type;
    uint16_t subtype;
    uint32_t seq;
    uint32_t payload_size;
    char data[MAX_PAYLOAD_BYTES];
} BenchMsg;

typedef struct
{
    uint16_t type;
    char ack[8];
} BenchReply;

#define BENCH_MSG_TYPE 0x5000
#define BENCH_STOP_TYPE 0x5001
#define SERVER_NAME "/bench_server"

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
    return (double)sorted[idx] / 1000.0; /* ns → µs */
}

static void run_one_size(int coid, size_t psize,
                         double *out_avg, double *out_p99,
                         FILE *summary_fp)
{
    BenchMsg msg;
    BenchReply reply;
    uint64_t *latencies;
    uint64_t t_start, t_end;
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

    memset(&msg, 0, sizeof(msg));
    msg.type = BENCH_MSG_TYPE;
    msg.payload_size = (uint32_t)psize;
    memset(msg.data, 0xAB, psize);

    /* warmup */
    for (i = 0; i < WARMUP_ITERS; i++)
    {
        msg.seq = (uint32_t)i;
        MsgSend(coid, &msg,
                sizeof(msg) - MAX_PAYLOAD_BYTES + psize,
                &reply, sizeof(reply));
    }

    /* timed loop */
    for (i = 0; i < NUM_ITERATIONS; i++)
    {
        msg.seq = (uint32_t)i;
        t_start = get_ns();

        if (MsgSend(coid,
                    &msg,
                    sizeof(msg) - MAX_PAYLOAD_BYTES + psize,
                    &reply, sizeof(reply)) == -1)
        {
            perror("MsgSend");
            break;
        }

        t_end = get_ns();
        latencies[i] = t_end - t_start;
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

    snprintf(fname, sizeof(fname), "/tmp/msg_results_%zuB.csv", psize);
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

    printf("  [msg] %6zuB | avg=%7.2f us  p99=%7.2f us  max=%7.2f us\n",
           psize, avg_us, p99, p_max);

    *out_avg = avg_us;
    *out_p99 = p99;

    free(latencies);
}

int main(void)
{
    int coid;
    BenchMsg stop_msg;
    BenchReply reply;
    FILE *summary_fp;
    int i;
    double avg, p99;

    /* name_open on QNX returns int directly */
    coid = name_open(SERVER_NAME + 1, 0);
    if (coid == -1)
    {
        perror("name_open — is msg_server running?");
        return EXIT_FAILURE;
    }

    summary_fp = fopen("/tmp/msg_summary.csv", "w");
    if (!summary_fp)
    {
        perror("fopen summary");
        return EXIT_FAILURE;
    }

    fprintf(summary_fp,
            "payload_bytes,avg_us,min_us,p50_us,p95_us,p99_us,p999_us,max_us\n");

    printf("[msg_client] Payload sweep — %d iterations each\n", NUM_ITERATIONS);
    printf("  %-8s | %-14s %-14s %-14s\n",
           "Size", "avg (us)", "p99 (us)", "max (us)");
    printf("  %s\n", "------------------------------------------------------");

    for (i = 0; i < NUM_PAYLOAD_SIZES; i++)
        run_one_size(coid, PAYLOAD_SIZES[i], &avg, &p99, summary_fp);

    fclose(summary_fp);
    printf("\n[msg_client] Summary saved: /tmp/msg_summary.csv\n");

    memset(&stop_msg, 0, sizeof(stop_msg));
    stop_msg.type = BENCH_STOP_TYPE;
    MsgSend(coid, &stop_msg, sizeof(stop_msg), &reply, sizeof(reply));

    name_close(coid);
    return EXIT_SUCCESS;
}