#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace qchat {

inline constexpr std::size_t max_name_bytes = 64;
inline constexpr std::size_t max_text_bytes = 4096;
inline constexpr std::size_t default_history_size = 1000;
inline constexpr std::int32_t max_page_size = 100;

struct Message {
    std::int64_t id{};
    std::int64_t timestamp_ms{};
    std::string display_name;
    std::string text;
    bool operator==(const Message &) const = default;
};

struct Page {
    std::string server_id;
    std::vector<Message> messages;
    std::int64_t next_cursor{};
    bool reset{};
};

class InvalidRequest : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Accepts well-formed UTF-8; rejects NUL and controls (except LF/TAB in message bodies).
[[nodiscard]] bool valid_text(std::string_view text, bool multiline);

class Store {
  public:
    explicit Store(std::string server_id, std::size_t capacity = default_history_size);
    [[nodiscard]] Message post(std::string_view display_name, std::string_view text);
    [[nodiscard]] Page poll(std::string_view server_id, std::int64_t after_id,
                            std::int32_t limit) const;

  private:
    const std::string server_id_;
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<Message> messages_;
    std::int64_t next_id_{1};
};
} // namespace qchat
