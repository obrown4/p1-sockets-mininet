#include <chrono>
#include <ctime>
#include <cxxopts.hpp>
#include <netdb.h>
#include <numeric>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <time.h>
#include <vector>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "client.h"

int Client::measure_rtt(int clientfd) {
  clock_t start, end;
  std::vector<double> rtts;
  char buffer[MAX_MSG_SIZE];

  // rtt calculations for 8 messages
  for (int i = 0; i < 8; ++i) {
    start = clock();
    send(clientfd, &SMALL_MSG, 1, 0);
    int bytes_recvd = recv(clientfd, buffer, MAX_MSG_SIZE, 0);
    if (bytes_recvd < 0) {
      spdlog::error("Error: failed to receive data from server");
      return 0;
    }
    assert(bytes_recvd == 1);
    end = clock();
    double rtt = double(end - start) / CLOCKS_PER_SEC;
    rtts.push_back(rtt);
  }

  assert(rtts.size() == 7);

  // calcualte rtt over last 4 recvd messages
  double avg = std::accumulate(rtts.begin() + 3, rtts.end(), 0.0) / 3.0;
  return static_cast<int>(avg * 1000); // in ms
}

double Client::measure_bandwidth(Perf &perf, Opts &opts, int clientfd) {
  int bytes_sent = 0;
  char buffer[MAX_MSG_SIZE];
  std::string LARGE_MSG(MAX_MSG_SIZE, '\0');

  clock_t start = clock();
  while (double(clock() - start) / CLOCKS_PER_SEC < opts.time.count()) {
    bytes_sent += send(clientfd, LARGE_MSG.c_str(), MAX_MSG_SIZE, 0);
    int bytes_recvd = recv(clientfd, buffer, MAX_MSG_SIZE, 0);
    if (bytes_recvd < 0) {
      spdlog::error("Error: failed to receive data from server");
      return -1;
    }
  }

  perf.kbytes = bytes_sent / 1000;

  // convert to Mb and sec -> Mbps
  int mb_sent = bytes_sent / (1000 * 1000);
  int rtt_in_sec = perf.rtt / 1000;

  double transmission_delay = opts.time.count() - rtt_in_sec;

  double bandwidth = mb_sent / transmission_delay; // in Mbps
  return bandwidth;
}

int make_client_sockaddr(sockaddr_in &addr, const std::string &hostname,
                         int port) {
  addr.sin_family = AF_INET;
  struct hostent *host = gethostbyname(hostname.c_str());
  if (host == nullptr) {
    spdlog::error("Error: unknown host {}", hostname);
    return -1;
  }
  memcpy(&addr.sin_addr, host->h_addr, host->h_length);
  addr.sin_port = htons(port);
  return 0;
}

int Client::start_client(Opts &opts) {
  // Prepare address structure
  sockaddr_in addr{};
  if (make_client_sockaddr(addr, opts.hostname, opts.port) == -1) {
    return -1;
  }

  // Create socket
  int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_TCP);
  if (sockfd < 0) {
    spdlog::error("Error: failed to create socket");
    return -1;
  }

  if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    spdlog::error("Error: failed to connect to server {}:{}", opts.hostname,
                  opts.port);
    close(sockfd);
    return -1;
  }

  spdlog::info("Connected to server %s:%d", opts.hostname, opts.port);

  Perf perf{};
  perf.rtt = measure_rtt(sockfd);
  if (perf.rtt == 0) {
    spdlog::error("Error: failed to measure RTT");
    close(sockfd);
    return -1;
  }

  perf.rate = measure_bandwidth(perf, opts, sockfd);
  if (perf.rate == -1) {
    close(sockfd);
    return -1;
  }

  spdlog::info("Sent=%d KB, Rate=%.3f Mbps, RTT=%dms", perf.kbytes, perf.rate,
               perf.rtt);

  close(sockfd);
  return 0;
}

std::optional<Client::Opts>
Client::get_client_options(cxxopts::ParseResult &opts) {
  Opts c;
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
