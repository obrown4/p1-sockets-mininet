#include <cxxopts.hpp>
#include <spdlog/spdlog.h>


int main(int argc, char* argv[]) {
    spdlog::info("Starting iPerfer...");
    cxxopts::Options options("iPerfer", "Tool to estimate throughput between hosts");
    
    return 0;
}