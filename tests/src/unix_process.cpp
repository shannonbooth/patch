// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <poll.h>
#include <process.h>
#include <stdexcept>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

class Pipe {
public:
    Pipe()
    {
        if (pipe(m_fds.data()) != 0)
            throw std::system_error(errno, std::generic_category(), "Creating pipe failed");
    }

    ~Pipe() { close(); }

    void close()
    {
        if (m_fds[0] != -1)
            ::close(m_fds[0]);
        if (m_fds[1] != -1)
            ::close(m_fds[1]);
    }

    int read_fd() const { return m_fds[0]; }

    int write_fd() const { return m_fds[1]; }

    void close_read_fd()
    {
        ::close(m_fds[0]);
        m_fds[0] = -1;
    }

    void close_write_fd()
    {
        ::close(m_fds[1]);
        m_fds[1] = -1;
    }

private:
    std::array<int, 2> m_fds;
};

Process::Process(const char* cmd, const std::vector<const char*>& args)
{
    Pipe stdout_pipe;
    Pipe stderr_pipe;

    pid_t pid = ::fork();
    if (pid < 0)
        throw std::system_error(errno, std::generic_category(), "Forking failed");

    if (pid == 0) {
        if (dup2(stdout_pipe.write_fd(), STDOUT_FILENO) == -1)
            throw std::system_error(errno, std::generic_category(), "Failed duping stdout");

        if (dup2(stderr_pipe.write_fd(), STDERR_FILENO) == -1)
            throw std::system_error(errno, std::generic_category(), "Failed duping stdout");

        stdout_pipe.close();
        stderr_pipe.close();

        ::execv(cmd, const_cast<char**>(args.data()));
        throw std::system_error(errno, std::generic_category(), "Failed to exec command");
    }

    stdout_pipe.close_write_fd();
    stderr_pipe.close_write_fd();

    m_stdout_data.resize(32);
    m_stderr_data.resize(32);

    size_t stdout_offset = 0;
    size_t stderr_offset = 0;

    while (true) {
        std::array<struct pollfd, 2> fds = {};
        fds[0].fd = stdout_pipe.read_fd();
        fds[0].events = POLLIN;
        fds[1].fd = stderr_pipe.read_fd();
        fds[1].events = POLLIN;

        int poll_result = ::poll(fds.data(), fds.size(), 5000);
        if (poll_result < 0)
            throw std::system_error(errno, std::generic_category(), "Poll failed waiting for data");

        if (poll_result == 0)
            throw std::runtime_error("Timeout waiting for data");

        bool done = false;

        for (const auto& fd : fds) {
            if (fd.revents == 0)
                continue;

            if (fd.fd == stdout_pipe.read_fd()) {
                auto ret = ::read(stdout_pipe.read_fd(), &m_stdout_data[0] + stdout_offset, m_stdout_data.size() - stdout_offset);
                if (ret <= 0) {
                    done = true;
                    continue;
                }

                stdout_offset += ret;

                if (stdout_offset >= m_stdout_data.size())
                    m_stdout_data.resize(m_stdout_data.size() * 2);
            } else if (fd.fd == stderr_pipe.read_fd()) {
                auto ret = ::read(stderr_pipe.read_fd(), &m_stderr_data[0] + stderr_offset, m_stderr_data.size() - stderr_offset);
                if (ret <= 0) {
                    done = true;
                    continue;
                }

                stderr_offset += ret;

                if (stderr_offset >= m_stderr_data.size())
                    m_stderr_data.resize(m_stderr_data.size() * 2);
            }
        }

        if (done)
            break;
    }

    m_stdout_data.resize(stdout_offset);
    m_stderr_data.resize(stderr_offset);

    pid = ::waitpid(pid, &m_return_code, 0);
    if (pid == -1)
        throw std::system_error(errno, std::generic_category(), "Failed to waiting command to finish executing");

    if (!WIFEXITED(m_return_code))
        throw std::runtime_error("Process did not terminate normally");

    m_return_code = WEXITSTATUS(m_return_code);
}
