#include "oe_core_bridge.h"
#include "oe_ipc.h"
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <signal.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <limits.h>

namespace OpenEmu {

class CoreBridgeImpl {
public:
    pid_t m_pid = -1;
    int m_control_fd = -1;
    int m_audio_read_fd = -1;
    int m_audio_write_fd = -1;
    int m_shmem_fd = -1;
    void* m_shmem = nullptr;
    size_t m_shmem_size = 0;
    int m_video_width = 0;
    int m_video_height = 0;
    OEPixelFormat m_pixel_format = OE_PIXEL_RGBA8888;
    int m_audio_sample_rate = 44100;
    bool m_is_running = false;

    ~CoreBridgeImpl()
    {
        shutdown();
    }

    bool start(const std::string& core_path, const std::string& host_path)
    {
        signal(SIGPIPE, SIG_IGN);

        int control_pipe[2];
        int audio_pipe[2];

        if (socketpair(AF_UNIX, SOCK_STREAM, 0, control_pipe) < 0 || pipe(audio_pipe) < 0) {
            std::cerr << "IPCBridge: socketpair/pipe failed: " << strerror(errno) << std::endl;
            return false;
        }

        /* Create a file-descriptor-backed shared memory region so it survives
           exec in the child process.  We dup2 the fd to OE_IPC_SHMEM_FD (a
           known slot) in the child and pass the size as argv[2]. */
        m_shmem_size = OE_IPC_MAX_WIDTH * OE_IPC_MAX_HEIGHT * 4;
        m_shmem_fd = (int)syscall(SYS_memfd_create, "oe_video", 0);
        if (m_shmem_fd < 0 || ftruncate(m_shmem_fd, (off_t)m_shmem_size) < 0) {
            std::cerr << "IPCBridge: memfd_create/ftruncate failed: " << strerror(errno) << std::endl;
            close(control_pipe[0]); close(control_pipe[1]);
            close(audio_pipe[0]);   close(audio_pipe[1]);
            return false;
        }
        m_shmem = mmap(nullptr, m_shmem_size, PROT_READ | PROT_WRITE,
                       MAP_SHARED, m_shmem_fd, 0);
        if (m_shmem == MAP_FAILED) {
            std::cerr << "IPCBridge: mmap failed: " << strerror(errno) << std::endl;
            m_shmem = nullptr;
            close(m_shmem_fd); m_shmem_fd = -1;
            close(control_pipe[0]); close(control_pipe[1]);
            close(audio_pipe[0]);   close(audio_pipe[1]);
            return false;
        }

        m_pid = fork();
        if (m_pid < 0) {
            std::cerr << "IPCBridge: fork failed: " << strerror(errno) << std::endl;
            munmap(m_shmem, m_shmem_size); m_shmem = nullptr;
            close(m_shmem_fd); m_shmem_fd = -1;
            close(control_pipe[0]); close(control_pipe[1]);
            close(audio_pipe[0]);   close(audio_pipe[1]);
            return false;
        }

        if (m_pid == 0) {
            close(control_pipe[1]);
            close(audio_pipe[0]);

            dup2(control_pipe[0], STDIN_FILENO);       /* fd 0: bidirectional control */
            dup2(audio_pipe[1],   STDOUT_FILENO);      /* fd 1: audio write pipe      */
            dup2(m_shmem_fd,      OE_IPC_SHMEM_FD);   /* fd 3: video shared memory   */

            char size_str[32];
            snprintf(size_str, sizeof(size_str), "%zu", m_shmem_size);
            execl(host_path.c_str(), host_path.c_str(), core_path.c_str(), size_str, nullptr);
            std::cerr << "IPCBridge: execl failed: " << strerror(errno) << std::endl;
            _exit(1);
        }

        close(control_pipe[0]);
        close(audio_pipe[1]);

        m_control_fd = control_pipe[1];
        m_audio_read_fd = audio_pipe[0];

        send_cmd_init();

        m_is_running = true;
        return true;
    }

    bool write_message(const OEIPCMessage& cmd)
    {
        if (m_control_fd < 0) return false;
        const ssize_t wrote = write(m_control_fd, &cmd, sizeof(cmd));
        if (wrote != (ssize_t)sizeof(cmd)) {
            m_is_running = false;
            return false;
        }
        return true;
    }

