#include "../include/oe_core_interface.h"
#include "../include/oe_ipc.h"
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <atomic>

static OECoreState* g_state = nullptr;
static const OECoreInterface* g_interface = nullptr;
static void* g_core_handle = nullptr;

static int g_video_width = 0;
static int g_video_height = 0;
static OEPixelFormat g_pixel_format = OE_PIXEL_RGBA8888;
static void* g_shm_addr = nullptr;
static size_t g_shm_size = 0;

static int g_audio_sample_rate = 44100;
static int g_audio_fd = -1;
static int g_control_fd = -1;

static std::atomic<bool> g_running{true};

static void ipc_send_response(int fd, OEIPCMessage* resp)
{
    write(fd, resp, sizeof(OEIPCMessage));
}

static void handle_command(OEIPCMessage* cmd)
{
    OEIPCMessage resp = {};
    resp.cmd = cmd->cmd;
    resp.error_code = OE_IPC_RESP_OK;

    switch (cmd->cmd) {
        case OE_IPC_CMD_LOAD_ROM:
            if (g_interface && g_state) {
                int ret = g_interface->load_rom(g_state, cmd->path);
                if (ret != 0) {
                    resp.error_code = OE_IPC_RESP_ERROR;
                } else {
                    g_interface->get_video_size(g_state, &g_video_width, &g_video_height);
                    g_pixel_format = g_interface->get_pixel_format(g_state);
                    g_audio_sample_rate = g_interface->get_audio_sample_rate(g_state);
                    resp.width = g_video_width;
                    resp.height = g_video_height;
                    resp.pixel_format = (int)g_pixel_format;
                    resp.audio_sample_rate = g_audio_sample_rate;
                }
            }
            break;

        case OE_IPC_CMD_RUN_FRAME:
            if (g_interface && g_state) {
                g_interface->run_frame(g_state);
                const void* buf = g_interface->get_video_buffer(g_state);
                if (buf && g_shm_addr && g_video_width > 0 && g_video_height > 0) {
                    int bpp = (g_pixel_format == OE_PIXEL_RGB565) ? 2 : 4;
                    size_t frame_size = (size_t)g_video_width * g_video_height * bpp;
                    if (frame_size <= g_shm_size) {
                        memcpy(g_shm_addr, buf, frame_size);
                    }
                }
                if (g_audio_fd >= 0) {
                    int16_t audio_buf[4096];
                    size_t samples = g_interface->read_audio(g_state, audio_buf, 2048);
                    if (samples > 0) {
                        write(g_audio_fd, audio_buf, samples * sizeof(int16_t));
                    }
                }
            }
            break;

        case OE_IPC_CMD_RESET:
            if (g_interface && g_state) {
                g_interface->reset(g_state);
            }
            break;

        case OE_IPC_CMD_SAVE_STATE:
            if (g_interface && g_state) {
                int ret = g_interface->save_state(g_state, cmd->path);
                if (ret != 0) resp.error_code = OE_IPC_RESP_ERROR;
            }
            break;

        case OE_IPC_CMD_LOAD_STATE:
            if (g_interface && g_state) {
                int ret = g_interface->load_state(g_state, cmd->path);
                if (ret != 0) resp.error_code = OE_IPC_RESP_ERROR;
            }
            break;

        case OE_IPC_CMD_SET_BUTTON:
            if (g_interface && g_state) {
                g_interface->set_button(g_state, cmd->player, cmd->button, cmd->value);
            }
            break;

        case OE_IPC_CMD_SET_AXIS:
            if (g_interface && g_state) {
                g_interface->set_axis(g_state, cmd->player, cmd->axis, cmd->value);
            }
            break;

        case OE_IPC_CMD_GET_VIDEO_SIZE:
            if (g_interface && g_state) {
                g_interface->get_video_size(g_state, &g_video_width, &g_video_height);
            }
            resp.width = g_video_width;
            resp.height = g_video_height;
            break;

        case OE_IPC_CMD_GET_PIXEL_FORMAT:
            if (g_interface && g_state) {
                g_pixel_format = g_interface->get_pixel_format(g_state);
            }
            resp.pixel_format = (int)g_pixel_format;
            break;

        case OE_IPC_CMD_GET_AUDIO_SAMPLE_RATE:
            if (g_interface && g_state) {
                g_audio_sample_rate = g_interface->get_audio_sample_rate(g_state);
            }
            resp.audio_sample_rate = g_audio_sample_rate;
            break;

        case OE_IPC_CMD_SET_SAVE_DIRECTORY:
            if (g_interface && g_state && g_interface->set_save_directory) {
                g_interface->set_save_directory(g_state, cmd->path);
            }
            break;

        case OE_IPC_CMD_QUIT:
            g_running = false;
            break;

        default:
            resp.error_code = OE_IPC_RESP_ERROR;
            break;
    }

    ipc_send_response(g_control_fd, &resp);
}

