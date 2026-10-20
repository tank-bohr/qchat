#include "qchat/service.hpp"
#include "server_fixture.hpp"
#include "test_support.hpp"

#include <array>
#include <future>

namespace {
void listening_before_ready() {
    // Connect inside the readiness callback so an early notification fails deterministically.
    ServerFixture server(1000, [](int port) {
        apache::thrift::transport::TSocket probe("127.0.0.1", port);
        probe.setConnTimeout(1000);
        probe.open();
        probe.close();
    });
    qchat::Connection client("127.0.0.1", server.port());
    CHECK(client.post("Guest", "Ready").id == 1);
}

void exchange() {
    expect_throw<std::invalid_argument>([] { const qchat::Service service(nullptr); });
    ServerFixture server(3);
    qchat::Connection alice("127.0.0.1", server.port());
    qchat::Connection bob("127.0.0.1", server.port());
    const auto first = alice.post("Alice", "Hello 世界 👋");
    CHECK(first.id == 1);
    auto page = bob.poll("", 0);
    CHECK(page.reset && page.messages.size() == 1);
    CHECK(page.messages.front().text == first.text);
    CHECK(page.messages.front().displayName == "Alice");
    CHECK(page.messages.front().timestampMs == first.timestampMs);
    CHECK(bob.post("Bob", "Hi Alice").id == 2);
    CHECK(alice.poll(page.serverId, 1).messages.front().displayName == "Bob");
    expect_throw<qchat::rpc::InvalidRequest>([&] { (void)alice.post("", "hello"); });
    expect_throw<qchat::rpc::InvalidRequest>(
        [&] { (void)alice.post("A", std::string(4097, 'x')); });
    expect_throw<qchat::rpc::InvalidRequest>([&] { (void)alice.poll(page.serverId, -1); });
    expect_throw<qchat::rpc::InvalidRequest>([&] { (void)alice.poll(page.serverId, 0, 101); });
    // Application errors keep the connection usable.
    CHECK(alice.post("Alice", "Third").id == 3);
    CHECK(alice.post("Alice", "Fourth").id == 4);
    page = bob.poll(page.serverId, 0, 1);
    CHECK(page.reset && page.messages.front().id == 2 && page.nextCursor == 2);
    CHECK(bob.poll(page.serverId, 4).messages.empty());
    qchat::Connection reconnected("127.0.0.1", server.port());
    CHECK(reconnected.poll("previous-server", 4).reset);
}

void parallel_clients() {
    ServerFixture server;
    std::vector<std::future<void>> clients;
    clients.reserve(8);
    for (int i = 0; i < 8; ++i) {
        clients.push_back(std::async(std::launch::async, [&] {
            qchat::Connection client("127.0.0.1", server.port());
            for (int j = 0; j < 25; ++j) {
                (void)client.post("Guest", "Concurrent message");
            }
        }));
    }
    for (auto &client : clients) {
        client.get();
    }
    qchat::Connection reader("127.0.0.1", server.port());
    CHECK(reader.poll("", 0).messages.size() == 100);
    const auto page = reader.poll("test-epoch", 100);
    CHECK(page.messages.size() == 100 && page.nextCursor == 200);
}

void oversized_frame() {
    ServerFixture server;
    apache::thrift::transport::TSocket socket("127.0.0.1", server.port());
    socket.setRecvTimeout(2000);
    socket.open();
    // A frame claiming 2 MiB must be rejected before the body is allocated/read.
    const std::array<std::uint8_t, 4> header{0x00, 0x20, 0x00, 0x00};
    socket.write(header.data(), static_cast<std::uint32_t>(header.size()));
    std::array<std::uint8_t, 1> response{};
    bool closed = false;
    try {
        closed = socket.read(response.data(), 1) == 0;
    } catch (const apache::thrift::transport::TTransportException &error) {
        closed = error.getType() != apache::thrift::transport::TTransportException::TIMED_OUT;
    }
    CHECK(closed);
    qchat::Connection healthy("127.0.0.1", server.port());
    CHECK(healthy.post("A", "Still serving").id == 1);
}
} // namespace

int main() {
    return run_tests([] {
        listening_before_ready();
        exchange();
        parallel_clients();
        oversized_frame();
    });
}
