#include "qchat/store.hpp"
#include "test_support.hpp"

#include <future>

namespace {
void validation() {
    expect_throw<std::invalid_argument>([] { const qchat::Store store(""); });
    expect_throw<std::invalid_argument>([] { const qchat::Store store("id", 0); });
    qchat::Store store("id");
    for (const auto &name : {"", " ", "\t", "a\nb", "\x7f", "\xc0\xaf", "\xed\xa0\x80"}) {
        expect_throw<qchat::InvalidRequest>([&] { (void)store.post(name, "hello"); });
    }
    for (const auto &text :
         {"", " \n\t", "\r", "\xf4\x90\x80\x80", "\xe2\x82", "\x80", "\xc2\x80"}) {
        expect_throw<qchat::InvalidRequest>([&] { (void)store.post("A", text); });
    }
    expect_throw<qchat::InvalidRequest>([&] { (void)store.post(std::string(65, 'n'), "hello"); });
    expect_throw<qchat::InvalidRequest>([&] { (void)store.post("A", std::string(4097, 'm')); });
    expect_throw<qchat::InvalidRequest>([&] { (void)store.post("A", std::string("a\0b", 3)); });
    CHECK(store.post(std::string(64, 'n'), std::string(4096, 'm')).id == 1);
    CHECK(store.post("世界", "Hello 👋\nمرحبا\t!").id == 2);
    CHECK(store.post("Same name", "<script>literal text</script>").id == 3);
    CHECK(store.post("Same name", "No identity claim").id == 4);
    for (const auto &bytes :
         {"\xe0\x80\x80", "\xf0\x80\x80\x80", "\xe2x\xac", "\xf5\x80\x80\x80"}) {
        CHECK(!qchat::valid_text(bytes, true));
    }
    CHECK(qchat::valid_text("\xc2\xa0\xe0\xa0\x80\xf4\x8f\xbf\xbf", true));
}

void pagination() {
    qchat::Store store("epoch", 3);
    auto page = store.poll("", 0, 100);
    CHECK(page.reset && page.messages.empty() && page.next_cursor == 0);
    CHECK(!store.poll("epoch", 0, 1).reset);
    for (int i = 1; i <= 5; ++i) {
        const auto message = store.post("A", std::to_string(i));
        CHECK(message.id == i && message.timestamp_ms > 0);
    }
    page = store.poll("epoch", 0, 2);
    CHECK(page.reset && page.messages.size() == 2 && page.messages.front().id == 3);
    CHECK(page.next_cursor == 4);
    page = store.poll("epoch", 4, 2);
    CHECK(!page.reset && page.messages.size() == 1 && page.next_cursor == 5);
    CHECK(store.poll("epoch", 5, 100).messages.empty());
    CHECK(store.poll("epoch", 5, 100).next_cursor == 5);
    CHECK(!store.poll("epoch", 2, 1).reset);
    CHECK(store.poll("old-epoch", 5, 1).reset);
    CHECK(store.poll("epoch", 100, 1).reset);
    expect_throw<qchat::InvalidRequest>([&] { (void)store.poll("epoch", -1, 1); });
    expect_throw<qchat::InvalidRequest>([&] { (void)store.poll("epoch", 0, 0); });
    expect_throw<qchat::InvalidRequest>([&] { (void)store.poll("epoch", 0, 101); });
    expect_throw<qchat::InvalidRequest>([&] { (void)store.poll(std::string(129, 'x'), 0, 1); });
}

void parallel_posts() {
    qchat::Store store("epoch", 2000);
    std::vector<std::future<void>> writers;
    writers.reserve(8);
    for (int thread = 0; thread < 8; ++thread) {
        writers.push_back(std::async(std::launch::async, [&, thread] {
            for (int i = 0; i < 200; ++i) {
                (void)store.post(std::to_string(thread), std::to_string(i));
                const auto page = store.poll("epoch", 0, 100);
                CHECK(!page.messages.empty());
            }
        }));
    }
    for (auto &writer : writers) {
        writer.get();
    }
    std::int64_t cursor = 0;
    for (;;) {
        const auto page = store.poll("epoch", cursor, 100);
        if (page.messages.empty()) {
            break;
        }
        for (const auto &message : page.messages) {
            CHECK(message.id == ++cursor);
        }
    }
    CHECK(cursor == 1600);
}
} // namespace

int main() {
    return run_tests([] {
        validation();
        pagination();
        parallel_posts();
    });
}
