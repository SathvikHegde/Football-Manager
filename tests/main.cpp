#include "test_framework.hpp"
#include "fm/engine/version.hpp"

int main(int /*argc*/, char* /*argv*/[]) {
    std::cout << "FM Test Suite Harness - Engine: " << fm::engine_status() << "\n";
    return fm::test::TestRegistry::instance().run_all();
}