static void setup_shm(size_t size)
{
    g_shm_size = size;
    g_shm_addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (g_shm_addr == MAP_FAILED) {
        std::cerr << "Failed to create shared memory: " << strerror(errno) << std::endl;
        g_shm_addr = nullptr;
    }
}

static void cleanup_shm()
{
    if (g_shm_addr) {
        munmap(g_shm_addr, g_shm_size);
        g_shm_addr = nullptr;
    }
}

extern "C" {

void oe_ipc_init_shared_memory(size_t size)
{
    setup_shm(size);
}

void* oe_ipc_get_shmem()
{
    return g_shm_addr;
}

void oe_ipc_set_audio_fd(int fd)
{
    g_audio_fd = fd;
}

void oe_ipc_set_control_fd(int fd)
{
    g_control_fd = fd;
}

}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <core.so path>" << std::endl;
        return 1;
    }

    const char* core_path = argv[1];

    g_core_handle = dlopen(core_path, RTLD_LAZY);
    if (!g_core_handle) {
        std::cerr << "dlopen failed: " << dlerror() << std::endl;
        return 1;
    }

    auto get_interface = (const OECoreInterface* (*)())dlsym(g_core_handle, "oe_get_core_interface");
    if (!get_interface) {
        std::cerr << "dlsym failed: " << dlerror() << std::endl;
        dlclose(g_core_handle);
        return 1;
    }

    g_interface = get_interface();
    if (!g_interface) {
        std::cerr << "oe_get_core_interface returned NULL" << std::endl;
        dlclose(g_core_handle);
        return 1;
    }

    g_state = g_interface->create();
    if (!g_state) {
        std::cerr << "core create failed" << std::endl;
        dlclose(g_core_handle);
        return 1;
    }

    /* Wire up the bidirectional control socket and audio pipe */
    g_control_fd = STDIN_FILENO;
    g_audio_fd   = STDOUT_FILENO;

    /* Map the shared video memory region passed from the parent via OE_IPC_SHMEM_FD */
    size_t shmem_size = (argc > 2) ? (size_t)atol(argv[2]) : (OE_IPC_MAX_WIDTH * OE_IPC_MAX_HEIGHT * 4);
    g_shm_size = shmem_size;
    g_shm_addr = mmap(nullptr, shmem_size, PROT_READ | PROT_WRITE, MAP_SHARED, OE_IPC_SHMEM_FD, 0);
    if (g_shm_addr == MAP_FAILED) {
        std::cerr << "oe_core_host: failed to map shared video memory: " << strerror(errno) << std::endl;
        g_shm_addr = nullptr;
    }

    OEIPCMessage cmd;
    while (g_running.load(std::memory_order_relaxed)) {
        ssize_t n = read(STDIN_FILENO, &cmd, sizeof(cmd));
        if (n <= 0) {
            break;
        }
        if (n == sizeof(cmd)) {
            handle_command(&cmd);
        }
    }

    if (g_state && g_interface) {
        g_interface->destroy(g_state);
    }
    cleanup_shm();
    if (g_core_handle) {
        dlclose(g_core_handle);
    }

    return 0;
}
