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

int MAX_MSG_SIZE = 1024 * 80; // 80KB
char ACK_MSG = 'A';

int measure_rtt(int clientfd) {
  clock_t start, end;
  std::vector<double> rtts;
  char buffer[MAX_MSG_SIZE];

  // rtt calculations for 7 messages
  for (int i = 0; i < 7; ++i) {
    start = clock();
    int bytes_recvd = recv(clientfd, buffer, MAX_MSG_SIZE, 0);
    end = clock();
    if (bytes_recvd < 0) {
      spdlog::error("Error: failed to receive data from client");
      return 0;
    }
    assert(bytes_recvd == 1);

    double rtt = double(end - start) / CLOCKS_PER_SEC;
    rtts.push_back(rtt);
    send(clientfd, &ACK_MSG, 1, 0);
  }

  assert(rtts.size() == 7);

  // calcualte rtt over last 3 recvd messages
  double avg = std::accumulate(rtts.begin() + 4, rtts.end(), 0.0) / 3.0;
  return static_cast<int>(avg * 1000); // in ms
}

double measure_bandwidth(Perf &p, int clientfd) {
  clock_t start, end;
  int total_bytes = 0;
  char buffer[MAX_MSG_SIZE];

  start = clock();
  while (int bytes_recvd = recv(clientfd, buffer, MAX_MSG_SIZE, 0) > 0) {
    if (bytes_recvd < 0) {
      spdlog::error("Error: failed to receive data from client");
      return -1;
    }
    total_bytes += bytes_recvd;
    send(clientfd, &ACK_MSG, 1, 0);
  }
  end = clock();
  p.kbytes_recvd = total_bytes / 1000;

  // convert to Mb and sec -> Mbps
  int mb_recvd = total_bytes / (1000 * 1000);
  int rtt_in_sec = p.rtt / 1000;

  double total_time = double(end - start) / CLOCKS_PER_SEC; // in ms
  double transmission_delay = total_time - rtt_in_sec;

  double bandwidth = mb_recvd / transmission_delay; // in Mbps
  return bandwidth;
}

void handle_connection(int clientfd) {
  Perf p{};
  p.rtt = measure_rtt(clientfd);
  if (p.rtt == 0) {
    spdlog::error("Error: failed to measure RTT");
    return;
  }

  p.rate = measure_bandwidth(p, clientfd);
  assert(p.rate >= 0);

  spdlog::info("Received=%d KB, Rate=%.3f Mbps, RTT=%dms", p.kbytes_recvd,
               p.rate, p.rtt);
}

int start_server(Server &server) {
  // Prepare address structure
  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(server.port);

  // Create socket
  int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_TCP);
  if (sockfd < 0) {
    spdlog::error("Error: failed to create socket");
    return 1;
  }

  // bind socket
  if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    spdlog::error("Error: failed to bind socket to port %d", server.port);
    close(sockfd);
    return 1;
  }

  spdlog::info("iPerfer server started");
  spdlog::info("Listening on port %d", server.port);

  if (listen(sockfd, 5)) {
    spdlog::error("Error: failed to listen on socket");
    close(sockfd);
    return 1;
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

std::optional<Server> get_server_options(cxxopts::ParseResult &opts) {
  Server s;
  if (!opts.contains("p")) {
    spdlog::error("Error: server mode requires a port number (-p)");
    return std::nullopt;
  }
  s.port = opts["p"].as<int>();

  if (s.port < 1024 || s.port > 65535) {
    spdlog::error("Error: port number must be in the range of [1024, 65535]");
    return std::nullopt;
  }

  return s;
}