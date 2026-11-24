#include "test_framework.hpp"

int main() {
    return chronogit::test::TestRegistry::instance().run_all();
}
