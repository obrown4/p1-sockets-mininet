#include <chrono>
#include <cxxopts.hpp>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "../perf.h"

class Client {
public:
  struct Opts {
    std::string hostname;
    int port;
    std::chrono::duration<double> time;
  };
  std::optional<Opts> get_client_options(cxxopts::ParseResult &opts);
  int start_client(Opts &client);

private:
  // constants
  int MAX_MSG_SIZE = 1024 * 80; // 80KB
  char SMALL_MSG = 'M';

  // funcs
  int measure_rtt(int clientfd);
  double measure_bandwidth(Perf &perf, Opts &client, int clientfd);
};
