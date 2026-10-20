#include "qchat/store.hpp"

#include <algorithm>
#include <chrono>
#include <limits>

namespace qchat {
namespace {
bool blank(std::string_view value) {
    return std::ranges::all_of(value, [](char c) { return c == ' ' || c == '\n' || c == '\t'; });
}
} // namespace

bool valid_text(std::string_view text, bool multiline) {
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i++]);
        std::uint32_t point = first;
        unsigned continuation = 0;
        std::uint32_t minimum = 0;
        if (first >= 0xC2 && first <= 0xDF) {
            point = first & 0x1FU;
            continuation = 1;
            minimum = 0x80;
        } else if (first >= 0xE0 && first <= 0xEF) {
            point = first & 0x0FU;
            continuation = 2;
            minimum = 0x800;
        } else if (first >= 0xF0 && first <= 0xF4) {
            point = first & 0x07U;
            continuation = 3;
            minimum = 0x10000;
        } else if (first >= 0x80) {
            return false;
        }
        if (text.size() - i < continuation) {
            return false;
        }
        for (unsigned j = 0; j < continuation; ++j) {
            const auto byte = static_cast<unsigned char>(text[i++]);
            if ((byte & 0xC0U) != 0x80U) {
                return false;
            }
            point = (point << 6U) | (byte & 0x3FU);
        }
        if (point < minimum || point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF)) {
            return false;
        }
        if ((point < 0x20 && !(multiline && (point == '\n' || point == '\t'))) ||
            (point >= 0x7F && point <= 0x9F)) {
            return false;
        }
    }
    return true;
}

Store::Store(std::string server_id, std::size_t capacity)
    : server_id_(std::move(server_id)), capacity_(capacity) {
    if (server_id_.empty() || capacity_ == 0) {
        throw std::invalid_argument("Server ID and history capacity must be nonempty");
    }
}

Message Store::post(std::string_view display_name, std::string_view text) {
    if (display_name.empty() || display_name.size() > max_name_bytes || blank(display_name) ||
        !valid_text(display_name, false)) {
        throw InvalidRequest(
            "Display name must contain 1–64 UTF-8 bytes without control characters");
    }
    if (text.empty() || text.size() > max_text_bytes || blank(text) || !valid_text(text, true)) {
        throw InvalidRequest(
            "Message must contain 1–4096 UTF-8 bytes; only tab/newline controls allowed");
    }
    const std::scoped_lock lock(mutex_);
    if (next_id_ == std::numeric_limits<std::int64_t>::max()) {
        throw InvalidRequest("Message sequence exhausted; restart the server");
    }
    Message message{next_id_,
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count(),
                    std::string(display_name), std::string(text)};
    messages_.push_back(message);
    ++next_id_;
    if (messages_.size() > capacity_) {
        messages_.pop_front();
    }
    return message;
}

Page Store::poll(std::string_view server_id, std::int64_t after_id, std::int32_t limit) const {
    if (after_id < 0 || limit < 1 || limit > max_page_size || server_id.size() > 128) {
        throw InvalidRequest("Invalid cursor, server ID, or page size (1–100)");
    }
    const std::scoped_lock lock(mutex_);
    const auto latest = next_id_ - 1;
    const bool expired = !messages_.empty() && after_id < messages_.front().id - 1;
    const bool reset = server_id != server_id_ || after_id > latest || expired;
    const auto cursor = reset ? 0 : after_id;
    Page page{server_id_, {}, cursor, reset};
    page.messages.reserve(static_cast<std::size_t>(limit));
    for (const auto &message : messages_) {
        if (message.id > cursor) {
            page.messages.push_back(message);
            page.next_cursor = message.id;
            if (page.messages.size() == static_cast<std::size_t>(limit)) {
                break;
            }
        }
    }
    return page;
}
} // namespace qchat
