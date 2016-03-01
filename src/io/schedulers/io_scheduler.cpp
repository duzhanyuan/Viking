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
#include <io/schedulers/sys_epoll.h>
#include <io/buffers/utils.h>
#include <misc/common.h>
#include <misc/debug.h>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include <typeindex>

using namespace io;

class scheduler::scheduler_impl {
    struct SocketNotFound {
        const epoll::event *event;
    };
    struct write_error {};

    epoll poll;
    scheduler::read_cb read_callback;
    scheduler::barrier_cb barrier_callback;
    scheduler::before_removing_cb before_removing;

    public:
    scheduler_impl() = default;
    scheduler_impl(std::unique_ptr<tcp_socket> sock, scheduler::read_cb read_callback,
                   scheduler::barrier_cb barrier_callback, scheduler::before_removing_cb before_removing)
        : read_callback(read_callback), barrier_callback(barrier_callback), before_removing(before_removing) {
        try {
            add(std::move(sock),
                static_cast<std::uint32_t>(epoll::read) | static_cast<std::uint32_t>(epoll::termination));
        } catch (const epoll::poll_error &) {
            throw;
        }
    }

    void add(std::unique_ptr<tcp_socket> socket, std::uint32_t flags) {
        try {
            auto channel = new io::channel(std::move(socket));
            poll.schedule(channel, flags);
        } catch (const epoll::poll_error &) {
            throw;
        }
    }

    void run() noexcept {
        const auto events = poll.await();
        for (const auto &event : events) {
            poll.modify(event.context, static_cast<std::uint32_t>(epoll::edge_triggered));
            if (event.context->socket->is_acceptor()) {
                add_new_connections(event.context);
                continue;
            }
            if (event.can_terminate()) {
                remove(event.context);
                continue;
            }
            if (event.can_read()) {
                process_read(event.context);
            }
            if (event.can_write()) {
                process_write(event.context);
                continue;
            }
        }
    }

    void add_new_connections(const channel *channel) noexcept {
        do {
            try {
                auto new_connection = channel->socket->accept();
                if (*new_connection) {
                    new_connection->make_non_blocking();
                    add(std::move(new_connection),
                        static_cast<std::uint32_t>(epoll::read) | static_cast<std::uint32_t>(epoll::termination));
                } else
                    break;
            } catch (epoll::poll_error &) {
            }
        } while (true);
    }

    void process_read(channel *channel) noexcept {
        try {
            read_callback(channel);
            poll.modify(channel, static_cast<std::uint32_t>(epoll::write));
        } catch (const io::tcp_socket::connection_closed_by_peer &) {
            remove(channel);
        }
    }

    void process_write(channel *channel) noexcept {
        try {
            auto filled = false;
            while (channel->queue && !filled) {
                if (channel->queue.is_front_async()) {
                    auto new_sync_buffer = barrier_callback(channel->queue);
                    if (*new_sync_buffer) {
                        channel->queue.replace_front(std::move(new_sync_buffer));
                    } else {
                        poll.modify(channel, static_cast<std::uint32_t>(epoll::level_triggered));
                        return;
                    }
                }
                auto result = fill_channel(channel);
                if (!(channel->queue)) {
                    // We filled the channel
                    if (channel->queue.keep_file_open()) {
                        poll.modify(channel, ~static_cast<std::uint32_t>(epoll::write));
                        poll.modify(channel, static_cast<std::uint32_t>(epoll::level_triggered));
                    } else {
                        remove(channel);
                        return;
                    }
                }
                filled = result;
            }
            if (!filled)
                poll.modify(channel, static_cast<std::uint32_t>(epoll::level_triggered));
        } catch (...) {
            remove(channel);
        }
    }

    bool fill_channel(channel *channel) {
        auto &front = *channel->queue.front();
        std::type_index sched_item_type = typeid(front);

        if (sched_item_type == typeid(memory_buffer)) {
            memory_buffer *mem_buffer = reinterpret_cast<memory_buffer *>(channel->queue.front());
            try {
                if (const auto written = channel->socket->write_some(mem_buffer->data)) {
                    if (written == mem_buffer->data.size())
                        channel->queue.remove_front();
                    else
                        std::vector<char>(mem_buffer->data.begin() + written, mem_buffer->data.end())
                            .swap(mem_buffer->data);
                } else {
                    return true;
                }
            } catch (...) {
                debug("Caught exception when writing a memory buffer");
                throw write_error{};
            }

        } else if (sched_item_type == typeid(io::unix_file)) {
            io::unix_file *unix_file = reinterpret_cast<io::unix_file *>(channel->queue.front());
            try {
                auto size_left = unix_file->size_left();
                if (const auto written = unix_file->send_to_fd(channel->socket->get_fd())) {
                    if (written == size_left)
                        channel->queue.remove_front();
                } else {
                    return true;
                }
            } catch (const unix_file::diy &e) {
                /* This is how Linux tells you that you'd better do it yourself in
                 * userspace.
                 * We need to replace this item with a MemoryBuffer version of this
                 * data, at the right offset.
                 */
                try {
                    debug("diy");
                    auto buffer = from(*e.ptr);
                    channel->queue.replace_front(std::move(buffer));
                    return fill_channel(channel);
                } catch (...) {
                    throw write_error{};
                }
            } catch (...) {
                debug("Caught exception when writing a unix file");
                throw write_error{};
            }
        }
        return false;
    }

    void remove(channel *c) noexcept {
        before_removing(c);
        poll.remove(c);
        delete c;
    }
};

scheduler::scheduler() {
    try {
        impl = new scheduler_impl();
    } catch (...) {
        throw;
    }
}

scheduler::scheduler(std::unique_ptr<tcp_socket> sock, scheduler::read_cb read_callback, barrier_cb barrier_callback,
                     before_removing_cb before_removing) {
    try {
        impl = new scheduler_impl(std::move(sock), read_callback, barrier_callback, before_removing);
    } catch (...) {
        throw;
    }
}

scheduler::~scheduler() { delete impl; }

void scheduler::add(std::unique_ptr<io::tcp_socket> socket, uint32_t flags) {
    try {
        impl->add(std::move(socket), flags);
    } catch (const epoll::poll_error &) {
        throw;
    }
}

void scheduler::run() noexcept { impl->run(); }

scheduler &scheduler::operator=(scheduler &&other) {
    if (this != &other) {
        impl = other.impl;
        other.impl = nullptr;
    }
    return *this;
}

scheduler::scheduler(scheduler &&other) { *this = std::move(other); }
