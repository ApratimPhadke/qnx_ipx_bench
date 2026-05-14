/* src/msg_server.c
 * QNX Message Passing SERVER
 * Handles variable-size messages for the payload sweep.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>

#define MAX_PAYLOAD_BYTES 16384

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

int main(void)
{
    name_attach_t *attach;
    BenchMsg *msg;
    BenchReply reply;
    int rcvid;

    /* heap allocation — 16KB struct is too large for stack */
    msg = malloc(sizeof(BenchMsg));
    if (!msg)
    {
        perror("malloc");
        return EXIT_FAILURE;
    }

    attach = name_attach(NULL, SERVER_NAME + 1, 0);
    if (attach == NULL)
    {
        perror("name_attach failed");
        free(msg);
        return EXIT_FAILURE;
    }

    printf("[server] Running. Channel id = %d\n", attach->chid);
    fflush(stdout);

    memset(&reply, 0, sizeof(reply));
    strncpy(reply.ack, "OK", sizeof(reply.ack));
    reply.type = BENCH_MSG_TYPE;

    while (1)
    {

        rcvid = MsgReceive(attach->chid, msg, sizeof(BenchMsg), NULL);

        if (rcvid == -1)
        {
            perror("MsgReceive failed");
            break;
        }

        /* internal pulses have negative type */
        if (msg->type < 0)
        {
            MsgReply(rcvid, EOK, NULL, 0);
            continue;
        }

        if (msg->type == BENCH_STOP_TYPE)
        {
            MsgReply(rcvid, EOK, &reply, sizeof(reply));
            break;
        }

        /* benchmark message — reply */
        MsgReply(rcvid, EOK, &reply, sizeof(reply));
    }

    name_detach(attach, 0);
    free(msg);
    printf("[server] Done.\n");
    return EXIT_SUCCESS;
}