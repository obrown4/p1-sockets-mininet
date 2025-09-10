#include <cxxopts.hpp>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "client/client.h"
#include "server/server.h"

int main(int argc, char *argv[]) {
  cxxopts::Options options("iPerfer",
                           "Tool to estimate throughput between hosts");
  options.add_options()("s,server", "Run in server mode")(
      "c,client", "Run in client mode")("p,port", "Port number",
                                        cxxopts::value<int>())(
      "h, hostname", "Hostname or IP address of the server",
      cxxopts::value<std::string>())(
      "t,time", "Time in seconds to transmit for (client mode)",
      cxxopts::value<double>())("d, debug", "Debug mode");

  auto result = options.parse(argc, argv);

  bool isServer = result.contains("s");
  bool isClient = result.contains("c");

  if (isServer == isClient) {
    spdlog::error("Error: specify either server mode (-s) or client mode (-c)");
    spdlog::info(options.help());
    return 1;
  }

  if (result.contains("d")) {
    spdlog::set_level(spdlog::level::debug);
  }

  if (isServer) {
    Server s;
    auto opts = s.get_server_options(result);
    if (!opts) {
      return 1;
    }
    if (s.start_server(*opts) == 1) {
      return 1;
    }
  } else {
    Client c;
    auto opts = c.get_client_options(result);
    if (!opts) {
      return 1;
    }
    if (c.start_client(*opts) == 1) {
      return 1;
    }
  }

  return 0;
}