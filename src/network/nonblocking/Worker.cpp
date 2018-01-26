#include "Worker.h"

#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "Utils.h"

namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps):
    _storage(ps),
    _running(false)
{
    // TODO: implementation here
}

// See Worker.h
Worker::~Worker() {
    // TODO: implementation here
}

// See Worker.h
void Worker::Start(int server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    // TODO: implementation here
    _server_socket = server_socket;
    _running = true;
    //OnRunArgs args = {&_running, server_socket};
    if (pthread_create(thread, 0, OnRun, 0) != 0) {
        throw std::runtime_error("unable to create thread");
    }
}

// See Worker.h
void Worker::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    // TODO: implementation here
    running = false;
}

// See Worker.h
void Worker::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    // TODO: implementation here
    pthread_join(_thread, 0);
}

// See Worker.h
void *Worker::OnRun(void *args) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // TODO: implementation here
    //auto temp_args = static_cast<OnRunArgs *>(args);
    //bool &running = *temp_args->first;
    //int server_socket = temp_args->second;

    // 1. Create epoll_context here
    _epoll_fd = epoll_create(EPOLL_MAX);
    if (_epoll_fd <= 0) {
        throw std::runtime_error("unable to epoll_create");
    }

    // 2. Add server_socket to context
    _connections.emplace_back(_epoll_fd, _server_socket, _storage);

    // 3. Accept new connections, don't forget to call make_socket_nonblocking on
    //    the client socket descriptor
    // 4. Add connections to the local context
    // 5. Process connection events
    while (_running) {
        int n = epoll_wait(_epoll_fd, _epoll_events, EPOLL_MAX, EPOLL_WAIT_TIMEOUT);
        if (n == -1) {
            throw std::runtime_error("epoll_wait err");
        }

        for (int i = 0; i < n; ++i) {
            bool event_success = true;
            Connection *connection = static_cast<Connection *>(_epoll_events[i].data.ptr);
            int sock = connection->fd;

            if (sock == _server_socket) {
                if (_epoll_events[i].events & EPOLLIN != EPOLLIN) {
                    throw std::runtime_error("unknown event from server socket or error");
                }

                int new_connection_fd = accept(_server_socket, 0, 0);
                if (new_connection_fd == -1) {
                    throw std::runtime_error("unable to accept new connection");
                }
                _connections.emplace_back(_epoll_fd, new_connection_fd, _storage);

            } else if (_epoll_events[i].events & EPOLLIN == EPOLLIN) {
                event_success = connection->Read();

            } else if (_epoll_events[i].events & EPOLLOUT == EPOLLOUT) {
                event_success = connection->Write();

            } else {
                // Case of error or socket shutdown
                event_success = false;
            }

            if (!event_success) {
                _connections.remove_if([=] (Connection &con) {return con.fd == connection->fd;});
            }
        } // end loop over events
    } // end main loop

    close(_epoll_fd);

    return 0;
    //
    // Do not forget to use EPOLLEXCLUSIVE flag when register socket
    // for events to avoid thundering herd type behavior.
}

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
