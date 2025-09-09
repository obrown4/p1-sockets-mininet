#include <cxxopts.hpp>
#include <optional>
#include <spdlog/spdlog.h>

#pragma once

struct Server {
  int port;
};

void handle_connection(int clientfd);
int start_server(Server &server);
std::optional<Server> get_server_options(cxxopts::ParseResult &opts);
double measure_bandwidth(int clientfd);
int measure_rtt(int clientfd);
