#include "Connection.h"
#include <sys/uio.h>

#include <iostream>

namespace Afina {
namespace Network {
namespace MTnonblock {

void Connection::Start() {
    _event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
    _logger->debug("Start connection on socket {}", _socket);
}

// See Connection.h
void Connection::OnError() {
    _logger->debug("Error on socket {}", _socket);
    running.store(false, std::memory_order_relaxed);
}

// See Connection.h
void Connection::OnClose() {
    _logger->debug("Socket {} closed connection", _socket);
    running.store(false, std::memory_order_relaxed);
}

// See Connection.h
void Connection::DoRead() {
    std::lock_guard<std::mutex> lock(mut);
    try {
        int readed_bytes = -1;
        while ((readed_bytes = read(_socket, client_buffer, sizeof(client_buffer))) > 0) {
            _logger->debug("Got {} bytes from socket", readed_bytes);

            // Single block of data readed from the socket could trigger inside actions a multiple times,
            // for example:
            // - read#0: [<command1 start>]
            // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
            while (readed_bytes > 0) {
                _logger->debug("Process {} bytes", readed_bytes);
                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(client_buffer, readed_bytes, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(client_buffer, client_buffer + parsed, readed_bytes - parsed);
                        readed_bytes -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", readed_bytes, arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));
                    argument_for_command.append(client_buffer, to_read);

                    std::memmove(client_buffer, client_buffer + to_read, readed_bytes - to_read);
                    arg_remains -= to_read;
                    readed_bytes -= to_read;
                }

                // Thre is command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    if (argument_for_command.size()) {
                        argument_for_command.resize(argument_for_command.size() - 2);
                    }
                    command_to_execute->Execute(*pStorage, argument_for_command, result);

                    // Send response
                    result += "\r\n";
                    if (output.empty()) {
                        _event.events |= EPOLLOUT;
                    }
                    output.push_back(result);
                    if (output.size() >= max_output_queue_size) {
                        _event.events &= !EPOLLIN;
                    }
                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while (readed_bytes)
        }

        if (readed_bytes == 0) {
            _logger->debug("Connection closed");
        } else {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
        if (errno != EAGAIN) {
            running.store(false, std::memory_order_relaxed);
        }
    }
}


void Connection::DoWrite() {
    std::lock_guard<std::mutex> lock(mut);
    _logger->debug("Do write on {} socket", _socket);
    struct iovec io[output.size()];
    size_t i = 0;
    for (i = 0; i < output.size(); ++i) {
        io[i].iov_base = &(output[i][0]);
        io[i].iov_len = output[i].size();
    }
    io[0].iov_base = static_cast<char *>(io[0].iov_base) + _head;
    io[0].iov_len -= _head;
    int written_bytes = writev(_socket, io, i);
    if (written_bytes <= 0) {
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
            running.store(false, std::memory_order_relaxed);
        }
        throw std::runtime_error("Failed to send response");
    }
    i = 0;
    _head += written_bytes;
    for (const auto& command : output) {
        if (_head >= command.size()) {
            i++;
            _head -= command.size();
        } else {
            break;
        }
    }
    output.erase(output.begin(), output.begin() + i);
    if (output.empty()) {
        _event.events &= ~EPOLLOUT;
    }
    if (output.size() <= max_output_queue_size){
        _event.events |= EPOLLIN;
    }
}

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

