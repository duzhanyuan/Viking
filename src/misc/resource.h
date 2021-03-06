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
#ifndef RESOURCE_H
#define RESOURCE_H

#include <io/filesystem.h>
#include <string>
#include <vector>

class resource {
    fs::path _path;
    fs::file_time_type _last_write;

    public:
    std::vector<char> raw;
    std::vector<char> deflated;
    std::vector<char> gzipped;
    resource() = default;
    resource(const fs::path &, const std::vector<char> &);
    resource(const fs::path &);
    ~resource() = default;
    operator bool();

    const fs::path &path() const;
    const fs::file_time_type &last_write() const;
};

#endif // RESOURCE_H
