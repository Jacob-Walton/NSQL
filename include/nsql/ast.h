/**
 * @file ast.h
 * @brief Abstract Syntax Tree definitions for NSQL
 */

#ifndef NSQL_AST_H
#define NSQL_AST_H

#include <nsql/lexer.h>  // Include lexer.h for NsqlTokenType definition
#include <stdbool.h>
#include <stdint.h>

/**
 * Node type enumeration
 */
typedef enum {
    // Queries
    NODE_ASK_QUERY,
    NODE_TELL_QUERY,
    NODE_FIND_QUERY,
    NODE_SHOW_QUERY,
    NODE_GET_QUERY,

    // Components
    NODE_FIELD_LIST,
    NODE_SOURCE,
    NODE_JOIN,
    NODE_GROUP_BY,
    NODE_ORDER_BY,
    NODE_LIMIT,

    // Actions
    NODE_ADD_ACTION,
    NODE_REMOVE_ACTION,
    NODE_UPDATE_ACTION,
    NODE_CREATE_ACTION,

    // Expressions
    NODE_BINARY_EXPR,
    NODE_UNARY_EXPR,
    NODE_IDENTIFIER,
    NODE_LITERAL,
    NODE_FIELD_DEF,
    NODE_CONSTRAINT,
    NODE_FUNCTION_CALL,

    // Error
    NODE_ERROR,

    // Program
    NODE_PROGRAM
} NodeType;

/**
 * Constraint type enumeration
 */
typedef enum { CONSTRAINT_REQUIRED, CONSTRAINT_UNIQUE, CONSTRAINT_DEFAULT } ConstraintType;

// Forward declare node structure
typedef struct Node Node;

/**
 * Union of different node data structures
 */
typedef union {
    struct {
        Node* source;
        Node* fields;
        Node* condition;
        Node* group_by;
        Node* order_by;
        Node* limit;
    } ask_query;

    struct {
        Node* source;
        Node* action;
        Node* condition;
    } tell_query;

    struct {
        Node* source;
        Node* condition;
        Node* group_by;
        Node* order_by;
        Node* limit;
    } find_query;

    struct {
        Node* source;
        Node* fields;
        Node* condition;
        Node* group_by;
        Node* order_by;
        Node* limit;
    } show_query;

    struct {
        Node* source;
        Node* fields;
        Node* condition;
        Node* group_by;
        Node* order_by;
        Node* limit;
    } get_query;

    struct {
        Node** fields;
        int    count;
    } field_list;

    struct {
        Node* identifier;
        Node* join;
    } source;

    struct {
        Node* source;
        Node* condition;
    } join;

    struct {
        Node* fields;
        Node* having;
    } group_by;

    struct {
        Node** fields;
        bool*  ascending;
        int    count;
    } order_by;

    struct {
        int limit;
        int offset;
    } limit;

    struct {
        Node* value;
        Node* record_spec;
    } add_action;

    struct {
        Node* condition;
    } remove_action;

    struct {
        Node** fields;
        Node** values;
        int    count;
    } update_action;

    struct {
        Node** field_defs;
        int    count;
    } create_action;

    struct {
        Node*         left;
        Node*         right;
        NsqlTokenType op;
    } binary_expr;

    struct {
        Node*         operand;
        NsqlTokenType op;
    } unary_expr;

    struct {
        char* name;
        int   length;
    } identifier;

    struct {
        union {
            char*  string_value;
            double number_value;
        } value;
        NsqlTokenType literal_type;
    } literal;

    struct {
        Node*  name;
        char*  type;
        Node** constraints;
        int    constraint_count;
    } field_def;

    struct {
        ConstraintType type;
        Node*          default_value;
    } constraint;

    struct {
        char*  name;
        Node** args;
        int    arg_count;
    } function_call;

    struct {
        char* message;
    } error;

    struct {
        Node** statements;
        int    count;
    } program;
} NodeData;

/**
 * AST node structure
 */
struct Node {
    NodeType type;
    int      line;
    union {
        NodeData as;
    };
};

/**
 * Free a node and all its children
 *
 * @param node The node to free
 */
void free_node(Node* node);

#endif /* NSQL_AST_H */
