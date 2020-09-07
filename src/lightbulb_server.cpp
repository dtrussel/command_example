#include "commands.hpp"
#include "websocket.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/lockfree/queue.hpp>

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


void process_commands(cmd::CommandExecutor& executor, boost::lockfree::queue<cmd::Command>& cmd_queue){
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

// TODO websocket async https://www.boost.org/doc/libs/develop/libs/beast/example/websocket/server/async/websocket_server_async.cpp

/*
void receive_commands(net::io_context& io_context, boost::lockfree::queue<cmd::Command>& cmd_queue){
  tcp::acceptor acceptor{io_context, {tcp::v4(), 8888}};
  tcp::socket socket{io_context};
  acceptor.accept(socket); // blocks until connection is ready
  websocket::stream<tcp::socket> ws{std::move(socket)};

  while (not signaled) {
    // TODO ws.read();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    //std::cout << j_set_color << std::endl;
    const auto command = cmd::deserialize(j_set_color);
    cmd_queue.push(command);
    cmd_queue.push(cmd::SetBrightness{99});
  }
}
*/

int main(){
  std::signal(SIGTERM, signal_handler);
  std::signal(SIGINT, signal_handler);

  Lightbulb bulb("LED");
  cmd::CommandExecutor executor(bulb);
  boost::lockfree::queue<cmd::Command> cmd_queue(100);

  net::io_context io_context(1);
  /*net::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto){
    std::cout << "Signal handler called" << std::endl;
    signaled = true;
    io_context.stop();
  });*/
  // TODO
  //https://www.boost.org/doc/libs/1_69_0/libs/beast/example/websocket/server/async/websocket_server_async.cpp
  //std::thread receive_task(receive_commands, std::ref(io_context), std::ref(cmd_queue));

  std::make_shared<Listener>(io_context, tcp::endpoint{tcp::v4(), 8888})->run();
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
