#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <sys/neutrino.h>
#include <sys/dispatch.h>

typedef struct
{
    uint16_t type;
    uint16_t subtype;
    char data[64];
    uint32_t seq;

} BenchMsg;

typedef struct
{
    uint16_t type;
    char ack[8];

} BenchReply;

#define BENCH_MSG_TYPE  0x5000
#define BENCH_STOP_TYPE 0x5001

#define SERVER_NAME "/bench_server"

#define NUM_ITERATIONS 10000
#define WARMUP_ITERS   200

static inline uint64_t get_ns()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((uint64_t)ts.tv_sec * 1000000000ULL) +
           (uint64_t)ts.tv_nsec;
}

int main()
{
    BenchMsg msg;
    BenchReply reply;

    uint64_t t_start;
    uint64_t t_end;

    uint64_t latencies[NUM_ITERATIONS];

    uint64_t sum = 0;
    uint64_t mn;
    uint64_t mx;

    FILE *fp;

    int i;
    int coid;

    
    coid = name_open(SERVER_NAME + 1, 0);

    if (coid == -1)
    {
        perror("name_open failed! Is server running?");
        return EXIT_FAILURE;
    }

    memset(&msg, 0, sizeof(msg));

    msg.type = BENCH_MSG_TYPE;

    strncpy(msg.data,
            "BENCH_PAYLOAD",
            sizeof(msg.data) - 1);

    printf("[client] Warming up (%d iterations)...\n",
           WARMUP_ITERS);

    for (i = 0; i < WARMUP_ITERS; i++)
    {
        msg.seq = i;

        if (MsgSend(coid,
                    &msg,
                    sizeof(msg),
                    &reply,
                    sizeof(reply)) == -1)
        {
            perror("Warmup MsgSend failed");

            name_close(coid);

            return EXIT_FAILURE;
        }
    }

    printf("[client] Benchmarking %d iterations...\n",
           NUM_ITERATIONS);

    for (i = 0; i < NUM_ITERATIONS; i++)
    {
        msg.seq = i;

        t_start = get_ns();

        if (MsgSend(coid,
                    &msg,
                    sizeof(msg),
                    &reply,
                    sizeof(reply)) == -1)
        {
            perror("MsgSend failed");
            break;
        }

        t_end = get_ns();

        latencies[i] = t_end - t_start;
    }

    
    msg.type = BENCH_STOP_TYPE;

    MsgSend(coid,
            &msg,
            sizeof(msg),
            &reply,
            sizeof(reply));

    
    fp = fopen("/tmp/msg_results.csv", "w");

    if (!fp)
    {
        perror("fopen");

        name_close(coid);

        return EXIT_FAILURE;
    }

    fprintf(fp, "iteration,latency_ns\n");

    mn = latencies[0];
    mx = latencies[0];

    for (i = 0; i < NUM_ITERATIONS; i++)
    {
        fprintf(fp,
                "%d,%llu\n",
                i,
                (unsigned long long)latencies[i]);

        sum += latencies[i];

        if (latencies[i] < mn)
            mn = latencies[i];

        if (latencies[i] > mx)
            mx = latencies[i];
    }

    fclose(fp);

    printf("\n");
    printf("[client] Benchmark Results\n");
    printf("----------------------------------\n");
    printf("Average Latency : %.2f us\n",
           (double)sum / NUM_ITERATIONS / 1000.0);

    printf("Minimum Latency : %.2f us\n",
           (double)mn / 1000.0);

    printf("Maximum Latency : %.2f us\n",
           (double)mx / 1000.0);

    printf("----------------------------------\n");

    printf("[client] Results saved to:\n");
    printf("/tmp/msg_results.csv\n");

    /* Disconnect */
    name_close(coid);

    return EXIT_SUCCESS;
}