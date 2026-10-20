#include "qchat/service.hpp"

namespace qchat {
namespace {
rpc::Message to_rpc(const Message &message) {
    rpc::Message result;
    result.__set_id(message.id);
    result.__set_timestampMs(message.timestamp_ms);
    result.__set_displayName(message.display_name);
    result.__set_text(message.text);
    return result;
}

[[noreturn]] void invalid(const InvalidRequest &error) {
    rpc::InvalidRequest result;
    result.__set_reason(error.what());
    // Thrift requires this generated wire exception; its string copy may allocate.
    throw result; // NOLINT(bugprone-exception-copy-constructor-throws)
}
} // namespace

Service::Service(std::shared_ptr<Store> store) : store_(std::move(store)) {
    if (!store_) {
        throw std::invalid_argument("Store is required");
    }
}

void Service::post(rpc::Message &result, const std::string &display_name, const std::string &text) {
    try {
        result = to_rpc(store_->post(display_name, text));
    } catch (const InvalidRequest &error) {
        invalid(error);
    }
}

void Service::poll(rpc::MessagePage &result, const std::string &server_id, std::int64_t after_id,
                   std::int32_t limit) {
    try {
        const auto page = store_->poll(server_id, after_id, limit);
        result = rpc::MessagePage{};
        result.__set_serverId(page.server_id);
        result.__set_nextCursor(page.next_cursor);
        result.__set_reset(page.reset);
        for (const auto &message : page.messages) {
            result.messages.push_back(to_rpc(message));
        }
    } catch (const InvalidRequest &error) {
        invalid(error);
    }
}
} // namespace qchat
