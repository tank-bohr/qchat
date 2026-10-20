#include "qchat/network.hpp"
#include "qchat/service.hpp"

namespace qchat {
namespace {
namespace transport = apache::thrift::transport;
namespace protocol = apache::thrift::protocol;

std::shared_ptr<apache::thrift::TConfiguration> configuration() {
    return std::make_shared<apache::thrift::TConfiguration>(max_frame_bytes, max_frame_bytes, 16);
}

class LimitedFrames final : public transport::TTransportFactory {
  public:
    std::shared_ptr<transport::TTransport>
    getTransport(std::shared_ptr<transport::TTransport> underlying) override {
        return std::make_shared<transport::TFramedTransport>(std::move(underlying),
                                                             configuration());
    }
};

class ReadyHandler final : public apache::thrift::server::TServerEventHandler {
  public:
    explicit ReadyHandler(std::function<void()> ready) : ready_(std::move(ready)) {}
    void preServe() override { ready_(); }

  private:
    std::function<void()> ready_;
};

std::shared_ptr<protocol::TBinaryProtocol>
make_protocol(const std::shared_ptr<transport::TTransport> &transport) {
    auto protocol = std::make_shared<protocol::TBinaryProtocol>(transport);
    protocol->setStringSizeLimit(8192);
    protocol->setContainerSizeLimit(100);
    return protocol;
}
} // namespace

Connection::Connection(const std::string &host, int port)
    : socket_(std::make_shared<transport::TSocket>(host, port, configuration())),
      transport_(std::make_shared<transport::TFramedTransport>(socket_, configuration())),
      client_(make_protocol(transport_)) {
    socket_->setConnTimeout(rpc_timeout_ms);
    socket_->setRecvTimeout(rpc_timeout_ms);
    socket_->setSendTimeout(rpc_timeout_ms);
    transport_->open();
}

Connection::~Connection() {
    // Close the socket directly: TFramedTransport::close() may flush an incomplete RPC.
    socket_->close();
}

rpc::Message Connection::post(const std::string &name, const std::string &text) {
    rpc::Message result;
    client_.post(result, name, text);
    return result;
}

rpc::MessagePage Connection::poll(const std::string &server_id, std::int64_t cursor,
                                  std::int32_t limit) {
    rpc::MessagePage result;
    client_.poll(result, server_id, cursor, limit);
    return result;
}

Server::Server(const std::string &address, int port, std::string server_id,
               std::function<void(int)> ready, std::size_t history_size)
    : socket_(std::make_shared<transport::TServerSocket>(address, port)),
      server_(std::make_shared<rpc::ChatProcessor>(std::make_shared<Service>(
                  std::make_shared<Store>(std::move(server_id), history_size))),
              socket_, std::make_shared<LimitedFrames>(),
              std::make_shared<protocol::TBinaryProtocolFactory>(8192, 100, true, true)) {
    socket_->setRecvTimeout(10000);
    socket_->setSendTimeout(rpc_timeout_ms);
    socket_->setInterruptableChildren(true);
    if (ready) {
        // preServe runs after listen(); the socket's listen callback runs before it.
        server_.setServerEventHandler(std::make_shared<ReadyHandler>(
            [socket = socket_, ready = std::move(ready)] { ready(socket->getPort()); }));
    }
    server_.setConcurrentClientLimit(128);
}

void Server::serve() { server_.serve(); }
void Server::stop() { server_.stop(); }
} // namespace qchat
