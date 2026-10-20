#pragma once

#include "Chat.h"

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TSocket.h>

#include <functional>

namespace qchat {
inline constexpr int max_frame_bytes = 1024 * 1024;
inline constexpr int rpc_timeout_ms = 3000;

// One instance per worker; Thrift clients must not be shared across threads.
class Connection {
  public:
    Connection(const std::string &host, int port);
    ~Connection();
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;
    [[nodiscard]] rpc::Message post(const std::string &name, const std::string &text);
    [[nodiscard]] rpc::MessagePage poll(const std::string &server_id, std::int64_t cursor,
                                        std::int32_t limit = 100);

  private:
    std::shared_ptr<apache::thrift::transport::TSocket> socket_;
    std::shared_ptr<apache::thrift::transport::TFramedTransport> transport_;
    rpc::ChatClient client_;
};

class Server {
  public:
    Server(const std::string &address, int port, std::string server_id,
           std::function<void(int)> ready = {}, std::size_t history_size = 1000);
    void serve();
    void stop();

  private:
    std::shared_ptr<apache::thrift::transport::TServerSocket> socket_;
    apache::thrift::server::TThreadedServer server_;
};
} // namespace qchat
