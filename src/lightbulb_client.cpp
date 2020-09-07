#include <nlohmann/json.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

int main(){
  // The io_context is required for all I/O
  net::io_context io_contextc;

  // These objects perform our I/O
  tcp::resolver resolver{io_contextc};
  websocket::stream<tcp::socket> ws{io_contextc};

  const auto host = "127.0.0.1";

  // Look up the domain name
  auto const results = resolver.resolve(host, "8888");

  // Make the connection on the IP address we get from a lookup
  net::connect(ws.next_layer(), results.begin(), results.end());
  ws.handshake(host, "/");
}
