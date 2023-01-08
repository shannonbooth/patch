// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <patch/process.h>
#include <poll.h>
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

static void write_c_str_ignore_error(int fd, const char* msg)
{
    ssize_t ret = ::write(fd, msg, strlen(msg));
    (void)ret;
}

Process::Process(const char* cmd, const std::vector<const char*>& args, const std::string& stdin_data)
{
    Pipe stdout_pipe;
    Pipe stderr_pipe;
    Pipe stdin_pipe;

    pid_t pid = ::fork();
    if (pid < 0)
        throw std::system_error(errno, std::generic_category(), "Forking failed");

    if (pid == 0) {
        if (dup2(stdout_pipe.write_fd(), STDOUT_FILENO) == -1)
            throw std::system_error(errno, std::generic_category(), "Failed duping stdout");

        if (dup2(stderr_pipe.write_fd(), STDERR_FILENO) == -1)
            throw std::system_error(errno, std::generic_category(), "Failed duping stdout");

        if (dup2(stdin_pipe.read_fd(), STDIN_FILENO) == -1)
            throw std::system_error(errno, std::generic_category(), "Failed duping stdin");

        stdout_pipe.close();
        stderr_pipe.close();
        stdin_pipe.close();

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        ::execv(cmd, const_cast<char**>(args.data()));

        // Not much we can do here. Let's make a best effort to write some data to stdout/stderr
        // to communicate the error to the parent process and abort to let it know that something
        // went terribly wrong!
        const char* error = std::strerror(errno);
        write_c_str_ignore_error(STDERR_FILENO, "Failed to execv");
        if (cmd) {
            write_c_str_ignore_error(STDERR_FILENO, " '");
            write_c_str_ignore_error(STDERR_FILENO, cmd);
            write_c_str_ignore_error(STDERR_FILENO, "'");
        }
        write_c_str_ignore_error(STDERR_FILENO, ": ");
        write_c_str_ignore_error(STDERR_FILENO, error);
        std::abort();
    }

    stdout_pipe.close_write_fd();
    stderr_pipe.close_write_fd();
    stdin_pipe.close_read_fd();

    if (!stdin_data.empty()) {
        ssize_t ret = ::write(stdin_pipe.write_fd(), stdin_data.data(), stdin_data.size());
        if (ret < 0)
            throw std::system_error(errno, std::generic_category(), "Failed writing data to stdin");

        if (static_cast<size_t>(ret) != stdin_data.size())
            throw std::system_error(errno, std::generic_category(), "Not enough data written to stdin");
    }

    stdin_pipe.close_write_fd();

    m_stdout_data.resize(32);
    m_stderr_data.resize(32);

    size_t stdout_offset = 0;
    size_t stderr_offset = 0;

    while (true) {
        std::vector<struct pollfd> fds;
        if (stdout_pipe.read_fd() >= 0) {
            struct pollfd fd = {};
            fd.fd = stdout_pipe.read_fd();
            fd.events = POLLIN;
            fds.emplace_back(fd);
        }

        if (stderr_pipe.read_fd() >= 0) {
            struct pollfd fd = {};
            fd.fd = stderr_pipe.read_fd();
            fd.events = POLLIN;
            fds.emplace_back(fd);
        }

        if (fds.empty())
            break;

        int poll_result = ::poll(fds.data(), fds.size(), 5000);
        if (poll_result < 0)
            throw std::system_error(errno, std::generic_category(), "Poll failed waiting for data");

        if (poll_result == 0)
            throw std::runtime_error("Timeout waiting for data");

        for (const auto& fd : fds) {
            if (fd.revents == 0)
                continue;

            if (fd.fd == stdout_pipe.read_fd()) {
                auto ret = ::read(stdout_pipe.read_fd(), &m_stdout_data[0] + stdout_offset, m_stdout_data.size() - stdout_offset);
                if (ret <= 0) {
                    stdout_pipe.close_read_fd();
                    continue;
                }

                stdout_offset += static_cast<size_t>(ret);

                if (stdout_offset >= m_stdout_data.size())
                    m_stdout_data.resize(m_stdout_data.size() * 2);
            } else if (fd.fd == stderr_pipe.read_fd()) {
                auto ret = ::read(stderr_pipe.read_fd(), &m_stderr_data[0] + stderr_offset, m_stderr_data.size() - stderr_offset);
                if (ret <= 0) {
                    stderr_pipe.close_read_fd();
                    continue;
                }

                stderr_offset += static_cast<size_t>(ret);

                if (stderr_offset >= m_stderr_data.size())
                    m_stderr_data.resize(m_stderr_data.size() * 2);
            }
        }
    }

    m_stdout_data.resize(stdout_offset);
    m_stderr_data.resize(stderr_offset);

    pid = ::waitpid(pid, &m_return_code, 0);
    if (pid == -1)
        throw std::system_error(errno, std::generic_category(), "Failed to waiting command to finish executing");

    // Show stderr data in here just incase there is something useful
    if (!WIFEXITED(m_return_code))
        throw std::runtime_error("Process did not terminate normally -- " + m_stderr_data);

    m_return_code = WEXITSTATUS(m_return_code);
}
