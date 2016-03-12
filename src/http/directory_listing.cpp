#include "directory_listing.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <inl/status_codes.h>
#include <misc/storage.h>

static std::string trim_quotes(std::string str) {
    if (str.front() == '\"')
        str = str.substr(1, str.size());
    if (str.back() == '\"')
        str = str.substr(0, str.size());
    return str;
}

static std::string url_encode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char)c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}

http::response http::list_directory(const http::request &req) {
    auto root_path = storage::config().root_path;
    try {
        if (req.url.back() != '/') {
            http::response r{req, http::status_code::Found};
            r.set("Location", req.url + '/');
            r.set("Cache-Control", "no-cache");
            return r;
        }
        fs::path root = root_path + req.url;
        std::ostringstream stream;
        stream << "<h1>Directory listing of " + req.url + "</h1>";
        for (auto it : fs::directory_iterator(root)) {
            fs::path p = it;
            stream << "<a href=\"";
            stream << url_encode(trim_quotes(p.filename())) << "\">";
            stream << trim_quotes(p.filename());
            stream << "</a><br/>";
        }
        http::response r{req, stream.str()};
        r.set("Content-Type", "text/html; charset=utf-8");
        r.set("Cache-Control", "no-cache");
        return r;
    } catch (...) {
        http::response r{req, http::status_code::NotFound};
        r.set("Cache-Control", "no-cache");
        return r;
    }
}
