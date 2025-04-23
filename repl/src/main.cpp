#include <iostream>
#include <string>

namespace repl {
    class Repl {
    public:
        void start() {
            std::string input;
            while (true) {
                std::cout << "> ";
                std::getline(std::cin, input);
                if (input == "exit") {
                    break;
                }
                std::cout << "You entered: " << input << std::endl;
            }
        }
    };
} // namespace repl

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::cerr << "Usage: " << argv[0] << " [options]" << std::endl;
        return 1;
    }

    // Initialize the REPL
    repl::Repl repl;
    repl.start();
    return 0;
}