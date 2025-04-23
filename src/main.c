#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast_serializer.h"
#include "lexer.h"
#include "parser.h"

#define MAX_LINE_LENGTH 1024

// Converts a token type to its string representation.
const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_ASK:
            return "TOKEN_ASK";
        case TOKEN_TELL:
            return "TOKEN_TELL";
        case TOKEN_FIND:
            return "TOKEN_FIND";
        case TOKEN_SHOW:
            return "TOKEN_SHOW";
        case TOKEN_GET:
            return "TOKEN_GET";
        case TOKEN_FOR:
            return "TOKEN_FOR";
        case TOKEN_FROM:
            return "TOKEN_FROM";
        case TOKEN_TO:
            return "TOKEN_TO";
        case TOKEN_IF:
            return "TOKEN_IF";
        case TOKEN_WHEN:
            return "TOKEN_WHEN";
        case TOKEN_WHERE:
            return "TOKEN_WHERE";
        case TOKEN_THAT:
            return "TOKEN_THAT";
        case TOKEN_GROUP:
            return "TOKEN_GROUP";
        case TOKEN_SORT:
            return "TOKEN_SORT";
        case TOKEN_BY:
            return "TOKEN_BY";
        case TOKEN_LIMIT:
            return "TOKEN_LIMIT";
        case TOKEN_AND:
            return "TOKEN_AND";
        case TOKEN_OR:
            return "TOKEN_OR";
        case TOKEN_HAVING:
            return "TOKEN_HAVING";
        case TOKEN_ORDER:
            return "TOKEN_ORDER";
        case TOKEN_ADD:
            return "TOKEN_ADD";
        case TOKEN_REMOVE:
            return "TOKEN_REMOVE";
        case TOKEN_UPDATE:
            return "TOKEN_UPDATE";
        case TOKEN_CREATE:
            return "TOKEN_CREATE";
        case TOKEN_WITH:
            return "TOKEN_WITH";
        case TOKEN_AS:
            return "TOKEN_AS";
        case TOKEN_IN:
            return "TOKEN_IN";
        case TOKEN_NOT:
            return "TOKEN_NOT";
        case TOKEN_WHICH:
            return "TOKEN_WHICH";
        case TOKEN_LIKE:
            return "TOKEN_LIKE";
        case TOKEN_PLUS:
            return "TOKEN_PLUS";
        case TOKEN_MINUS:
            return "TOKEN_MINUS";
        case TOKEN_STAR:
            return "TOKEN_STAR";
        case TOKEN_SLASH:
            return "TOKEN_SLASH";
        case TOKEN_PERCENT:
            return "TOKEN_PERCENT";
        case TOKEN_IDENTIFIER:
            return "TOKEN_IDENTIFIER";
        case TOKEN_STRING:
            return "TOKEN_STRING";
        case TOKEN_INTEGER:
            return "TOKEN_INTEGER";
        case TOKEN_DECIMAL:
            return "TOKEN_DECIMAL";
        case TOKEN_EQUAL:
            return "TOKEN_EQUAL";
        case TOKEN_GT:
            return "TOKEN_GT";
        case TOKEN_LT:
            return "TOKEN_LT";
        case TOKEN_GTE:
            return "TOKEN_GTE";
        case TOKEN_LTE:
            return "TOKEN_LTE";
        case TOKEN_NEQ:
            return "TOKEN_NEQ";
        case TOKEN_COMMA:
            return "TOKEN_COMMA";
        case TOKEN_LPAREN:
            return "TOKEN_LPAREN";
        case TOKEN_RPAREN:
            return "TOKEN_RPAREN";
        case TOKEN_EOF:
            return "TOKEN_EOF";
        case TOKEN_ERROR:
            return "TOKEN_ERROR";
        default:
            return "UNKNOWN_TOKEN";
    }
}

// Returns a string describing the storage engine type.
const char* engine_type_to_string(uint8_t engine_type) {
    switch (engine_type) {
        case ENGINE_SQL:
            return "SQL";
        case ENGINE_NOSQL:
            return "NoSQL";
        case ENGINE_AUTO:
            return "Auto";
        default:
            return "Unknown";
    }
}

