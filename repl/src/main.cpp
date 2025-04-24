#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include "lexer.h"
#include "parser.h"
#include "ast_serializer.h"

int main() {
    std::cout << "NSQL REPL. Type 'exit' to quit.\n";
    
    std::string buffer;
    std::string line;
    bool continuing = false;
    
    while (true) {
        // Different prompts for initial and continuation lines
        if (continuing) {
            std::cout << "... > ";
        } else {
            std::cout << "nsql> ";
        }
        
        if (!std::getline(std::cin, line)) break;
        
        // Check for exit commands
        if (!continuing && (line == "exit" || line == "quit")) break;
        if (line.empty()) {
            if (continuing) {
                // Empty line during multiline input - treat as cancel
                continuing = false;
                buffer.clear();
                std::cout << "Query input canceled.\n";
            }
            continue;
        }
        
        // Add this line to our buffer
        if (!buffer.empty()) {
            buffer += " ";  // Space between lines
        }
        buffer += line;
        
        // Check if the statement is complete (ends with PLEASE)
        bool isComplete = false;
        if (line.find("PLEASE") != std::string::npos) {
            continuing = false;
            isComplete = true;
        } else {
            continuing = true;
        }
        
        // Process complete statements
        if (isComplete) {
            // Tokenize
            Lexer lexer;
            lexer_init(&lexer, buffer.c_str());

            // Parse
            Parser parser;
            parser_init(&parser, &lexer);
            
            // Parse the whole program (multiple statements)
            Node* program = parse_program(&parser);

            if (program) {
                // Print AST for successful parts (even if there were some errors)
                if (program->as.program.count > 0) {
                    print_ast(program, 0);
                } else if (parser.had_error) {
                    std::cerr << "No valid statements found due to parsing errors.\n";
                }
                
                free_node(program);
            } else {
                std::cerr << "Fatal parsing error - could not recover.\n";
            }
            
            // Clear buffer for next query
            buffer.clear();
        }
    }
    
    return 0;
}