#ifndef OE_IPC_H
#define OE_IPC_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define OE_IPC_SHM_NAME "/oe_video_shm"
#define OE_IPC_MAX_WIDTH 512
#define OE_IPC_MAX_HEIGHT 448
#define OE_IPC_SHMEM_FD 3

#define OE_IPC_CMD_LOAD_ROM    1
#define OE_IPC_CMD_RUN_FRAME   2
#define OE_IPC_CMD_RESET       3
#define OE_IPC_CMD_SAVE_STATE 4
#define OE_IPC_CMD_LOAD_STATE 5
#define OE_IPC_CMD_SET_BUTTON 6
#define OE_IPC_CMD_SET_AXIS   7
#define OE_IPC_CMD_QUIT       8

#define OE_IPC_CMD_GET_VIDEO_SIZE 9
#define OE_IPC_CMD_GET_PIXEL_FORMAT 10
#define OE_IPC_CMD_GET_AUDIO_SAMPLE_RATE 11
#define OE_IPC_CMD_SET_SAVE_DIRECTORY 12

#define OE_IPC_RESP_OK         0
#define OE_IPC_RESP_ERROR     -1

typedef struct {
    int cmd;
    int player;
    int button;
    int axis;
    int value;
    int width;
    int height;
    int pixel_format;
    int audio_sample_rate;
    int32_t error_code;
    char path[512];
} OEIPCMessage;

#endif
