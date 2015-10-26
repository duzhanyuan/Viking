#include <server/server.h>
#include <server/dispatcher.h>
#include <io/watcher.h>
#include <io/socket_watcher.h>
#include <http/parser.h>
#include <misc/storage.h>
#include <io/outputscheduler.h>

#include <utility>
#include <algorithm>
#include <functional>
#include <iostream>
#include <thread>
#include <future>
#include <regex>
#include <map>

using namespace Web;

std::mutex Log::mLock;
std::string Log::_fn;
Server::Server(int port) : _port(port) {
  Log::Init("log_file.txt");
  Log::SetEnabled(false);
  Log::i("Started logging");
  setSettings({});
}

void Server::setSettings(const Settings &s) {
  Storage::setSettings(s);
  _maxPending = s.max_connections;
}

void Server::run() {
  if (_port == -1)
    throw std::runtime_error("Port number not set!");
  try {
    _masterSocket = IO::Socket::start_socket(_port, _maxPending);
    // IO::Watcher _master_listener(_masterSocket, _maxPending);

    IO::OutputScheduler &output_scheduler = IO::OutputScheduler::get();
    std::thread output_thread(&IO::OutputScheduler::Run,
                              std::ref(output_scheduler));

    output_thread.detach();

    IO::SocketWatcher<IO::Socket> watcher(_masterSocket);

    auto watcher_callbacks = [&](
        std::vector<std::shared_ptr<IO::Socket>> sockets) {
      for (auto &sock : sockets) {
        Log::i("Will dispatch " + std::to_string(sockets.size()) +
               " connections");
        auto should_close = Dispatcher::Dispatch(*sock);
        if (should_close)
          watcher.Remove(*sock);
      }
    };

    while (true) {
      watcher.Run(watcher_callbacks);
    }

  } catch (std::exception &ex) {
    Log::e(std::string("Server error: ").append(ex.what()));
    auto msg =
        std::string("Server error. Please see the log file. Last exception: ");
    msg += ex.what();
    throw std::runtime_error(msg);
  }
}
int Server::maxPending() const { return _maxPending; }

void Server::setMaxPending(int maxPending) { _maxPending = maxPending; }
