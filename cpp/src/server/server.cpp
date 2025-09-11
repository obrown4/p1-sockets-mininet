#include <cstddef>
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

int Server::measure_rtt(int clientfd)
{
  std::vector<double> rtts;
  char buffer[MAX_MSG_SIZE];

  if (recv(clientfd, buffer, 1, 0) < 0)
  {
    spdlog::error("Error: failed to receive data from client");
    return -1;
  }

  // rtt calculations for 7 messages
  for (int i = 0; i < 7; ++i)
  {
    auto start = std::chrono::high_resolution_clock::now();
    if (send(clientfd, &ACK_MSG, sizeof(ACK_MSG), 0) < 0)
    {
      spdlog::error("Error: failed to send data to client");
      return -1;
    }

    int bytes_recvd = recv(clientfd, buffer, 1, 0);
    if (bytes_recvd <= 0)
    {
      spdlog::error("Error: failed to receive data from client");
      return -1;
    }
    auto end = std::chrono::high_resolution_clock::now();

    // take measurement
    std::chrono::duration<double> rtt = end - start;
    spdlog::debug("RTT{}: {}", i, rtt.count() * 1000);
    rtts.push_back(rtt.count() * 1000); // one way
  }

  if (send(clientfd, &ACK_MSG, sizeof(ACK_MSG), 0) < 0)
  {
    spdlog::error("Error: failed to send data to client");
    return -1;
  }

  // calcualte rtt over last 3 recvd messages
  double avg = std::accumulate(rtts.begin() + 4, rtts.end(), 0.0) / 3.0;
  return avg;
}

double Server::measure_bandwidth(Perf &perf, int clientfd)
{
  size_t total_bytes = 0;
  char buffer[MAX_MSG_SIZE];

  bool open = true;
  auto start = std::chrono::high_resolution_clock::now();
  double propogation = 0.0;

  while (open)
  {
    size_t bytes_recvd = 0;
    do
    {
      int msg =
          recv(clientfd, buffer + bytes_recvd, MAX_MSG_SIZE - bytes_recvd, 0);
      if (msg < 0)
      {
        spdlog::error("Error: failed to receive data from client");
        return -1;
      }
      if (msg == 0)
      {
        open = false;
        break;
      }

      bytes_recvd += msg;
    } while (bytes_recvd < MAX_MSG_SIZE);

    if (bytes_recvd > 0)
    {
      total_bytes += bytes_recvd;
      send(clientfd, &ACK_MSG, sizeof(ACK_MSG), 0);
      ++propogation;
    }
  }
  auto end = std::chrono::high_resolution_clock::now();

  perf.kbytes = total_bytes / 1000;

  // convert to Mb and sec -> Mbps
  double mb_recvd = (static_cast<double>(total_bytes) * 8.0) / (1000 * 1000);
  assert(mb_recvd > 0);

  int rtt_in_sec = perf.rtt / 1000;

  std::chrono::duration<double> elapsed = end - start;
  assert(elapsed.count() > 0);

  double transmission_delay = elapsed.count() - (rtt_in_sec * propogation);

  double bandwidth = mb_recvd / transmission_delay; // in Mbps
  return bandwidth;
}

void Server::handle_connection(int clientfd)
{
  Perf perf{};
  perf.rtt = Server::measure_rtt(clientfd);
  spdlog::debug("Measured RTT = {}ms", perf.rtt);
  if (perf.rtt < 0)
  {
    spdlog::error("Error: failed to measure RTT");
    return;
  }

  perf.rate = Server::measure_bandwidth(perf, clientfd);
  assert(perf.rate >= 0);

  spdlog::info("Received={} KB, Rate={:03.3f} Mbps, RTT={}ms", perf.kbytes,
               perf.rate, perf.rtt);
}

int Server::make_server_sockaddr(sockaddr_in *addr, int port)
{
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = INADDR_ANY;
  addr->sin_port = htons(port);
  return 0;
}

int Server::start_server(Opts &opts)
{
  // Prepare address structure
  sockaddr_in addr{};
  make_server_sockaddr(&addr, opts.port);

  spdlog::debug("Binding to port {}", opts.port);

  // Create socket
  int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd < 0)
  {
    spdlog::error("Error: failed to create socket");
    return -1;
  }

  // bind socket
  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    spdlog::error("Error: failed to bind socket to port %d", opts.port);
    close(sockfd);
    return -1;
  }

  spdlog::info("iPerfer server started");
  spdlog::debug("Listening on port %d", opts.port);

  if (listen(sockfd, 5))
  {
    spdlog::error("Error: failed to listen on socket");
    close(sockfd);
    return -1;
  }

  // accept incoming connections
  int clientfd = accept(sockfd, nullptr, nullptr);
  if (clientfd < 0)
  {
    spdlog::error("Error: failed to accept incoming connection");
    return -1;
  }
  spdlog::info("Client connected");
  handle_connection(clientfd);
  close(sockfd);
  return 0;
}

std::optional<Server::Opts>
Server::get_server_options(cxxopts::ParseResult &opts)
{
  Server::Opts server_opts;
  if (!opts.contains("p"))
  {
    spdlog::error("Error: server mode requires a port number (-p)");
    return std::nullopt;
  }
  server_opts.port = opts["p"].as<int>();

  if (server_opts.port < 1024 || server_opts.port > 65535)
  {
    spdlog::error("Error: port number must be in the range of [1024, 65535]");
    return std::nullopt;
  }

  return server_opts;
}