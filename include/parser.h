#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "lexer.h"

// Forward declarations
typedef struct Parser Parser;
typedef struct Node   Node;

// Parser error handling
typedef struct {
    const char* message;
    int         line;
    bool        panic_mode;
} ParseError;

// Parser state
struct Parser {
    Token      current;
    Token      previous;
    Lexer*     lexer;
    bool       had_error;
    bool       panic_mode;
    ParseError error;
};

// AST node types
typedef enum {
    // Query types
    NODE_ASK_QUERY,
    NODE_TELL_QUERY,
    NODE_FIND_QUERY,
    NODE_SHOW_QUERY,
    NODE_GET_QUERY,

    // Statement components
    NODE_FIELD_LIST,
    NODE_SOURCE,
    NODE_CONDITION,
    NODE_JOIN,
    NODE_GROUP_BY,
    NODE_ORDER_BY,
    NODE_LIMIT,

    // Actions for TELL queries
    NODE_ADD_ACTION,
    NODE_REMOVE_ACTION,
    NODE_UPDATE_ACTION,
    NODE_CREATE_ACTION,

    // Expressions
    NODE_BINARY_EXPR,
    NODE_UNARY_EXPR,
    NODE_IDENTIFIER,
    NODE_FUNCTION_CALL,
    NODE_LITERAL,

    // Data definition
    NODE_FIELD_DEF,
    NODE_CONSTRAINT,

    // Misc
    NODE_ERROR,
    NODE_PROGRAM
} NodeType;

// Constraint types
typedef enum { CONSTRAINT_REQUIRED, CONSTRAINT_UNIQUE, CONSTRAINT_DEFAULT } ConstraintType;

// Generic AST node
struct Node {
    NodeType type;
    int      line;

    // Union of possible node contents
    union {
        // ASK query
        struct {
            struct Node* source;
            struct Node* fields;
            struct Node* condition;
            struct Node* group_by;
            struct Node* order_by;
            struct Node* limit;
        } ask_query;

        // TELL query
        struct {
            struct Node* source;
            struct Node* action;
            struct Node* condition;
        } tell_query;

        // FIND query
        struct {
            struct Node* source;
            struct Node* condition;
            struct Node* group_by;
            struct Node* order_by;
            struct Node* limit;
        } find_query;

        // SHOW query
        struct {
            struct Node* source;
            struct Node* fields;
            struct Node* condition;
            struct Node* group_by;
            struct Node* order_by;
            struct Node* limit;
        } show_query;

        // GET query
        struct {
            struct Node* source;
            struct Node* fields;
            struct Node* condition;
            struct Node* group_by;
            struct Node* order_by;
            struct Node* limit;
        } get_query;

        // Field list
        struct {
            int           count;
            struct Node** fields;
        } field_list;

        // Source
        struct {
            struct Node* identifier;
            struct Node* join;
        } source;

        // Join
        struct {
            struct Node* source;
            struct Node* condition;
        } join;

        // Condition
        struct {
            struct Node* left;
            TokenType    op;
            struct Node* right;
        } condition;

        // Group by
        struct {
            struct Node* fields;
            struct Node* having;
        } group_by;

        // Order by
        struct {
            int           count;
            struct Node** fields;
            bool*         ascending;  // Array of booleans for ASC/DESC
        } order_by;

        // Limit
        struct {
            int limit;
            int offset;
        } limit;

        // Add action (for TELL)
        struct {
            struct Node* value;
            struct Node* record_spec;
        } add_action;

        // Remove action (for TELL)
        struct {
            struct Node* condition;
        } remove_action;

        // Update action (for TELL)
        struct {
            int           count;
            struct Node** fields;
            struct Node** values;
        } update_action;

        // Create action (for TELL)
        struct {
            int           count;
            struct Node** field_defs;
        } create_action;

        // Binary expression
        struct {
            struct Node* left;
            TokenType    op;
            struct Node* right;
        } binary_expr;

        // Unary expression
        struct {
            TokenType    op;
            struct Node* operand;
        } unary_expr;

        // Identifier
        struct {
            char* name;
            int   length;
        } identifier;

        // Function call
        struct {
            char*         name;
            int           arg_count;
            struct Node** args;
        } function_call;

        // Literal
        struct {
            TokenType literal_type;
            union {
                char*  string_value;
                double number_value;
                bool   boolean_value;
            } value;
        } literal;

        // Field definition
        struct {
            struct Node*  name;
            char*         type;
            int           constraint_count;
            struct Node** constraints;
        } field_def;

        // Constraint
        struct {
            ConstraintType type;
            struct Node*   default_value;
        } constraint;

        // Error
        struct {
            char* message;
        } error;

        // Program
        struct {
            Node** statements;
            int count;
        } program;
    } as;
};

// Initialize parser
void parser_init(Parser* parser, Lexer* lexer);

// Parse statements
Node* parse_program(Parser* parser);

// Parse NSQL query - returns AST root
Node* parse_query(Parser* parser);

// Free AST nodes
void free_node(Node* node);

// Print AST for debugging
void print_ast(Node* node, int indent);

#ifdef __cplusplus
}
#endif