// Displays information about a serialized AST, including size, checksum, and metadata.
void display_serialized_info(SerializedAST* ast) {
    if (!ast) {
        printf("Serialization failed\n");
        return;
    }

    size_t      size;
    const void* data = ast_get_data(ast, &size);
    printf("Serialized Size: %zu bytes\n", size);

    // Check checksum validity
    bool valid = ast_verify_checksum(ast);
    printf("Checksum Valid: %s\n", valid ? "Yes" : "No");

    // Extract metadata for display
    ExecutionMetadata metadata;
    if (ast_extract_metadata(ast, &metadata)) {
        printf("Execution Metadata:\n");
        printf("  Engine: %s\n", engine_type_to_string(metadata.engine_type));
        printf("  Priority: %d\n", metadata.priority);
        printf("  Timeout: %d ms\n", metadata.timeout_ms);
        printf("  Estimated Rows: %d\n", metadata.estimated_rows);

        // Display hints
        printf("  Hints: ");
        if (metadata.hint_flags & HINT_READ_ONLY)
            printf("READ_ONLY ");
        if (metadata.hint_flags & HINT_PARALLEL_EXEC)
            printf("PARALLEL ");
        if (metadata.hint_flags & HINT_INDEX_SCAN)
            printf("INDEX_SCAN ");
        if (metadata.hint_flags & HINT_FULL_SCAN)
            printf("FULL_SCAN ");
        if (metadata.hint_flags & HINT_CACHE_RESULT)
            printf("CACHE ");
        if (metadata.hint_flags & HINT_PRIORITY_HIGH)
            printf("HIGH_PRIORITY ");
        if (metadata.hint_flags & HINT_PRIORITY_LOW)
            printf("LOW_PRIORITY ");
        printf("\n");

        // Display target index if any
        if (metadata.target_index) {
            printf("  Target Index: %s\n", metadata.target_index);
        }
    }

    // Display first bytes of the serialized data (hex dump)
    printf("Header Bytes: ");
    const unsigned char* bytes = (const unsigned char*)data;
    for (int i = 0; i < 16 && i < size; i++) {
        printf("%02X ", bytes[i]);
    }
    printf("\n");
}

// Prints help and usage information for the NSQL command-line tool.
void print_help() {
    printf("NSQL Query Language v0.1.0\n");
    printf("\nExample Queries:\n");
    printf("  ASK users FOR name, email WHEN age > 18\n");
    printf("  TELL users TO ADD \"new_user\" WITH name = \"John\", age = 30\n");
    printf("  TELL users TO UPDATE name = \"John\", status = \"active\" WHERE id = 123\n");
    printf("  TELL users TO REMOVE WHERE last_login < \"2023-01-01\"\n");
    printf("  TELL db TO CREATE name AS TEXT (REQUIRED), age AS INTEGER, email AS TEXT (UNIQUE)\n");
    printf("  FIND orders IN sales THAT total > 1000 ORDER BY date DESC\n");
    printf("  SHOW ME products FROM inventory WHERE category = \"electronics\" LIMIT 10\n");
    printf("  GET COUNT(id) FROM users GROUP BY country HAVING COUNT(id) > 100\n");
    printf("\n");
    printf("Special Commands:\n");
    printf("  --help       Show this help text\n");
    printf("  --tokens     Show lexical tokens\n");
    printf("  --ast        Show abstract syntax tree\n");
    printf("  --serialize  Show serialized AST info\n");
    printf("  exit         Exit the program\n");
}

