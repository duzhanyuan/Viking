//
// Created by Vladimir on 8/2/2015.
//

#include "Watcher.h"
#include <algorithm>
#include <assert.h>
#include <iostream>
#include <string.h>
#include "log.h"

using namespace IO;

void Watcher::Stop() {
    _stopRequested = true;
}

void Watcher::Close(std::shared_ptr<Socket> sock)
{
    struct epoll_event *ev = nullptr;
    for(auto& event : _events) {
        if(event.data.fd==(*sock).get_fd()) {
            ev = &event;
            break;
        }
    }
    auto result = epoll_ctl(_efd, EPOLL_CTL_DEL, (*sock).get_fd(), ev);

    if(result == 0)
    {
        //Log::i("closed event on fd = " + std::to_string(ev->data.fd));
    }

    for(int index = 0; index < _to_observe.size(); ++index) {
        if((*_to_observe[index]).get_fd() == (*sock).get_fd())
        {
            _to_observe.erase(_to_observe.begin() + index);
            //Log::i("closed socket with fd = " + std::to_string((*sock).get_fd()));
            break;
        }
    }
}

Watcher::Watcher(std::shared_ptr<Socket> socket) : _socket(socket) {
    _efd = epoll_create1(0);
    if (_efd == -1) {
        throw std::runtime_error("Epoll create failed");
    }
    try {
        AddSocket(socket);
    }
    catch (std::runtime_error &ex) {
        throw;
    }

    _events.resize(_maxEvents);
}

Watcher::~Watcher() {
    for (auto &event : _events)
        ::close(event.data.fd);
}
bool Watcher::stopRequested() const
{
    return _stopRequested;
}

void Watcher::setStopRequested(bool stopRequested)
{
    _stopRequested = stopRequested;
}


std::vector<std::shared_ptr<Socket>> Watcher::Watch() {
    int events_number;
    std::vector<std::shared_ptr<Socket>> result;
    do {
        //std::cout << "will call epoll_wait on _efd = " << _efd << std::endl;
        events_number = epoll_wait(_efd, _events.data(), _maxEvents, -1);
        if (events_number == -1 && errno != EINTR)
            throw std::runtime_error("Epoll wait error, errno = " + std::to_string(errno));
    }
    while (events_number == -1);
    for (int index = 0; index < events_number; ++index) {
        if ((_events[index].events & EPOLLERR) || (_events[index].events & EPOLLHUP) ||
                (!(_events[index].events & EPOLLIN))) {/*
            assert(!(_events[index].events & EPOLLERR));
            assert(!(_events[index].events & EPOLLHUP));
            assert((_events[index].events & EPOLLIN));*/
            Log::e("EPOLLERR, EPOLLHUP, or EPOLLIN received, errno = " + std::to_string(errno));
            ::close(_events[index].data.fd);
            _to_observe.erase(_to_observe.begin() + index);
            continue;
        }
        if ((*_socket).get_fd() == _events[index].data.fd) {
            /* the listening socket received something,
             * that means we'll accept a new connection */
            try {
                while (true) {
                    auto new_connection = (*_socket).Accept();
                    if ((*new_connection).get_fd() == -1) // done accepting
                        break;
                    (*new_connection).MakeNonBlocking();
                    AddSocket(new_connection);
                }
            }
            catch (std::exception &ex) {
                Log::e(ex.what());
                throw;
            }
        }
        else {
            /* a connection socket received something, we'll add it
             * to the list of active socekts */
            auto connection_it = std::find_if(_to_observe.begin(), _to_observe.end(),
                                              [&](std::shared_ptr<Socket> ptr) {
                    return (*ptr).get_fd() == _events[index].data.fd;
        });
            if (connection_it != _to_observe.end()) {
                result.push_back(std::shared_ptr<Socket>(*connection_it));
            }
            else
                throw std::runtime_error("Error when polling. An inactive socket was marked as active.");
        }

    }
    return result;
}

void Watcher::AddSocket(std::shared_ptr<Socket> socket) {
    _to_observe.push_back(socket);
    int fd = (*socket).get_fd();
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET;
    int result = epoll_ctl(_efd, EPOLL_CTL_ADD, ev.data.fd, &ev);
    if (result == -1)
        throw std::runtime_error("Could not add event with fd " + std::to_string(fd));
}

void Watcher::Start(std::function<void(std::shared_ptr<Socket>)> callback) {
    try {
        while (!stopRequested()) {
            auto sockets_with_activity = Watch();
            for (auto &socket : sockets_with_activity)
            {
                callback(socket);
            }
        }
    }
    catch (std::runtime_error &ex) {
        throw;
    }
}

void Watcher::Start(std::function<void (std::vector<std::shared_ptr<Socket> >)> callback)
{
    try {
        while(!stopRequested()) {
            auto sockets_with_activity = Watch();
            if(sockets_with_activity.size() > 0) {
                callback(sockets_with_activity);
            }
        }
    }
    catch(std::runtime_error &ex) {
        throw;
    }
}

