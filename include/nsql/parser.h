#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <nsql/lexer.h>
#include <nsql/ast.h>
#include <nsql/error_reporter.h>
#include <stdbool.h>

// Parser state
typedef struct {
    Lexer* lexer;         // Lexer to get tokens from
    Token current;        // Current token
    Token previous;       // Previous token
    bool had_error;       // Did we encounter an error during compilation?
    bool panic_mode;      // Are we in panic mode?
    ErrorContext errors;  // Error context for reporting
} Parser;

// Initialize the parser with a lexer
void parser_init(Parser* parser, Lexer* lexer);

// Free the parser resources
void parser_free(Parser* parser);

// Parse a query
Node* parse_query(Parser* parser);

// Parse a complete program (multiple queries)
Node* parse_program(Parser* parser);

// Print AST for debugging
void print_ast(Node* node, int indent);

// Format parser errors into a string
size_t parser_format_errors(Parser* parser, char* buffer, size_t size);

// Format parser errors as JSON
size_t parser_format_errors_json(Parser* parser, char* buffer, size_t size);

#ifdef __cplusplus
}
#endif