    bool read_message(OEIPCMessage* resp)
    {
        if (m_control_fd < 0 || !resp) return false;
        const ssize_t n = read(m_control_fd, resp, sizeof(*resp));
        if (n != (ssize_t)sizeof(*resp)) {
            m_is_running = false;
            return false;
        }
        return true;
    }

    void send_cmd_init()
    {
        OEIPCMessage cmd = {};
        cmd.cmd = OE_IPC_CMD_GET_VIDEO_SIZE;
        if (!write_message(cmd)) return;

        OEIPCMessage resp = {};
        if (!read_message(&resp)) return;
        m_video_width = resp.width;
        m_video_height = resp.height;

        cmd.cmd = OE_IPC_CMD_GET_PIXEL_FORMAT;
        if (!write_message(cmd)) return;
        resp = {};
        if (!read_message(&resp)) return;
        m_pixel_format = (OEPixelFormat)resp.pixel_format;

        cmd.cmd = OE_IPC_CMD_GET_AUDIO_SAMPLE_RATE;
        if (!write_message(cmd)) return;
        resp = {};
        if (!read_message(&resp)) return;
        m_audio_sample_rate = resp.audio_sample_rate;
    }

    void setSaveDirectory(const std::string& path)
    {
        OEIPCMessage cmd = {};
        cmd.cmd = OE_IPC_CMD_SET_SAVE_DIRECTORY;
        snprintf(cmd.path, sizeof(cmd.path), "%s", path.c_str());
        if (!write_message(cmd)) return;

        OEIPCMessage resp = {};
        read_message(&resp);
    }

    bool loadROM(const std::string& path)
    {
        OEIPCMessage cmd = {};
        cmd.cmd = OE_IPC_CMD_LOAD_ROM;
        snprintf(cmd.path, sizeof(cmd.path), "%s", path.c_str());
        if (!write_message(cmd)) return false;

        OEIPCMessage resp = {};
        if (!read_message(&resp)) return false;
        if (resp.error_code == OE_IPC_RESP_OK) {
            /* Update video/audio info now that the ROM is loaded */
            if (resp.width > 0)  m_video_width  = resp.width;
            if (resp.height > 0) m_video_height = resp.height;
            if (resp.audio_sample_rate > 0) m_audio_sample_rate = resp.audio_sample_rate;
            if (resp.pixel_format >= 0) m_pixel_format = (OEPixelFormat)resp.pixel_format;
        }
        return resp.error_code == OE_IPC_RESP_OK;
    }

    void runFrame()
    {
        if (!m_is_running) return;
        OEIPCMessage cmd = {};
        cmd.cmd = OE_IPC_CMD_RUN_FRAME;
        if (!write_message(cmd)) return;

        OEIPCMessage resp = {};
        read_message(&resp);
    }

    void reset()
    {
        OEIPCMessage cmd = {};
        cmd.cmd = OE_IPC_CMD_RESET;
        if (!write_message(cmd)) return;

        OEIPCMessage resp = {};
        read_message(&resp);
    }

    bool saveState(const std::string& path)
    {
        OEIPCMessage cmd = {};
        cmd.cmd = OE_IPC_CMD_SAVE_STATE;
        snprintf(cmd.path, sizeof(cmd.path), "%s", path.c_str());
        if (!write_message(cmd)) return false;

        OEIPCMessage resp = {};
        if (!read_message(&resp)) return false;
        return resp.error_code == OE_IPC_RESP_OK;
    }

    bool loadState(const std::string& path)
    {
        OEIPCMessage cmd = {};
        cmd.cmd = OE_IPC_CMD_LOAD_STATE;
        snprintf(cmd.path, sizeof(cmd.path), "%s", path.c_str());
        if (!write_message(cmd)) return false;

        OEIPCMessage resp = {};
        if (!read_message(&resp)) return false;
        return resp.error_code == OE_IPC_RESP_OK;
    }

    void setButton(int player, int button, bool pressed)
    {
        OEIPCMessage cmd = {};
        cmd.cmd = OE_IPC_CMD_SET_BUTTON;
        cmd.player = player;
        cmd.button = button;
        cmd.value = pressed ? 1 : 0;
        if (!write_message(cmd)) return;

        OEIPCMessage resp = {};
        read_message(&resp);
    }

    CoreBridge::VideoSize videoSize() const
    {
        return {m_video_width, m_video_height};
    }

