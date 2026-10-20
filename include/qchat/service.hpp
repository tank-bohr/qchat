#pragma once

#include "Chat.h"
#include "qchat/store.hpp"

namespace qchat {
class Service final : public rpc::ChatIf {
  public:
    explicit Service(std::shared_ptr<Store> store);
    void post(rpc::Message &result, const std::string &display_name,
              const std::string &text) override;
    void poll(rpc::MessagePage &result, const std::string &server_id, std::int64_t after_id,
              std::int32_t limit) override;

  private:
    std::shared_ptr<Store> store_;
};
} // namespace qchat
