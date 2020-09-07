#include "commands.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>

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


void process_commands(cmd::CommandExecutor& executor, std::queue<cmd::Command>& cmd_queue, std::mutex& mtx){
  while (not cmd_queue.empty()) {
    const auto& command = cmd_queue.front();
    std::visit(executor, command);
    cmd_queue.pop();
  }
}

std::atomic<bool> signaled = false;

void signal_handler(int signal){
  if ((SIGTERM == signal) or (SIGINT == signal)) {
    signaled = true;
  }
}

const json j_set_color = R"(
{
  "command_type": "set_color",
  "command_arguments": { "red": 11, "green": 22, "blue": 33 }
}
)"_json;

// TODO websocket async https://www.boost.org/doc/libs/develop/libs/beast/example/websocket/server/async/websocket_server_async.cpp

void receive_commands(net::io_context& io_context, std::queue<cmd::Command>& cmd_queue, std::mutex& mtx){
  tcp::acceptor acceptor{io_context, {tcp::v4(), 8888}};
  tcp::socket socket{io_context};
  acceptor.accept(socket); // blocks until connection is ready
  websocket::stream<tcp::socket> ws{std::move(socket)};

  while (not signaled) {
    // TODO ws.read();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::scoped_lock lock(mtx);
    //std::cout << j_set_color << std::endl;
    const auto command = cmd::deserialize(j_set_color);
    cmd_queue.push(command);
    cmd_queue.push(cmd::SetBrightness{99});
  }
}

int main(){
  std::signal(SIGTERM, signal_handler);
  std::signal(SIGINT, signal_handler);

  Lightbulb bulb("LED");
  cmd::CommandExecutor executor(bulb);
  std::queue<cmd::Command> cmd_queue;
  std::mutex mtx;

  net::io_context io_context(1);
  /*net::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto){
    std::cout << "Signal handler called" << std::endl;
    signaled = true;
    io_context.stop();
  });*/
  // TODO
  //https://www.boost.org/doc/libs/1_69_0/libs/beast/example/websocket/server/sync/websocket_server_sync.cpp
  std::thread receive_task(receive_commands, std::ref(io_context), std::ref(cmd_queue), std::ref(mtx));

  while (not signaled) {
    const auto now = std::chrono::steady_clock::now();
    process_commands(executor, cmd_queue, mtx);
    // some other tasks...
    std::this_thread::sleep_until(now + std::chrono::milliseconds(500));
  }

  std::cout << "=== THE END ===\n";
  //io_context.stop();
  if (receive_task.joinable()) {
    receive_task.join();
  }
}
