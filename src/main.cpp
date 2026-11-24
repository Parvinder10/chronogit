#include "chronogit/cli/cli_app.hpp"

int main(int argc, char** argv) {
    chronogit::CliApp app;
    return app.run(argc, argv);
}
