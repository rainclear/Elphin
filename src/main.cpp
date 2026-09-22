#include <csignal>
#include "server/elphin_server.hpp"
#include "common/logger.hpp"

int main() {
    // Ignore SIGPIPE to prevent server crashes on sudden client disconnects
    ::signal(SIGPIPE, SIG_IGN);

    LOG_INFO("Starting Elphin In-Memory KV Store...");

    elphin::server::ServerConfig config;
    config.port = 6379;
    config.worker_threads = 2;

    elphin::server::ElphinServer server(config);
    server.start();

    return 0;
}