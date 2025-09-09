#include <chrono>
#include <cxxopts.hpp>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "server/server.h"

struct Client {
  std::string hostname;
  int port;
  std::chrono::duration<double> time;
};

std::optional<Client> getClientOptions(cxxopts::ParseResult &opts) {
  Client c;
  if (!opts.contains("p") || !opts.contains("h") || !opts.contains("t")) {
    spdlog::error("Error: client mode requires a port number (-p), hostname "
                  "(-h), and time (-t)");
    return std::nullopt;
  }
  c.port = opts["p"].as<int>();
  c.hostname = opts["h"].as<std::string>();
  c.time = opts["t"].as<std::chrono::duration<double>>();

  if (c.port < 1024 || c.port > 65535) {
    spdlog::error("Error: port number must be in the range of [1024, 65535]");
    return std::nullopt;
  }

  if (c.time.count() <= 0) {
    spdlog::error("Error: time argument must be greater than 0");
    return std::nullopt;
  }

  return c;
}

void clientMode(Client &c) {}

int main(int argc, char *argv[]) {
  spdlog::info("Starting iPerfer...");
  cxxopts::Options options("iPerfer",
                           "Tool to estimate throughput between hosts");
  options.add_options()("s,server", "Run in server mode")(
      "c,client", "Run in client mode")("p,port", "Port number",
                                        cxxopts::value<int>())(
      "h, hostname", "Hostname or IP address of the server",
      cxxopts::value<std::string>())(
      "t,time", "Time in seconds to transmit for (client mode)",
      cxxopts::value<double>());

  auto result = options.parse(argc, argv);

  bool isServer = result.contains("s");
  bool isClient = result.contains("c");

  if (isServer == isClient) {
    spdlog::error("Error: specify either server mode (-s) or client mode (-c)");
    spdlog::info(options.help());
    return 1;
  }

  if (isServer) {
    auto s = get_server_options(result);
    if (!s) {
      return 1;
    }
    if (start_server(*s)) {
      return 1;
    }
  } else {
    auto c = getClientOptions(result);
    if (!c) {
      return 1;
    }
    clientMode(*c);
  }

  return 0;
}