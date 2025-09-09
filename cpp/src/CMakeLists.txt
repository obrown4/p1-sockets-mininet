add_executable(iPerfer iPerfer.cpp server/server.cpp)
# Add your setup code above this line, then uncomment the line below

target_link_libraries(iPerfer spdlog::spdlog cxxopts::cxxopts)