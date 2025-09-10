#include <cxxopts.hpp>
#include <numeric>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "server.h"

int Server::measure_rtt(int clientfd) {
  clock_t start, end;
  std::vector<double> rtts;
  char buffer[MAX_MSG_SIZE];

  // rtt calculations for 7 messages
  for (int i = 0; i < 7; ++i) {
    start = clock();
    int bytes_recvd = recv(clientfd, buffer, MAX_MSG_SIZE, 0);
    end = clock();
    if (bytes_recvd <= 0) {
      spdlog::error("Error: failed to receive data from client");
      return -1;
    }
    // assert(bytes_recvd == 1);

    double rtt = double(end - start) / CLOCKS_PER_SEC;
    spdlog::debug("RTT{} = {}ms", i, rtt * 1000);
    rtts.push_back(rtt * 1000);
    send(clientfd, &ACK_MSG, sizeof(ACK_MSG), 0);
  }

  assert(rtts.size() == 7);

  // calcualte rtt over last 3 recvd messages
  double avg = std::accumulate(rtts.begin() + 4, rtts.end(), 0.0) / 3.0;
  return avg;
}

double Server::measure_bandwidth(Perf &perf, int clientfd) {
  clock_t start, end;
  size_t total_bytes = 0;
  size_t bytes_recvd;
  char buffer[MAX_MSG_SIZE];

  start = clock();
  while ((bytes_recvd = recv(clientfd, buffer, MAX_MSG_SIZE, 0)) > 0) {
    total_bytes += bytes_recvd;
    send(clientfd, &ACK_MSG, sizeof(ACK_MSG), 0);
  }
  end = clock();
  perf.kbytes = total_bytes / 1000;

  // convert to Mb and sec -> Mbps
  double mb_recvd = static_cast<double>(total_bytes) * 8 / (1000 * 1000);
  assert(mb_recvd > 0);

  int rtt_in_sec = perf.rtt / 1000;

  double total_time = double(end - start) / CLOCKS_PER_SEC; // in ms
  assert(total_time > 0);

  double transmission_delay = total_time - rtt_in_sec;

  spdlog::debug(
      "Mb Recvd: {}, Total Time: {}s, RTT: {}s, Transmission Delay: {}s",
      mb_recvd, total_time, rtt_in_sec, transmission_delay);

  double bandwidth = mb_recvd / transmission_delay; // in Mbps
  return bandwidth;
}

void Server::handle_connection(int clientfd) {
  Perf perf{};
  perf.rtt = Server::measure_rtt(clientfd);
  spdlog::debug("Measured RTT = {}ms", perf.rtt);
  if (perf.rtt < 0) {
    spdlog::error("Error: failed to measure RTT");
    return;
  }

  perf.rate = Server::measure_bandwidth(perf, clientfd);
  assert(perf.rate >= 0);

  spdlog::info("Received={} KB, Rate={:03.3f} Mbps, RTT={}ms", perf.kbytes,
               perf.rate, perf.rtt);
}

int Server::make_server_sockaddr(sockaddr_in *addr, int port) {
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = INADDR_ANY;
  addr->sin_port = htons(port);
  return 0;
}

int Server::start_server(Opts &opts) {
  // Prepare address structure
  sockaddr_in addr{};
  make_server_sockaddr(&addr, opts.port);

  spdlog::info("Binding to port {}", opts.port);

  // Create socket
  int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd < 0) {
    spdlog::error("Error: failed to create socket");
    return -1;
  }

  // bind socket
  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    spdlog::error("Error: failed to bind socket to port %d", opts.port);
    close(sockfd);
    return -1;
  }

  spdlog::info("iPerfer server started");
  spdlog::debug("Listening on port %d", opts.port);

  if (listen(sockfd, 5)) {
    spdlog::error("Error: failed to listen on socket");
    close(sockfd);
    return -1;
  }

  // accept incoming connections
  while (true) {
    int clientfd = accept(sockfd, nullptr, nullptr);
    if (clientfd < 0) {
      spdlog::error("Error: failed to accept incoming connection");
      continue;
    }
    spdlog::info("Client connected");
    handle_connection(clientfd);
    close(clientfd);
  }
}

std::optional<Server::Opts>
Server::get_server_options(cxxopts::ParseResult &opts) {
  Server::Opts server_opts;
  if (!opts.contains("p")) {
    spdlog::error("Error: server mode requires a port number (-p)");
    return std::nullopt;
  }
  server_opts.port = opts["p"].as<int>();

  if (server_opts.port < 1024 || server_opts.port > 65535) {
    spdlog::error("Error: port number must be in the range of [1024, 65535]");
    return std::nullopt;
  }

  return server_opts;
}