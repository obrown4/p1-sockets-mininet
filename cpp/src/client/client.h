#include <chrono>
#include <cxxopts.hpp>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

struct Client {
  std::string hostname;
  int port;
  std::chrono::duration<double> time;
};

std::optional<Client> get_client_options(cxxopts::ParseResult &opts);
int start_client(Client &client);