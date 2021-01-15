#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <deque>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

#include <spdlog/logger.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include "protocol/Parser.h"

#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <sys/epoll.h>

namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<spdlog::logger> logger, std::shared_ptr<Afina::Storage> ps) : _socket(s), pStorage(ps), _logger(logger) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
        std::memset(client_buffer, 0, 4096);
        running.store(true, std::memory_order_release);
    }
        

    inline bool isAlive() const { return running.load(std::memory_order_relaxed); }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class Worker;
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;
    std::atomic<bool> running;

    // Here is connection state
    // - parser: parse state of the stream
    // - command_to_execute: last command parsed out of stream
    // - arg_remains: how many bytes to read from stream to get command argument
    // - argument_for_command: buffer stores argument
    std::size_t arg_remains;
    std::size_t _head;
    Protocol::Parser parser;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;

    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> pStorage;
    std::deque<std::string> output;
    char client_buffer[4096];

    size_t max_output_queue_size;
    std::mutex mut;
};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
