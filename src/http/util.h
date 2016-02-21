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
#ifndef UTIL_H
#define UTIL_H
#include <http/request.h>
#include <io/filesystem.h>

namespace http {
class util {
    public:
    static bool is_passable(const request &) noexcept;
    static bool is_disk_resource(const request &) noexcept;
    static bool is_complete(const request &) noexcept;
    static bool can_have_body(http::method) noexcept;
    static std::string get_mimetype(fs::path) noexcept;
};
}

#endif // UTIL_H
