#include <thread>
#include <sstream>
#include "dispatcher.h"
#include "Parser.h"
#include "routeutility.h"
#include "global.h"
#include "log.h"
#include "responsemanager.h"
#include "storage.h"

std::map<std::string, std::function<Http::Response(Http::Request)>> Web::Dispatcher::routes;

using namespace Web;

bool Dispatcher::Dispatch(IO::Socket& connection)
{
    try {
        auto &&request = Http::Parser(connection)();
        if(request.IsPassable()) {
            if(!request.IsResource()) {
                auto handler = RouteUtility::GetHandler(request, routes);
                if(handler) {
                    PassToUser(request, handler, connection);
                    return false;
                }
                else {
                    // no user-defined handler, return not found
                    ResponseManager::Respond(request, {request, 404}, connection);
                    return true;
                }
            }
            else {
                //it's a resource
                try {
                    auto &&resource = Storage::GetResource(request.URI.c_str());
                    Http::Response resp {request, resource};
                    ResponseManager::Respond(request, resp, connection);
                    return true;
                }
                catch(int code) {
                    ResponseManager::Respond(request, {request, code}, connection);
                }
            }
        }
        else {
            //not passable
        }
    }
    catch(std::runtime_error &ex) {
        Log::e(ex.what());
    }
    return true;
}

void Dispatcher::PassToUser(Http::Request request, std::function<Http::Response(Http::Request)> user_handler, IO::Socket& socket)
{
    /* from this point, everything is asynchronous */
//    std::thread request_thread([=, &socket]() {
        auto response = user_handler(request);
        ResponseManager::Respond(std::move(request), std::move(response), socket);
//    });
    //request_thread.detach();
}