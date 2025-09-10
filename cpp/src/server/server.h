#include <cxxopts.hpp>
#include <netinet/in.h>
#include <optional>
#include <spdlog/spdlog.h>

#include "../perf.h"

#pragma once

class Server {
public:
  struct Opts {
    int port;
  };
  int start_server(Opts &server);
  std::optional<Opts> get_server_options(cxxopts::ParseResult &opts);

private:
  // contstnants
  int MAX_MSG_SIZE = 1024 * 80; // 80KB
  char ACK_MSG = 'A';

  // funcs
  void handle_connection(int clientfd);
  double measure_bandwidth(Perf &perf, int clientfd);
  int measure_rtt(int clientfd);
  int make_server_sockaddr(sockaddr_in *addr, int port);
};