// Starts a simple interactive Read-Eval-Print Loop (REPL) for NSQL queries.
void repl() {
    char line[MAX_LINE_LENGTH];

    printf("NSQL v0.1.0\n");
    printf("Type 'exit' to quit or '--help' for examples\n");
    printf("> ");

    for (;;) {
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        // Remove trailing newline character from the input.
        line[strcspn(line, "\n")] = '\0';

        // Exit the REPL if the user types 'exit' or 'quit'.
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }

        // Show help if requested.
        if (strcmp(line, "--help") == 0) {
            print_help();
            printf("> ");
            continue;
        }

        // Parse command-line flags from the input.
        bool show_tokens     = strstr(line, "--tokens") != NULL;
        bool show_ast        = strstr(line, "--ast") != NULL;
        bool show_serialized = strstr(line, "--serialize") != NULL;

        // Create a clean query without flags
        char query[MAX_LINE_LENGTH];
        strncpy(query, line, MAX_LINE_LENGTH);

        // Remove any flags from the query string.
        if (show_tokens || show_ast || show_serialized) {
            char* flags_pos = strstr(query, "--");
            if (flags_pos != NULL) {
                *flags_pos = '\0';
            }
        }

        // Trim whitespace from the end of the query.
        int len = strlen(query);
        while (len > 0 && isspace(query[len - 1])) {
            query[--len] = '\0';
        }

        // Skip processing if the query is empty after removing flags.
        if (strlen(query) == 0) {
            printf("> ");
            continue;
        }

        // Initialize the lexer with the user's query.
        Lexer lexer;
        lexer_init(&lexer, query);

        // Show lexical tokens if requested.
        if (show_tokens) {
            printf("Tokens:\n");
            Lexer token_lexer;
            lexer_init(&token_lexer, query);

            for (;;) {
                Token token = lexer_next_token(&token_lexer);
                printf("  %d (%s), '%.*s'\n", token.type, token_type_to_string(token.type),
                       token.length, token.start);

                if (token.type == TOKEN_EOF || token.type == TOKEN_ERROR)
                    break;
            }
            printf("\n");
        }

        // Parse the query into an AST.
        Parser parser;
        parser_init(&parser, &lexer);
        Node* ast = parse_query(&parser);

        // Display results based on parsing outcome and flags.
        if (parser.had_error) {
            printf("Syntax error\n");
        } else {
            printf("Query is valid\n");

            // Show the AST if requested.
            if (show_ast && ast != NULL) {
                printf("\nAbstract Syntax Tree:\n");
                print_ast(ast, 0);
                printf("\n");
            }

            // Show serialized AST information if requested.
            if (show_serialized && ast != NULL) {
                printf("\nSerialized AST:\n");

                // Create metadata with defaults optimized for this query
                ExecutionMetadata metadata = ast_create_metadata(ast);

                // Serialize the AST
                SerializedAST* serialized = ast_serialize(ast, &metadata);

                // Display serialization info
                display_serialized_info(serialized);

                // Free serialized data
                ast_free(serialized);

                printf("\n");
            }
        }

        // Free the AST after processing.
        if (ast != NULL) {
            free_node(ast);
        }

        // Prompt for the next input.
        printf("> ");
    }
}

// Processes an input file containing NSQL queries.
void process_file(const char* filename, bool show_tokens, bool show_ast, bool show_serialized) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", filename);
        exit(74);
    }

    // Read the entire file into a buffer.
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = malloc(file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", filename);
        fclose(file);
        exit(74);
    }

    size_t bytes_read  = fread(buffer, sizeof(char), file_size, file);
    buffer[bytes_read] = '\0';

    fclose(file);

    // Initialize the lexer with the file contents.
    Lexer lexer;
    lexer_init(&lexer, buffer);

    // Show lexical tokens if requested.
    if (show_tokens) {
        printf("Tokens in %s:\n", filename);
        Lexer token_lexer;
        lexer_init(&token_lexer, buffer);

        for (;;) {
            Token token = lexer_next_token(&token_lexer);
            printf("  %d (%s), '%.*s'\n", token.type, token_type_to_string(token.type),
                   token.length, token.start);

            if (token.type == TOKEN_EOF || token.type == TOKEN_ERROR)
                break;
        }
        printf("\n");
    }

    // Parse the query from the file.
    Parser parser;
    parser_init(&parser, &lexer);
    Node* ast = parse_query(&parser);

    // Display results based on parsing outcome and flags.
    if (parser.had_error) {
        printf("Syntax error in file\n");
        free(buffer);
        exit(65);
    } else {
        printf("File contains valid NSQL query\n");

        // Show the AST if requested or by default.
        if (show_ast || (!show_tokens && !show_serialized)) {
            printf("\nAbstract Syntax Tree:\n");
            print_ast(ast, 0);
            printf("\n");
        }

        // Show serialized AST information if requested.
        if (show_serialized && ast != NULL) {
            printf("\nSerialized AST:\n");

            // Create metadata with defaults optimized for this query
            ExecutionMetadata metadata = ast_create_metadata(ast);

            // Serialize the AST
            SerializedAST* serialized = ast_serialize(ast, &metadata);

            // Display serialization info
            display_serialized_info(serialized);

            // Free serialized data
            ast_free(serialized);

            printf("\n");
        }
    }

    // Free resources after processing.
    if (ast != NULL) {
        free_node(ast);
    }
    free(buffer);
}

