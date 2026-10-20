#pragma once

#include "qchat/network.hpp"

#include <future>
#include <thread>

class ServerFixture {
  public:
    explicit ServerFixture(std::size_t capacity = 1000)
        : server_(
              "127.0.0.1", 0, "test-epoch", [this](int port) { ready_.set_value(port); }, capacity),
          thread_([this] {
              try {
                  server_.serve();
              } catch (...) {
                  try {
                      ready_.set_exception(std::current_exception());
                  } catch (const std::future_error &) {
                      failure_ = std::current_exception();
                  }
              }
          }) {
        auto future = ready_.get_future();
        if (future.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
            server_.stop();
            thread_.join();
            throw std::runtime_error("Server did not start");
        }
        try {
            port_ = future.get();
        } catch (...) {
            thread_.join();
            throw;
        }
    }
    ~ServerFixture() {
        server_.stop();
        thread_.join();
    }
    ServerFixture(const ServerFixture &) = delete;
    ServerFixture &operator=(const ServerFixture &) = delete;
    [[nodiscard]] int port() const { return port_; }

  private:
    std::promise<int> ready_;
    qchat::Server server_;
    std::exception_ptr failure_;
    std::thread thread_;
    int port_{};
};
