#pragma once

#include "commands.hpp"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/lockfree/queue.hpp>

#include <iostream>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>

// Report a failure
void fail(beast::error_code ec, char const *what) {
  std::cout << what << ": " << ec.message() << "\n";
}

// Echoes back all received WebSocket messages
class Session : public std::enable_shared_from_this<Session> {
  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer buffer_;
  cmd::CommandQueue& cmd_queue_;

public:
  // Take ownership of the socket
  explicit Session(tcp::socket &&socket, cmd::CommandQueue& cmd_queue) : ws_(std::move(socket)), cmd_queue_(cmd_queue) {}

  // Start the asynchronous operation
  void run() {
    // Set suggested timeout settings for the websocket
    ws_.set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::server));

    // Accept the websocket handshake
    ws_.async_accept(
        beast::bind_front_handler(&Session::on_accept, shared_from_this()));
  }

  void on_accept(beast::error_code ec) {
    if (ec)
      return fail(ec, "accept");

    // Read a message
    do_read();
  }

  void do_read() {
    // Read a message into our buffer
    ws_.async_read(buffer_, beast::bind_front_handler(&Session::on_read,
                                                      shared_from_this()));
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    // This indicates that the Session was closed
    if (ec == websocket::error::closed)
      return;

    if (ec)
      fail(ec, "read");

    auto data = reinterpret_cast<char*>(buffer_.data().data());
    const auto json_cmd = json::parse(data, data + buffer_.data().size());
    buffer_.consume(buffer_.size());
    const auto command = cmd::deserialize(json_cmd);
    cmd_queue_.push(command);

    // TODO answer the client?

    do_read();

    // Echo the message
    /*ws_.text(ws_.got_text());
    ws_.async_write(
        buffer_.data(),
        beast::bind_front_handler(&Session::on_write, shared_from_this()));
    */
  }

  void on_write(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec)
      return fail(ec, "write");

    // Clear the buffer
    buffer_.consume(buffer_.size());

    do_read();
  }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the Sessions
class Listener : public std::enable_shared_from_this<Listener> {
  net::io_context &ioc_;
  tcp::acceptor acceptor_;
  cmd::CommandQueue& cmd_queue_;

public:
  Listener(net::io_context &ioc, tcp::endpoint endpoint, cmd::CommandQueue& cmd_queue)
      : ioc_(ioc), acceptor_(ioc), cmd_queue_(cmd_queue) {
    beast::error_code ec;

    // Open the acceptor
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
      fail(ec, "open");
      return;
    }

    // Allow address reuse
    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
      fail(ec, "set_option");
      return;
    }

    // Bind to the server address
    acceptor_.bind(endpoint, ec);
    if (ec) {
      fail(ec, "bind");
      return;
    }

    // Start listening for connections
    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
      fail(ec, "listen");
      return;
    }
  }

  // Start accepting incoming connections
  void run() { do_accept(); }

private:
  void do_accept() {
    // The new connection gets its own strand
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(&Listener::on_accept, shared_from_this()));
  }

  void on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
      fail(ec, "accept");
    } else {
      // Create the Session and run it
      std::make_shared<Session>(std::move(socket), cmd_queue_)->run();
    }

    // Accept another connection
    do_accept();
  }
};
