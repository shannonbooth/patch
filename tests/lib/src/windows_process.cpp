// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/process.h>
#include <patch/system.h>
#include <stdexcept>
#include <system_error>
#include <windows.h>

class Pipe {
public:
    explicit Pipe(bool inherit_for_read = true)
    {
        SECURITY_ATTRIBUTES attr;
        attr.nLength = sizeof(SECURITY_ATTRIBUTES);
        attr.bInheritHandle = true;
        attr.lpSecurityDescriptor = nullptr;

        if (!CreatePipe(&m_read_handle, &m_write_handle, &attr, 0))
            throw std::system_error(GetLastError(), std::system_category(), "Failed creating stdout pipe");

        if (!SetHandleInformation(inherit_for_read ? m_read_handle : m_write_handle, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(m_read_handle);
            CloseHandle(m_write_handle);
            throw std::system_error(GetLastError(), std::system_category(), "Failed setting handle information");
        }
    }

    void close_write_handle()
    {
        if (m_write_handle) {
            CloseHandle(m_write_handle);
            m_write_handle = nullptr;
        }
    }

    void close_read_handle()
    {
        if (m_read_handle) {
            CloseHandle(m_read_handle);
            m_read_handle = nullptr;
        }
    }

    ~Pipe()
    {
        if (m_read_handle)
            CloseHandle(m_read_handle);
        if (m_write_handle)
            CloseHandle(m_write_handle);
    }

    HANDLE read_handle() const { return m_read_handle; }
    HANDLE write_handle() const { return m_write_handle; }

private:
    HANDLE m_read_handle;
    HANDLE m_write_handle;
};

std::wstring create_cmdline(const std::vector<const char*>& args)
{
    // FIXME: apply quoting rules.
    std::string output;
    for (const char* arg : args) {
        if (!arg)
            break;
        output += arg;
        output += " ";
    }

    if (!output.empty())
        output.pop_back();
    return Patch::to_wide(output);
}

static void remove_carriage_returns(std::string& str)
{
    size_t pos = 0;
    while ((pos = str.find("\r\n", pos)) != std::string::npos)
        str.erase(pos, 1);
}

static std::string read_data_from_handle(HANDLE handle)
{
    std::string data;
    data.resize(32);

    size_t offset = 0;

    while (true) {
        DWORD read_amount;
        int ret = ReadFile(handle, &data[0] + offset, static_cast<DWORD>(data.size() - offset), &read_amount, nullptr);
        if (ret == 0 || read_amount == 0) {
            data.resize(offset);
            break;
        }

        offset += read_amount;
        if (offset >= data.size())
            data.resize(data.size() * 2);
    }

    remove_carriage_returns(data);

    return data;
}

Process::Process(const char* cmd, const std::vector<const char*>& args, const std::string& stdin_data)
{
    Pipe stdout_pipe;
    Pipe stderr_pipe;
    Pipe stdin_pipe(false);

    PROCESS_INFORMATION process_info {};
    STARTUPINFOW start_info {};

    start_info.cb = sizeof(start_info);
    start_info.hStdOutput = stdout_pipe.write_handle();
    start_info.hStdError = stderr_pipe.write_handle();
    start_info.hStdInput = stdin_pipe.read_handle();
    start_info.dwFlags |= STARTF_USESTDHANDLES;

    std::wstring cmd_str = Patch::to_wide(cmd);
    std::wstring cmdline = create_cmdline(args);

    int ret = CreateProcessW(cmd_str.c_str(), // application name
        &cmdline[0],                          // command line
        nullptr,                              // process security attributes
        nullptr,                              // primary thread security attributes
        true,                                 // handles are inherited
        0,                                    // creation flags
        nullptr,                              // use parent's environment
        nullptr,                              // use parent's current directory
        &start_info,                          // STARTUPINFO pointer
        &process_info);                       // receives PROCESS_INFORMATION

    if (ret == 0)
        throw std::system_error(GetLastError(), std::system_category(), "Failed creating process");

    stdout_pipe.close_write_handle();
    stderr_pipe.close_write_handle();
    stdin_pipe.close_read_handle();

    if (!stdin_data.empty()) {
        DWORD written;
        if (WriteFile(stdin_pipe.write_handle(), stdin_data.data(), static_cast<DWORD>(stdin_data.size()), &written, nullptr) == 0)
            throw std::system_error(errno, std::generic_category(), "Failed writing data to stdin");
    }

    stdin_pipe.close_write_handle();

    m_stdout_data = read_data_from_handle(stdout_pipe.read_handle());
    m_stderr_data = read_data_from_handle(stderr_pipe.read_handle());

    DWORD wait_ret = WaitForSingleObject(process_info.hProcess, 5000);
    if (wait_ret == WAIT_TIMEOUT) {
        CloseHandle(process_info.hProcess);
        CloseHandle(process_info.hThread);
        throw std::runtime_error("Timeout waiting for process");
    }

    DWORD exit_code;
    ret = GetExitCodeProcess(process_info.hProcess, &exit_code);

    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);

    if (ret == 0)
        throw std::system_error(GetLastError(), std::system_category(), "Failed getting process exit code");

    m_return_code = exit_code;
}
