// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <chrono>
#include <config.h>
#include <cstdlib>
#include <patch/file.h>
#include <patch/pty_spawn.h>
#include <patch/system.h>
#include <poll.h>
#include <stdexcept>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

#if defined(HAVE_FORKPTY_PTY)
#    include <pty.h>
#elif defined(HAVE_FORKPTY_UTIL)
#    include <util.h>
#else
#    error "Unknown include for forkpty"
#endif

PtySpawn::PtySpawn(const char* cmd, const std::vector<const char*>& args, const std::string& stdin_data)
{
    pid_t pid = forkpty(&m_master, nullptr, nullptr, nullptr);

    if (pid < 0)
        throw std::system_error(errno, std::generic_category(), "Opening tty device failed");

    // Child process runs patch command.
    if (pid == 0) {
        ::setsid();
        ::execv(cmd, const_cast<char**>(args.data()));
        throw std::system_error(errno, std::generic_category(), "Failed to exec command");
    }

    // Turn off terminal echo so that we do not get back from the spawned process what we have sent it.
    disable_echo(m_master);

    write(stdin_data);
    m_output = read();

    pid = ::waitpid(pid, &m_return_code, 0);
    if (pid == -1)
        throw std::system_error(errno, std::generic_category(), "Failed to waiting command to finish executing");

    if (!WIFEXITED(m_return_code))
        throw std::runtime_error("Process did not terminate normally");

    m_return_code = WEXITSTATUS(m_return_code);
}

void PtySpawn::write(const std::string& data)
{
    auto ret = ::write(m_master, data.c_str(), data.size());
    if (ret != data.size())
        throw std::system_error(errno, std::generic_category(), "Failed to exec command");
}

std::string PtySpawn::read(std::chrono::milliseconds timeout)
{
    std::string buffer;
    buffer.resize(32);

    size_t offset = 0;

    while (true) {

        struct pollfd fds = {};
        fds.fd = m_master;
        fds.events = POLLIN;

        int poll_result = ::poll(&fds, 1, static_cast<int>(timeout.count()));
        if (poll_result < 0)
            throw std::system_error(errno, std::generic_category(), "Poll failed waiting for data");

        if (poll_result == 0)
            throw std::runtime_error("Timeout waiting for data");

        auto ret = ::read(m_master, &buffer[0] + offset, buffer.size() - offset);
        if (ret <= 0) {
            buffer.resize(offset);
            break;
        }

        offset += ret;

        if (offset >= buffer.size())
            buffer.resize(buffer.size() * 2);
    }

    return buffer;
}

void PtySpawn::disable_echo(int fd)
{
    struct termios term;
    if (tcgetattr(fd, &term) < 0)
        throw std::system_error(errno, std::generic_category(), "Failed getting terminal attributes");

    term.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    term.c_oflag &= ~ONLCR;

    if (tcsetattr(fd, TCSANOW, &term) < 0)
        throw std::system_error(errno, std::generic_category(), "Failed setting terminal attributes");
}
