#include "qchat/network.hpp"

#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#include <array>
#include <atomic>
#include <charconv>
#include <iostream>
#include <random>
#include <string_view>
#include <thread>

namespace {
std::string server_id() {
    std::random_device random;
    constexpr std::string_view digits = "0123456789abcdef";
    std::string id;
    for (int i = 0; i < 32; ++i) {
        id += digits[random() & 15U];
    }
    return id;
}
} // namespace

int main(int argc, char **argv) {
    try {
        std::string address = "127.0.0.1";
        int port = 9090;
        for (int i = 1; i < argc; ++i) {
            const std::string_view option(argv[i]);
            if (option == "--help") {
                std::cout
                    << "Usage: qchat-server [--bind IP] [--port 1-65535]\n"
                       "Anonymous public room; in-memory history; no authentication or TLS.\n";
                return 0;
            }
            if ((option != "--bind" && option != "--port") || i + 1 == argc) {
                throw std::invalid_argument("Expected --bind IP or --port 1-65535");
            }
            const std::string_view value(argv[++i]);
            if (option == "--bind") {
                address = value;
                std::array<unsigned char, 16> buffer{};
                if (inet_pton(AF_INET, address.c_str(), buffer.data()) != 1 &&
                    inet_pton(AF_INET6, address.c_str(), buffer.data()) != 1) {
                    throw std::invalid_argument("--bind requires an IPv4 or IPv6 address");
                }
            } else {
                const auto [end, error] =
                    std::from_chars(value.data(), value.data() + value.size(), port);
                if (error != std::errc{} || end != value.data() + value.size() || port < 1 ||
                    port > 65535) {
                    throw std::invalid_argument("Port must be an integer from 1 to 65535");
                }
            }
        }
        sigset_t signals;
        sigemptyset(&signals);
        sigaddset(&signals, SIGINT);
        sigaddset(&signals, SIGTERM);
        if (pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0) {
            throw std::runtime_error("Cannot configure termination signals");
        }
        std::atomic<bool> listening = false;
        qchat::Server server(address, port, server_id(), [&](int bound_port) {
            listening = true;
            std::cout << "QChat listening on " << address << ':' << bound_port << '\n'
                      << std::flush;
        });
        std::jthread signal_thread([&](const std::stop_token &stop) {
            bool termination_requested = false;
            while (!stop.stop_requested()) {
                if (termination_requested && listening) {
                    server.stop();
                    break;
                }
                const timespec timeout{0, 100'000'000};
                if (sigtimedwait(&signals, nullptr, &timeout) > 0) {
                    termination_requested = true;
                }
            }
        });
        server.serve();
    } catch (const std::exception &error) {
        std::cerr << "qchat-server: " << error.what() << '\n';
        return 1;
    }
}