// Executes a single NSQL command provided as a string.
void execute_command(const char* query, bool show_tokens, bool show_ast, bool show_serialized) {
    // Initialize lexer with the input
    Lexer lexer;
    lexer_init(&lexer, query);

    // Show lexical tokens if requested.
    if (show_tokens) {
        printf("Tokens:\n");
        Lexer token_lexer;
        lexer_init(&token_lexer, query);

        for (;;) {
            Token token = lexer_next_token(&token_lexer);
            printf("  %d (%s), '%.*s'\n", token.type, token_type_to_string(token.type),
                   token.length, token.start);

            if (token.type == TOKEN_EOF || token.type == TOKEN_ERROR)
                break;
        }
        printf("\n");
    }

    // Parse the query into an AST.
    Parser parser;
    parser_init(&parser, &lexer);
    Node* ast = parse_query(&parser);

    // Display results based on parsing outcome and flags.
    if (parser.had_error) {
        printf("Syntax error\n");
        exit(65);
    } else {
        printf("Query is valid\n");

        // Show the AST if requested or by default.
        if (show_ast || (!show_tokens && !show_serialized)) {
            printf("\nAbstract Syntax Tree:\n");
            print_ast(ast, 0);
            printf("\n");
        }

        // Show serialized AST information if requested.
        if (show_serialized && ast != NULL) {
            printf("\nSerialized AST:\n");

            // Create metadata with defaults optimized for this query
            ExecutionMetadata metadata = ast_create_metadata(ast);

            // Serialize the AST
            SerializedAST* serialized = ast_serialize(ast, &metadata);

            // Display serialization info
            display_serialized_info(serialized);

            // Get binary data
            size_t      size;
            const void* data = ast_get_data(serialized, &size);

            // Here is where the binary data would be passed to the database engine.
            // db_engine_execute(data, size);

            // Free serialized data
            ast_free(serialized);

            printf("\n");
        }
    }

    // Free resources after processing.
    if (ast != NULL) {
        free_node(ast);
    }
}

// Exports the AST for a query to a binary file.
void export_ast(const char* query, const char* output_file) {
    // Initialize lexer with the input
    Lexer lexer;
    lexer_init(&lexer, query);

    // Parse the query and check for errors.
    Parser parser;
    parser_init(&parser, &lexer);
    Node* ast = parse_query(&parser);

    if (parser.had_error || ast == NULL) {
        printf("Syntax error in query\n");
        exit(65);
    }

    // Create metadata with defaults optimized for this query
    ExecutionMetadata metadata = ast_create_metadata(ast);

    // Serialize the AST
    SerializedAST* serialized = ast_serialize(ast, &metadata);
    if (!serialized) {
        printf("Failed to serialize AST\n");
        free_node(ast);
        exit(70);
    }

    // Get binary data
    size_t      size;
    const void* data = ast_get_data(serialized, &size);

    // Write to file
    FILE* file = fopen(output_file, "wb");
    if (!file) {
        fprintf(stderr, "Could not open file \"%s\" for writing.\n", output_file);
        ast_free(serialized);
        free_node(ast);
        exit(74);
    }

    size_t bytes_written = fwrite(data, 1, size, file);
    if (bytes_written != size) {
        fprintf(stderr, "Failed to write data to file \"%s\".\n", output_file);
        fclose(file);
        ast_free(serialized);
        free_node(ast);
        exit(74);
    }

    fclose(file);
    printf("Serialized AST written to %s (%zu bytes)\n", output_file, size);

    // Free resources
    ast_free(serialized);
    free_node(ast);
}

