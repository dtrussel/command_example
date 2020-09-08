#include "commands.hpp"
#include "websocket.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

#include <csignal>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

void process_commands(cmd::CommandExecutor& executor, cmd::CommandQueue& cmd_queue){
  cmd::Command command;
  while (cmd_queue.pop(command)) {
    std::visit(executor, command);
  }
}

std::atomic<bool> signaled = false;

void signal_handler(int signal){
  if ((SIGTERM == signal) or (SIGINT == signal)) {
    signaled = true;
  }
}

int main(){
  std::signal(SIGTERM, signal_handler);
  std::signal(SIGINT, signal_handler);

  Lightbulb bulb("LED");
  cmd::CommandExecutor executor(bulb);
  cmd::CommandQueue cmd_queue(100);

  net::io_context io_context(1);

  std::make_shared<Listener>(io_context, tcp::endpoint{tcp::v4(), 8888}, cmd_queue)->run();
  std::thread io_task([&io_context](){ io_context.run(); });

  while (not signaled) {
    const auto now = std::chrono::steady_clock::now();
    process_commands(executor, cmd_queue);
    // some other tasks...
    std::this_thread::sleep_until(now + std::chrono::milliseconds(500));
  }

  std::cout << "=== THE END ===\n";
  io_context.stop();
  if (io_task.joinable()) {
    io_task.join();
  }
}
