#ifndef SERVER_H
#define SERVER_H

#include <misc/settings.h>
#include <http/resolution.h>
#include <http/request.h>
#include <vector>
#include <memory>

namespace Web {
class Server {
    class ServerImpl;
    ServerImpl *impl;

    public:
    Server(int);
    ~Server();
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;
    Server(Server &&);
    Server &operator=(Server &&);
    void AddRoute(const Http::Method &method, const std::string &uri_regex,
                  std::function<Http::Resolution(Http::Request)> function);
    void SetSettings(const Settings &);
    void Initialize();
    void Run(bool indefinitely = true);
    void Freeze();
    struct PortInUse {
        int port;
    };
};
}

#endif // SERVER_H
