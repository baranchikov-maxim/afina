#pragma once

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <list>

namespace Afina {

namespace Protocol{
class Parser;
}

namespace Network {
namespace NonBlocking {



class Connection {
public:
    const int fd;

    enum {READ_BUF_SZ = 1000};

    Connection(const &Connection) = delete;
    void operator= (const &Connection) = delete;

    Connection(int epoll_fd, int fd, std::shared_ptr<Afina::Storage> ps) : fd(fd), _ps(ps)
    {
        epoll_event event;
        ev.events = EPOLLEXCLUSIVE | EPOLLHUP | EPOLLIN | EPOLLOUT | EPOLLERR;

        ev.data.ptr = this;

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
            sent_sz = send(fd, _exec_res.front().c_str(), _exec_res.front().size());

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
