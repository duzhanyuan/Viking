/*
Copyright (C) 2015 Voinea Constantin Vladimir

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/
#include <io/schedulers/io_scheduler.h>
#include <io/schedulers/channel.h>
#include <http/dispatcher/dispatcher.h>
#include <server/server.h>
#include <misc/storage.h>
#include <misc/debug.h>
#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <signal.h>
#include <fstream>

using namespace Web;
using namespace io;
tcp_socket *MakeSocket(int port, int max_pending);

class Server::ServerImpl {
    int port_;
    int max_pending_;
    bool stop_requested_;
    dispatcher dispatcher_;
    io::scheduler scheduler_;

    inline void IgnoreSigpipe() { signal(SIGPIPE, SIG_IGN); }

    public:
    ServerImpl(int port) : port_(port), stop_requested_(false) {}

    inline void Initialize() {
        IgnoreSigpipe();
        debug("Pid = " + std::to_string(getpid()));
        if (auto sock = MakeSocket(port_, max_pending_)) {
            scheduler_ = io::scheduler(std::unique_ptr<io::tcp_socket>(sock),
                                       [this](const io::channel *ch) {
                                           try {
                                               return dispatcher_.handle_connection(ch);
                                           } catch (...) {
                                               throw;
                                           }
                                       },
                                       [this](schedule_item & schedule_item) -> auto {
                                           if (schedule_item.is_front_async()) {
                                               AsyncBuffer<http::response> *async_buffer =
                                                   static_cast<AsyncBuffer<http::response> *>(schedule_item.front());
                                               if (async_buffer->IsReady())
                                                   return dispatcher_.handle_barrier(async_buffer);
                                           }
                                           return std::make_unique<memory_buffer>(std::vector<char>());
                                       },
                                       [this](const io::channel *ch) { dispatcher_.will_remove(ch); });
        } else {
            throw Server::PortInUse{port_};
        }
    }

    inline void Run(bool indefinitely) {
        if (!indefinitely)
            scheduler_.run();
        else {
            stop_requested_ = false;
            while (!stop_requested_) {
                scheduler_.run();
            }
        }
    }

    inline void Freeze() { stop_requested_ = true; }

    inline void AddRoute(const http::method &method, std::function<bool(const std::string &)> validator,
                         std::function<http::resolution(http::request)> function) {
        dispatcher_.add_route(std::make_pair(std::make_pair(method, validator), function));
    }

    inline void AddRoute(const http::method &method, const std::regex &regex,
                         std::function<http::resolution(http::request)> function) {

        auto ptr = [regex](auto string) {
            try {
                return std::regex_match(string, regex);
            } catch (const std::regex_error &) {
                return false;
            }
        };
        dispatcher_.add_route(std::make_pair(std::make_pair(method, ptr), function));
    }

    inline void SetSettings(const configuration &s) {
        storage::set_config(s);
        max_pending_ = s.max_connections;
    }
};

io::tcp_socket *MakeSocket(int port, int max_pending) {
    try {
        auto sock = new io::tcp_socket(port);
        sock->bind();
        sock->make_non_blocking();
        sock->listen(max_pending);
        return sock;
    } catch (const io::tcp_socket::port_in_use &) {
        return nullptr;
    }
}

Server::Server(int port) { impl = new ServerImpl(port); }

Server::~Server() { delete impl; }

Server &Server::operator=(Server &&other) {
    if (this != &other) {
        impl = other.impl;
        other.impl = nullptr;
    }
    return *this;
}

Server::Server(Server &&other) { *this = std::move(other); }

void Server::AddRoute(const http::method &method, std::function<bool(const std::string &)> validator,
                      std::function<http::resolution(http::request)> function) {
    impl->AddRoute(method, validator, function);
}

void Server::AddRoute(const http::method &method, const std::regex &regex,
                      std::function<http::resolution(http::request)> function) {
    impl->AddRoute(method, regex, function);
}

void Server::SetSettings(const configuration &s) { impl->SetSettings(s); }

void Server::Initialize() { impl->Initialize(); }

void Server::Run(bool indefinitely) { impl->Run(indefinitely); }

void Server::Freeze() { impl->Freeze(); }

std::string Server::GetVersion() const noexcept { return "0.7.8"; }
