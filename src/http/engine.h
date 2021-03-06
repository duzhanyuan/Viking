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

#ifndef SOCKET_HTTPUTILITY_H
#define SOCKET_HTTPUTILITY_H

#include <http/parser.h>
#include <http/request.h>
#include <io/socket/socket.h>
#include <string>

namespace http {
class context {
    const io::tcp_socket *m_socket;
    http_parser_settings settings_;
    http_parser parser_;
    request m_request;
    std::string buffer;
    std::string header_field;
    bool complete_;

    void assign_method(http_method method_numeric);

    public:
    context(const io::tcp_socket *socket);
    const io::tcp_socket *get_socket() const;
    const request &get_request() const noexcept;
    http::context &operator()();
    bool complete() const noexcept;
};
};

#endif // SOCKET_HTTPUTILITY_H
