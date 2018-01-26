#ifndef AFINA_NETWORK_NONBLOCKING_WORKER_H
#define AFINA_NETWORK_NONBLOCKING_WORKER_H

#include <memory>
#include <atomic>
#include <pthread.h>


#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <list>
//#include <Connection.h>
#include <protocol/Parser.h>
#include <afina/execute/Command.h>

#include "Utils.h"

namespace Afina {

// Forward declaration, see afina/Storage.h
class Storage;

namespace Network {
namespace NonBlocking {
class Connection;
/**
 * # Thread running epoll
 * On Start spaws background thread that is doing epoll on the given server
 * socket and process incoming connections and its data
 */
class Worker {
public:
    Worker(std::shared_ptr<Afina::Storage> ps);
    ~Worker();

    /**
     * Spaws new background thread that is doing epoll on the given server
     * socket. Once connection accepted it must be registered and being processed
     * on this thread
     */
    void Start(int server_socket);

    /**
     * Signal background thread to stop. After that signal thread must stop to
     * accept new connections and must stop read new commands from existing. Once
     * all readed commands are executed and results are send back to client, thread
     * must stop
     */
    void Stop();

    /**
     * Blocks calling thread until background one for this worker is actually
     * been destoryed
     */
    void Join();

protected:
    //typedef std::pair<bool *, int> OnRunArgs;
    /**
     * Method executing by background thread
     */
    void *OnRun(void *args);

private:
    enum {EPOLL_MAX = 100, EPOLL_WAIT_TIMEOUT = 1000};

    pthread_t _thread;
    bool _running;
    std::shared_ptr<Afina::Storage> _storage;
    int _server_socket;

    int _epoll_fd;
    epoll_event _epoll_events[EPOLL_MAX];
    std::list<Connection> _connections;
};

class Connection {
public:
    const int fd;

    enum {READ_BUF_SZ = 1000};

    Connection(int epoll_fd, int fd, std::shared_ptr<Afina::Storage> ps) : fd(fd), _ps(ps)
    {
        epoll_event event;
        event.events = /*EPOLLEXCLUSIVE |*/ EPOLLHUP | EPOLLIN | EPOLLOUT | EPOLLERR;

        event.data.ptr = this;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
            throw std::runtime_error("epoll_ctl error");
        }
    }

    ~Connection() {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    bool Read() {
        char buf[READ_BUF_SZ];
        int nread;
        size_t parsed;
        bool ready;
        do {
            nread = recv(fd, buf, READ_BUF_SZ, 0);
            if (nread < 0) {
                return false;
            }

            bool run_success = Run(buf, nread);
            if (!run_success) {
                return false;
            }
        } while (nread > 0);

        return nread >= 0;
    }

    bool Run(char *buf, int len) {
        while(len > 0) {
            if (!_ready) {
                size_t parsed = 0;
                try {
                    _ready = _parser.Parse(buf, len, parsed);
                } catch(...) {
                    _exec_res.push_back("Server Error");
                    _ready = false;
                    _arg_buf = "";
                    return Write();
                }

                buf += parsed;
                len -= parsed;
            }

            if (_ready) {
                int parse_len = std::min(len, int(_body_size - _arg_buf.size()));
                _arg_buf.append(buf, parse_len);

                buf += parse_len;
                len -= parse_len;
                if (_arg_buf.size() == _body_size) {
                    std::string _out;
                    try {
                        _cmd->Execute(*_ps, _arg_buf, _out);
                    } catch(...) {
                        _out = "Server Error";
                    }
                    _exec_res.push_back(_out);
                    _ready = false;
                    _arg_buf = "";
                    _body_size = 0;
                }
            }

            if (!Write()) {
                return false;
            }
        }
    }

    bool Write() {
        int sent_sz = 1;
        while (sent_sz > 0 && !_exec_res.empty()) {
            sent_sz = send(fd, _exec_res.front().c_str(), _exec_res.front().size(), 0);

            if (sent_sz < 0) {
                return false;
            }

            _exec_res.front().erase(0, sent_sz);
            if (_exec_res.front().empty()) {
                _exec_res.pop_front();
            }
        }

        return sent_sz >= 0;
    }

private:
    std::shared_ptr<Afina::Storage> _ps;

    Afina::Protocol::Parser _parser;
    std::list<std::string> _exec_res;

    bool _ready = false;
    std::string _arg_buf;
    std::unique_ptr<Execute::Command> _cmd;
    uint32_t _body_size;
};

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
#endif // AFINA_NETWORK_NONBLOCKING_WORKER_H
