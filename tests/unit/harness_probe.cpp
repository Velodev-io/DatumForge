#include "support/check.hpp"
int main(int argc, char **) {
    return run([&] { CHECK(argc == 1); });
}