// Imports a serialized AST from a binary file and displays its information.
void import_ast(const char* input_file) {
    // Read file
    FILE* file = fopen(input_file, "rb");
    if (!file) {
        fprintf(stderr, "Could not open file \"%s\" for reading.\n", input_file);
        exit(74);
    }

    // Get file size
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    // Allocate buffer
    void* buffer = malloc(file_size);
    if (!buffer) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", input_file);
        fclose(file);
        exit(74);
    }

    // Read file
    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        fprintf(stderr, "Failed to read data from file \"%s\".\n", input_file);
        fclose(file);
        free(buffer);
        exit(74);
    }

    fclose(file);

    // Deserialize AST
    SerializedAST* ast = ast_deserialize(buffer, file_size);
    if (!ast) {
        fprintf(stderr, "Failed to deserialize AST from file \"%s\".\n", input_file);
        free(buffer);
        exit(70);
    }

    // Display information
    printf("Imported serialized AST from %s\n", input_file);
    display_serialized_info(ast);

    // Free resources
    ast_free(ast);
    free(buffer);
}

int main(int argc, char* argv[]) {
    // Command-line flags and file arguments.
    bool        show_tokens     = false;
    bool        show_ast        = false;
    bool        show_serialized = false;
    const char* export_file     = NULL;
    const char* import_file     = NULL;

    // Parse command-line arguments.
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) {
            show_tokens = true;
        } else if (strcmp(argv[i], "--ast") == 0) {
            show_ast = true;
        } else if (strcmp(argv[i], "--serialize") == 0) {
            show_serialized = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "--export") == 0 && i + 1 < argc) {
            export_file = argv[++i];
        } else if (strcmp(argv[i], "--import") == 0 && i + 1 < argc) {
            import_file = argv[++i];
        }
    }

    if (import_file != NULL) {
        // Import mode: read and display a serialized AST from a file.
        import_ast(import_file);
        return 0;
    }

    if (argc == 1 || (argc <= 4 && (show_tokens || show_ast || show_serialized))) {
        // No arguments or only flags: start the interactive REPL.
        repl();
    } else if (export_file != NULL && argc >= 4) {
        // Export mode: write a serialized AST to a file.
        int last_arg = 1;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--export") == 0) {
                i++;  // Skip export filename
                continue;
            } else if (strcmp(argv[i], "--tokens") == 0 || strcmp(argv[i], "--ast") == 0 ||
                       strcmp(argv[i], "--serialize") == 0) {
                continue;
            }
            last_arg = i;
        }
        export_ast(argv[last_arg], export_file);
    } else if (argv[1][0] != '-') {
        // Read and process queries from a file.
        process_file(argv[1], show_tokens, show_ast, show_serialized);
    } else if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
        // Execute a single command provided on the command line.
        execute_command(argv[2], show_tokens, show_ast, show_serialized);
    } else {
        // Print usage information for invalid arguments.
        fprintf(stderr, "Usage: nsql [script] [--tokens] [--ast] [--serialize]\n");
        fprintf(stderr, "       nsql -c \"query\" [--tokens] [--ast] [--serialize]\n");
        fprintf(stderr, "       nsql --export output.nsql \"query\"\n");
        fprintf(stderr, "       nsql --import input.nsql\n");
        fprintf(stderr, "       nsql --help\n");
        exit(64);
    }

    return 0;
}