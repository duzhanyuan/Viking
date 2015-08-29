#include "socket.h"
#include "watcher.h"
#include "request.h"
#include "parser.h"
#include "server.h"
#include "dispatcher.h"
#include "response.h"
#include <iostream>
#include "json.h"

int main() {
  try {
    Web::Server s(1234);
    Web::Dispatcher::routes.insert(std::make_pair(
        "^\\/adsaf\\/json\\/(\\d+)$", [](Http::Request req) -> Http::Response {
          // /adsaf/json/<int>

          Json::Value root(Json::arrayValue);
          Json::Value records(Json::arrayValue);

          Json::Value val;
          val["this"] = "that";

          records.append(val);

          Json::Value a1(Json::arrayValue);
          decltype(a1) a2(Json::arrayValue);

          a1.append("1");
          a1.append("2");

          a2.append(req.uri_components()[2]);
          a2.append("2");

          records.append(a1);
          records.append(a2);

          root.append(records);

          return {req, root};
        }));

    Web::Dispatcher::routes.insert(std::make_pair(
        "^\\/adsaf\\/json\\/$", [](Http::Request req) -> Http::Response {
          // /adsaf/json/
          Json::Value root(Json::arrayValue);
          Json::Value records(Json::arrayValue);

          Json::Value a1(Json::arrayValue);
          decltype(a1) a2(Json::arrayValue);

          a1.append({"1"});
          a1.append({"2"});

          a2.append({"3"});
          a2.append({"4"});

          records.append(a1);
          records.append(a2);

          root.append(records);

          return {req, root};
        }));
    Settings settings;
#ifndef __arm__
    settings.root_path =
        "/run/user/1000/gvfs/smb-share:server=192.168.2.103,share=backup/"
        "server";
#else
    settings.root_path = "/mnt/exthdd/server";
#endif
    settings.max_connections = 1000;
    s.setSettings(settings);
    s.run();
  } catch (std::exception& ex) {
    std::cerr << ex.what();
  }
  return 0;
}
