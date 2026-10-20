#pragma once

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     ": " #expression);                                            \
        }                                                                                          \
    } while (false)

template <typename Exception, typename Function> void expect_throw(Function function) {
    bool caught = false;
    try {
        function();
    } catch (const Exception &) {
        caught = true;
    }
    CHECK(caught);
}

template <typename Function> int run_tests(Function function) {
    try {
        function();
        std::cout << "All checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