    OEPixelFormat pixelFormat() const
    {
        return m_pixel_format;
    }

    const void* videoBuffer() const
    {
        return m_shmem;
    }

    int audioSampleRate() const
    {
        return m_audio_sample_rate;
    }

    size_t readAudio(int16_t* buffer, size_t maxSamples)
    {
        if (m_audio_read_fd < 0) return 0;
        
        ssize_t bytes_read = read(m_audio_read_fd, buffer, maxSamples * sizeof(int16_t));
        if (bytes_read < 0) return 0;
        return bytes_read / sizeof(int16_t);
    }

    bool isRunning() const
    {
        if (!m_is_running || m_pid <= 0) return false;
        int status;
        pid_t result = waitpid(m_pid, &status, WNOHANG);
        return result == 0;
    }

    void shutdown()
    {
        if (!m_is_running) return;

        if (m_control_fd >= 0) {
            OEIPCMessage cmd = {};
            cmd.cmd = OE_IPC_CMD_QUIT;
            write(m_control_fd, &cmd, sizeof(cmd));
            close(m_control_fd);
            m_control_fd = -1;
        }

        if (m_audio_read_fd >= 0) {
            close(m_audio_read_fd);
            m_audio_read_fd = -1;
        }

        if (m_pid > 0) {
            kill(m_pid, SIGTERM);
            waitpid(m_pid, nullptr, 0);
            m_pid = -1;
        }

        if (m_shmem) {
            munmap(m_shmem, m_shmem_size);
            m_shmem = nullptr;
        }

        if (m_shmem_fd >= 0) {
            close(m_shmem_fd);
            m_shmem_fd = -1;
        }

        m_is_running = false;
    }
};

CoreBridge::CoreBridge(const std::string& libraryPath)
    : m_impl(std::make_shared<CoreBridgeImpl>())
{
    char exe_path[PATH_MAX] = {0};
    std::string host_path = "./oe_core_host";
    if (readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1) > 0) {
        std::string self_path(exe_path);
        const size_t slash = self_path.find_last_of('/');
        if (slash != std::string::npos) {
            host_path = self_path.substr(0, slash + 1) + "oe_core_host";
        }
    }
    if (!m_impl->start(libraryPath, host_path)) {
        throw std::runtime_error("Failed to start core host process");
    }
}

CoreBridge::~CoreBridge()
{
    if (m_impl) {
        m_impl->shutdown();
    }
}

bool CoreBridge::loadROM(const std::string& path)
{
    if (!m_impl) return false;
    return m_impl->loadROM(path);
}

void CoreBridge::setSaveDirectory(const std::string& path)
{
    if (m_impl) {
        m_impl->setSaveDirectory(path);
    }
}

void CoreBridge::runFrame()
{
    if (m_impl) {
        m_impl->runFrame();
    }
}

void CoreBridge::reset()
{
    if (m_impl) {
        m_impl->reset();
    }
}

bool CoreBridge::saveState(const std::string& path)
{
    if (!m_impl) return false;
    return m_impl->saveState(path);
}

bool CoreBridge::loadState(const std::string& path)
{
    if (!m_impl) return false;
    return m_impl->loadState(path);
}

void CoreBridge::setButton(int player, int button, bool pressed)
{
    if (m_impl) {
        m_impl->setButton(player, button, pressed);
    }
}

CoreBridge::VideoSize CoreBridge::videoSize() const
{
    if (m_impl) {
        return m_impl->videoSize();
    }
    return {0, 0};
}

OEPixelFormat CoreBridge::pixelFormat() const
{
    if (m_impl) {
        return m_impl->pixelFormat();
    }
    return OE_PIXEL_RGBA8888;
}

const void* CoreBridge::videoBuffer() const
{
    if (m_impl) {
        return m_impl->videoBuffer();
    }
    return nullptr;
}

int CoreBridge::audioSampleRate() const
{
    if (m_impl) {
        return m_impl->audioSampleRate();
    }
    return 0;
}

size_t CoreBridge::readAudio(int16_t* buffer, size_t maxSamples)
{
    if (m_impl) {
        return m_impl->readAudio(buffer, maxSamples);
    }
    return 0;
}

bool CoreBridge::isRunning() const
{
    if (m_impl) {
        return m_impl->isRunning();
    }
    return false;
}

} // namespace OpenEmu
