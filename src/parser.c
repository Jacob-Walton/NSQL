#include "parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward parsing function declarations
static Node* parse_ask_query(Parser* parser);
static Node* parse_tell_query(Parser* parser);
static Node* parse_find_query(Parser* parser);
static Node* parse_show_query(Parser* parser);

static Node* parse_field_list(Parser* parser);
static Node* parse_source(Parser* parser);
static Node* parse_join(Parser* parser);
static Node* parse_condition(Parser* parser);
static Node* parse_group_by(Parser* parser);
static Node* parse_order_by(Parser* parser);
static Node* parse_limit(Parser* parser);

static Node* parse_add_action(Parser* parser);
static Node* parse_remove_action(Parser* parser);
static Node* parse_update_action(Parser* parser);
static Node* parse_create_action(Parser* parser);
static Node* parse_record_spec(Parser* parser);
static Node* parse_field_def(Parser* parser);
static Node* parse_constraint(Parser* parser);

static Node* parse_expression(Parser* parser);
static Node* parse_logic_or(Parser* parser);
static Node* parse_logic_and(Parser* parser);
static Node* parse_equality(Parser* parser);
static Node* parse_comparison(Parser* parser);
static Node* parse_term(Parser* parser);
static Node* parse_factor(Parser* parser);
static Node* parse_unary(Parser* parser);
static Node* parse_primary(Parser* parser);
static Node* parse_function_call(Parser* parser);

// Util functions
static void        advance(Parser* parser);
static bool        check(Parser* parser, TokenType type);
static bool        match(Parser* parser, TokenType type);
static void        consume(Parser* parser, TokenType type, const char* message);
static void        error_at_current(Parser* parser, const char* message);
static void        error_at(Parser* parser, Token* token, const char* message);
static void        synchronize(Parser* parser);
static Node*       create_node(NodeType type);
static char*       copy_token_string(Token* token);
static const char* token_type_to_op_string(TokenType type);

/**
 * Initialize the parser with a lexer.
 *
 * @param parser The parser instance.
 * @param lexer The lexer instance.
 */
void parser_init(Parser* parser, Lexer* lexer) {
    parser->lexer            = lexer;
    parser->had_error        = false;
    parser->panic_mode       = false;
    parser->error.message    = NULL;
    parser->error.line       = 0;
    parser->error.panic_mode = false;

    // Prime the parser with the first token
    advance(parser);
}

/**
 * Advance the parser to the next token.
 *
 * @param parser The parser instance.
 */
static void advance(Parser* parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = lexer_next_token(parser->lexer);
        if (parser->current.type != TOKEN_ERROR)
            break;

        error_at_current(parser, parser->current.start);
    }
}

/**
 * Check if the current token is of the expected type.
 *
 * @param parser The parser instance.
 * @param type The expected token type.
 *
 * @return true if the token matches the expected type, false otherwise.
 */
static bool check(Parser* parser, TokenType type) {
    return parser->current.type == type;
}

/**
 * Match and consume the current token if it matches the expected type.
 *
 * @param parser The parser instance.
 * @param type The expected token type.
 * @return true if the token was consumed, false otherwise.
 */
static bool match(Parser* parser, TokenType type) {
    if (!check(parser, type))
        return false;
    advance(parser);
    return true;
}

/**
 * Consume the current token if it matches the expected type.
 * If it doesn't match, report an error with the provided message.
 *
 * @param parser The parser instance.
 * @param type The expected token type.
 * @param message The error message to report if the token doesn't match.
 */
static void consume(Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }

    error_at_current(parser, message);
}

/**
 * Report error at current token.
 *
 * @param parser The parser instance.
 * @param message The error message.
 */
static void error_at_current(Parser* parser, const char* message) {
    error_at(parser, &parser->current, message);
}

/**
 * Report error at given token.
 *
 * @param parser The parser instance.
 * @param token The token where the error occurred.
 * @param message The error message.
 */
static void error_at(Parser* parser, Token* token, const char* message) {
    if (parser->panic_mode)
        return;
    parser->panic_mode = true;

    parser->error.message    = message;
    parser->error.line       = token->line;
    parser->error.panic_mode = true;
    parser->had_error        = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);

    // Sync after error
    synchronize(parser);
}

/**
 * Synchronize the parser after an error.
 *
 * Compile with -DENABLE_SYNC_RECOVERY=OFF to disable this feature.
 *
 * @param parser The parser instance.
 */
static void synchronize(Parser* parser) {
#ifdef ENABLE_SYNC_RECOVERY
    parser->panic_mode = false;

    while (parser->current.type != TOKEN_EOF) {
        // Skip until we find a query start keyword
        switch (parser->current.type) {
            case TOKEN_ASK:
            case TOKEN_TELL:
            case TOKEN_FIND:
            case TOKEN_SHOW:
            case TOKEN_GET:
                return;
            default:;  // Do nothing
        }

        advance(parser);
    }
#else
    // Reset the parser, end statement processing
    parser->panic_mode       = false;
    parser->had_error        = false;
    parser->error.message    = NULL;
    parser->error.line       = 0;
    parser->error.panic_mode = false;
#endif
}

/**
 * Create a new AST node of the specified type.
 *
 * @param type The type of the node to create.
 * @return A pointer to the newly created node.
 */
static Node* create_node(NodeType type) {
    Node* node = (Node*)malloc(sizeof(Node));
    if (node == NULL) {
        fprintf(stderr, "Error: Out of memory\n");
        exit(EXIT_FAILURE);
    }
    memset(node, 0, sizeof(Node));
    node->type = type;
    return node;
}

/**
 * Copy token string to a new memory location.
 *
 * @param token The token to copy.
 */
static char* copy_token_string(Token* token) {
    char* str = (char*)malloc(token->length + 1);
    if (str == NULL) {
        fprintf(stderr, "Error: Out of memory\n");
        exit(EXIT_FAILURE);
    }

    memcpy(str, token->start, token->length);
    str[token->length] = '\0';
    return str;
}

/**
 * Convert token type to operator string representation.
 *
 * @param type The token type.
 * @return The operator string representation.
 */
static const char* token_type_to_op_string(TokenType type) {
    switch (type) {
        case TOKEN_PLUS:
            return "+";
        case TOKEN_MINUS:
            return "-";
        case TOKEN_STAR:
            return "*";
        case TOKEN_SLASH:
            return "/";
        case TOKEN_PERCENT:
            return "%";
        case TOKEN_EQUAL:
            return "=";
        case TOKEN_GT:
            return ">";
        case TOKEN_LT:
            return "<";
        case TOKEN_GTE:
            return ">=";
        case TOKEN_LTE:
            return "<=";
        case TOKEN_NEQ:
            return "!=";
        case TOKEN_LIKE:
            return "LIKE";
        case TOKEN_AND:
            return "AND";
        case TOKEN_OR:
            return "OR";
        case TOKEN_NOT:
            return "NOT";
        default:
            return "UNKNOWN";
    }
}

/**
 * Parse NSQL query and return the AST root.
 *
 * @param parser The parser instance.
 */
Node* parse_query(Parser* parser) {
    if (match(parser, TOKEN_ASK)) {
        return parse_ask_query(parser);
    } else if (match(parser, TOKEN_TELL)) {
        return parse_tell_query(parser);
    } else if (match(parser, TOKEN_FIND)) {
        return parse_find_query(parser);
    } else if (match(parser, TOKEN_SHOW)) {
        return parse_show_query(parser);
    } else if (match(parser, TOKEN_GET)) {
        return parse_show_query(parser);
    }

    error_at_current(parser, "Expected a query type (ASK, TELL, FIND, SHOW, GET)");
    return NULL;
}

// TODO: Shove all of the conditionals into one function, so as to not repeat code

/**
 * Parse ASK query.
 *
 * @param parser The parser instance.
 * @return The AST node representing the ASK query.
 */
static Node* parse_ask_query(Parser* parser) {
    Node* node = create_node(NODE_ASK_QUERY);
    node->line = parser->previous.line;

    // Parse source
    node->as.ask_query.source = parse_source(parser);

    // Parse FOR clause
    consume(parser, TOKEN_FOR, "Expected 'FOR' after source in ASK query");

    // Parse field list
    node->as.ask_query.fields = parse_field_list(parser);

    // Parse optional condition
    if (match(parser, TOKEN_WHEN) || match(parser, TOKEN_IF) || match(parser, TOKEN_WHERE)) {
        node->as.ask_query.condition = parse_condition(parser);
    } else {
        node->as.ask_query.condition = NULL;
    }

    // Parse optional GROUP BY
    if (match(parser, TOKEN_GROUP)) {
        consume(parser, TOKEN_BY, "Expected 'BY' after 'GROUP'");
        node->as.ask_query.group_by = parse_group_by(parser);
    } else {
        node->as.ask_query.group_by = NULL;
    }

    // Parse optional ORDER BY / SORT BY
    if ((match(parser, TOKEN_ORDER) && match(parser, TOKEN_BY)) ||
        (match(parser, TOKEN_SORT) && match(parser, TOKEN_BY))) {
        node->as.ask_query.order_by = parse_order_by(parser);
    } else {
        node->as.ask_query.order_by = NULL;
    }

    // Parse optional LIMIT
    if (match(parser, TOKEN_LIMIT)) {
        node->as.ask_query.limit = parse_limit(parser);
    } else {
        node->as.ask_query.limit = NULL;
    }

    return node;
}

/**
 * Parse TELL query.
 *
 * @param parser The parser instance.
 * @return The AST node representing the TELL query.
 */
static Node* parse_tell_query(Parser* parser) {
    Node* node = create_node(NODE_TELL_QUERY);
    node->line = parser->previous.line;

    // Parse source
    node->as.tell_query.source = parse_source(parser);

    // Parse TO
    consume(parser, TOKEN_TO, "Expected 'TO' after source in TELL query");

    // Parse action
    if (match(parser, TOKEN_ADD)) {
        node->as.tell_query.action = parse_add_action(parser);
    } else if (match(parser, TOKEN_REMOVE)) {
        node->as.tell_query.action = parse_remove_action(parser);
    } else if (match(parser, TOKEN_UPDATE)) {
        node->as.tell_query.action = parse_update_action(parser);
    } else if (match(parser, TOKEN_CREATE)) {
        node->as.tell_query.action = parse_create_action(parser);
    } else {
        error_at_current(parser, "Expected action (ADD, REMOVE, UPDATE, CREATE)");
        return NULL;
    }

    // Parse optional condition
    if (match(parser, TOKEN_WHEN) || match(parser, TOKEN_IF) || match(parser, TOKEN_WHERE)) {
        node->as.tell_query.condition = parse_condition(parser);
    } else {
        node->as.tell_query.condition = NULL;
    }

    return node;
}

/**
 * Parse FIND query.
 *
 * @param parser The parser instance.
 * @return The AST node representing the FIND query.
 */
static Node* parse_find_query(Parser* parser) {
    Node* node = create_node(NODE_FIND_QUERY);
    node->line = parser->previous.line;

    // Parse fields
    if (check(parser, TOKEN_IDENTIFIER)) {
        // Fields explicitly specified
        // In FIND queries, the fields are implicit in the source
        node->as.find_query.source = parse_source(parser);
    } else {
        // No fields specified, use implicit source
        Node* source_node = create_node(NODE_SOURCE);
        source_node->line = parser->previous.line;

        // Create an implicit "*" identifier
        Node* id_node                 = create_node(NODE_IDENTIFIER);
        id_node->line                 = parser->previous.line;
        id_node->as.identifier.name   = strdup("*");
        id_node->as.identifier.length = 1;

        source_node->as.source.identifier = id_node;
        source_node->as.source.join       = NULL;

        node->as.find_query.source = source_node;
    }

    // Parse IN if present
    if (match(parser, TOKEN_IN)) {
        // Replace the source
        Node* old_source           = node->as.find_query.source;
        node->as.find_query.source = parse_source(parser);
        free_node(old_source);
    }

    // Parse condition
    if (match(parser, TOKEN_THAT) || match(parser, TOKEN_WHEN) || match(parser, TOKEN_WHERE) ||
        match(parser, TOKEN_WHICH)) {
        node->as.find_query.condition = parse_condition(parser);
    } else {
        node->as.find_query.condition = NULL;
    }

    // Parse optional GROUP BY
    if (match(parser, TOKEN_GROUP)) {
        consume(parser, TOKEN_BY, "Expected 'BY' after 'GROUP'");
        node->as.find_query.group_by = parse_group_by(parser);
    } else {
        node->as.find_query.group_by = NULL;
    }

    // Parse optional ORDER BY / SORT BY
    if ((match(parser, TOKEN_ORDER) && match(parser, TOKEN_BY)) ||
        (match(parser, TOKEN_SORT) && match(parser, TOKEN_BY))) {
        node->as.find_query.order_by = parse_order_by(parser);
    } else {
        node->as.find_query.order_by = NULL;
    }

    // Parse optional LIMIT
    if (match(parser, TOKEN_LIMIT)) {
        node->as.find_query.limit = parse_limit(parser);
    } else {
        node->as.find_query.limit = NULL;
    }

    return node;
}

/**
 * Parse SHOW query.
 *
 * @param parser The parser instance.
 * @return The AST node representing the SHOW query.
 */
static Node* parse_show_query(Parser* parser) {
    Node* node = create_node(NODE_SHOW_QUERY);
    node->line = parser->previous.line;

    // Skip optional ME
    match(parser, TOKEN_IDENTIFIER);  // Just consume "ME" if present

    // Parse field list
    node->as.show_query.fields = parse_field_list(parser);

    // Parse FROM
    consume(parser, TOKEN_FROM, "Expected 'FROM' after fields in SHOW query");

    // Parse source
    node->as.show_query.source = parse_source(parser);

    // Parse optional condition
    if (match(parser, TOKEN_WHEN) || match(parser, TOKEN_IF) || match(parser, TOKEN_WHERE)) {
        node->as.show_query.condition = parse_condition(parser);
    } else {
        node->as.show_query.condition = NULL;
    }

    // Parse optional GROUP BY
    if (match(parser, TOKEN_GROUP)) {
        consume(parser, TOKEN_BY, "Expected 'BY' after 'GROUP'");
        node->as.show_query.group_by = parse_group_by(parser);
    } else {
        node->as.show_query.group_by = NULL;
    }

    // Parse optional ORDER BY / SORT BY
    if ((match(parser, TOKEN_ORDER) && match(parser, TOKEN_BY)) ||
        (match(parser, TOKEN_SORT) && match(parser, TOKEN_BY))) {
        node->as.show_query.order_by = parse_order_by(parser);
    } else {
        node->as.show_query.order_by = NULL;
    }

    // Parse optional LIMIT
    if (match(parser, TOKEN_LIMIT)) {
        node->as.show_query.limit = parse_limit(parser);
    } else {
        node->as.show_query.limit = NULL;
    }

    return node;
}

#if 0
/**
 * Parse GET query
 *
 * @param parser The parser instance.
 * @return The AST node representing the GET query.
 */
static Node* parse_get_query(Parser* parser) {
    Node* node = create_node(NODE_GET_QUERY);
    node->line = parser->previous.line;

    // Parse field list
    node->as.get_query.fields = parse_field_list(parser);

    // Parse FROM
    consume(parser, TOKEN_FROM, "Expected 'FROM' after fields in GET query");

    // Parse source
    node->as.get_query.source = parse_source(parser);

    // Parse optional condition
    if (match(parser, TOKEN_WHEN) || match(parser, TOKEN_IF) || match(parser, TOKEN_WHERE)) {
        node->as.get_query.condition = parse_condition(parser);
    } else {
        node->as.get_query.condition = NULL;
    }

    // Parse optional GROUP BY
    if (match(parser, TOKEN_GROUP)) {
        consume(parser, TOKEN_BY, "Expected 'BY' after 'GROUP'");
        node->as.get_query.group_by = parse_group_by(parser);
    } else {
        node->as.get_query.group_by = NULL;
    }

    // Parse optional ORDER BY / SORT BY
    if ((match(parser, TOKEN_ORDER) && match(parser, TOKEN_BY)) ||
        (match(parser, TOKEN_SORT) && match(parser, TOKEN_BY))) {
        node->as.get_query.order_by = parse_order_by(parser);
    } else {
        node->as.get_query.order_by = NULL;
    }

    // Parse optional LIMIT
    if (match(parser, TOKEN_LIMIT)) {
        node->as.get_query.limit = parse_limit(parser);
    } else {
        node->as.get_query.limit = NULL;
    }

    return node;
}
#endif

/**
 * Parse field list.
 *
 * @param parser The parser instance.
 * @return The AST node representing the field list.
 */
static Node* parse_field_list(Parser* parser) {
    Node* node = create_node(NODE_FIELD_LIST);
    node->line = parser->previous.line;

    // Allocate initial space for fields
    int capacity               = 4;
    node->as.field_list.fields = (Node**)malloc(capacity * sizeof(Node*));
    if (node->as.field_list.fields == NULL) {
        fprintf(stderr, "Error: Out of memory\n");
        exit(EXIT_FAILURE);
    }

    node->as.field_list.count = 0;

    // Parse first field
    if (check(parser, TOKEN_IDENTIFIER) || check(parser, TOKEN_STRING)) {
        Node* field                 = create_node(NODE_IDENTIFIER);
        field->line                 = parser->current.line;
        field->as.identifier.length = parser->current.length;
        field->as.identifier.name   = copy_token_string(&parser->current);

        node->as.field_list.fields[node->as.field_list.count++] = field;
        advance(parser);

        // Parse additional fields separated by commas
        while (match(parser, TOKEN_COMMA)) {
            if (node->as.field_list.count >= capacity) {
                capacity *= 2;
                node->as.field_list.fields =
                    (Node**)realloc(node->as.field_list.fields, capacity * sizeof(Node*));
                if (node->as.field_list.fields == NULL) {
                    fprintf(stderr, "Error: Out of memory\n");
                    exit(EXIT_FAILURE);
                }
            }

            if (check(parser, TOKEN_IDENTIFIER) || check(parser, TOKEN_STRING)) {
                field                       = create_node(NODE_IDENTIFIER);
                field->line                 = parser->current.line;
                field->as.identifier.length = parser->current.length;
                field->as.identifier.name   = copy_token_string(&parser->current);

                node->as.field_list.fields[node->as.field_list.count++] = field;
                advance(parser);
            } else {
                error_at_current(parser, "Expected identifier or string after comma");
                break;
            }
        }
    } else {
        error_at_current(parser, "Expected identifier or string for field list");
    }

    return node;
}

/**
 * Parse a source.
 *
 * @param parser The parser instance.
 * @return The AST node representing the source.
 */
static Node* parse_source(Parser* parser) {
    Node* node = create_node(NODE_SOURCE);
    node->line = parser->previous.line;

    // Parse identifier or string
    if (check(parser, TOKEN_IDENTIFIER) || check(parser, TOKEN_STRING)) {
        node->as.source.identifier                       = create_node(NODE_IDENTIFIER);
        node->as.source.identifier->line                 = parser->current.line;
        node->as.source.identifier->as.identifier.length = parser->current.length;
        node->as.source.identifier->as.identifier.name   = copy_token_string(&parser->current);

        advance(parser);

        // Check for JOIN
        if (match(parser, TOKEN_AND) || match(parser, TOKEN_WITH)) {
            node->as.source.join = parse_join(parser);
        } else {
            node->as.source.join = NULL;
        }
    } else {
        error_at_current(parser, "Expected identifier or string for source");
    }

    return node;
}

/**
 * Parse WITH (JOIN) clause.
 *
 * @param parser The parser instance.
 * @return The AST node representing the JOIN clause.
 */
static Node* parse_join(Parser* parser) {
    Node* node = create_node(NODE_JOIN);
    node->line = parser->previous.line;

    // Parse the joined source
    node->as.join.source = parse_source(parser);

    // Parse join condition
    if (match(parser, TOKEN_WHEN) || match(parser, TOKEN_WHERE)) {
        node->as.join.condition = parse_condition(parser);
    } else {
        error_at_current(parser, "Expected 'WHEN' or 'WHERE' after join source");
    }

    return node;
}

/**
 * Parse condition (WHERE clause).
 *
 * @param parser The parser instance.
 * @return The AST node representing the condition.
 */
static Node* parse_condition(Parser* parser) {
    return parse_expression(parser);
}

/**
 * Parse GROUP BY clause.
 *
 * @param parser The parser instance.
 * @return The AST node representing the GROUP BY clause.
 */
static Node* parse_group_by(Parser* parser) {
    Node* node = create_node(NODE_GROUP_BY);
    node->line = parser->previous.line;

    // Parse fields to group by
    node->as.group_by.fields = parse_field_list(parser);

    // Parse optional HAVING clause
    if (match(parser, TOKEN_HAVING)) {
        node->as.group_by.having = parse_condition(parser);
    } else {
        node->as.group_by.having = NULL;
    }

    return node;
}

/**
 * Parse ORDER BY / SORT BY clause.
 *
 * @param parser The parser instance.
 * @return The AST node representing the ORDER BY / SORT BY clause.
 */
static Node* parse_order_by(Parser* parser) {
    Node* node = create_node(NODE_ORDER_BY);
    node->line = parser->previous.line;

    // Allocate initial space for sort fields
    int capacity                = 4;
    node->as.order_by.fields    = (Node**)malloc(capacity * sizeof(Node*));
    node->as.order_by.ascending = (bool*)malloc(capacity * sizeof(bool));

    if (node->as.order_by.fields == NULL || node->as.order_by.ascending == NULL) {
        fprintf(stderr, "Error: Out of memory\n");
        exit(EXIT_FAILURE);
    }

    node->as.order_by.count = 0;

    // Parse first field
    if (check(parser, TOKEN_IDENTIFIER)) {
        Node* field                 = create_node(NODE_IDENTIFIER);
        field->line                 = parser->current.line;
        field->as.identifier.length = parser->current.length;
        field->as.identifier.name   = copy_token_string(&parser->current);

        node->as.order_by.fields[node->as.order_by.count] = field;

        // Default to ascending order
        node->as.order_by.ascending[node->as.order_by.count] = true;

        advance(parser);

        // Check for ASC/DESC
        if (match(parser, TOKEN_IDENTIFIER)) {
            // Compare with "ASC" or "DESC"
            if (parser->previous.length == 3 && strncmp(parser->previous.start, "ASC", 3) == 0) {
                node->as.order_by.ascending[node->as.order_by.count] = true;
            } else if (parser->previous.length == 4 &&
                       strncmp(parser->previous.start, "DESC", 4) == 0) {
                node->as.order_by.ascending[node->as.order_by.count] = false;
            } else {
                // Not ASC or DESC, must be another field
                error_at_current(parser, "Expected 'ASC', 'DESC', or ','");
            }
        }

        node->as.order_by.count++;

        // Parse additional sort fields
        while (match(parser, TOKEN_COMMA)) {
            if (node->as.order_by.count >= capacity) {
                capacity *= 2;
                node->as.order_by.fields =
                    (Node**)realloc(node->as.order_by.fields, capacity * sizeof(Node*));
                node->as.order_by.ascending =
                    (bool*)realloc(node->as.order_by.ascending, capacity * sizeof(bool));
                if (node->as.order_by.fields == NULL || node->as.order_by.ascending == NULL) {
                    fprintf(stderr, "Error: Out of memory\n");
                    exit(EXIT_FAILURE);
                }
            }

            if (check(parser, TOKEN_IDENTIFIER)) {
                field                       = create_node(NODE_IDENTIFIER);
                field->line                 = parser->current.line;
                field->as.identifier.length = parser->current.length;
                field->as.identifier.name   = copy_token_string(&parser->current);

                node->as.order_by.fields[node->as.order_by.count] = field;

                // Default to ascending order
                node->as.order_by.ascending[node->as.order_by.count] = true;

                advance(parser);

                // Check for ASC/DESC
                if (match(parser, TOKEN_IDENTIFIER)) {
                    // Compare with "ASC" or "DESC"
                    if (parser->previous.length == 3 &&
                        strncmp(parser->previous.start, "ASC", 3) == 0) {
                        node->as.order_by.ascending[node->as.order_by.count] = true;
                    } else if (parser->previous.length == 4 &&
                               strncmp(parser->previous.start, "DESC", 4) == 0) {
                        node->as.order_by.ascending[node->as.order_by.count] = false;
                    } else {
                        // Not ASC or DESC, must be another field
                        error_at_current(parser, "Expected 'ASC', 'DESC', or ','");
                    }
                }

                node->as.order_by.count++;
            } else {
                error_at_current(parser, "Expected identifier after comma in ORDER BY clause");
                break;
            }
        }
    } else {
        error_at_current(parser, "Expected identifier for ORDER BY clause");
    }

    return node;
}

/**
 * Parse LIMIT clause.
 *
 * @param parser The parser instance.
 * @return The AST node representing the LIMIT clause.
 */
static Node* parse_limit(Parser* parser) {
    Node* node = create_node(NODE_LIMIT);
    node->line = parser->previous.line;

    // Parse limit value
    if (check(parser, TOKEN_INTEGER)) {
        char* end;
        node->as.limit.limit = (int)strtol(parser->current.start, &end, 10);
        advance(parser);
    } else {
        error_at_current(parser, "Expected integer for LIMIT clause");
    }

    // Parse optional OFFSET
    if (match(parser, TOKEN_IDENTIFIER) && parser->previous.length == 6 &&
        strncmp(parser->previous.start, "OFFSET", 6) == 0) {
        if (check(parser, TOKEN_INTEGER)) {
            char* end;
            node->as.limit.offset = (int)strtol(parser->current.start, &end, 10);
            advance(parser);
        } else {
            error_at_current(parser, "Expected integer for OFFSET clause");
        }
    } else {
        node->as.limit.offset = 0;
    }

    return node;
}

/**
 * Parse ADD action.
 *
 * @param parser The parser instance.
 * @return The AST node representing the ADD action.
 */
static Node* parse_add_action(Parser* parser) {
    Node* node = create_node(NODE_ADD_ACTION);
    node->line = parser->previous.line;

    // Parse value to add
    node->as.add_action.value = parse_expression(parser);

    // Parse optional record specifications
    if (match(parser, TOKEN_WITH)) {
        node->as.add_action.record_spec = parse_record_spec(parser);
    } else {
        node->as.add_action.record_spec = NULL;
    }

    return node;
}

/**
 * Parse REMOVE action.
 *
 * @param parser The parser instance.
 * @return The AST node representing the REMOVE action.
 */
static Node* parse_remove_action(Parser* parser) {
    Node* node = create_node(NODE_REMOVE_ACTION);
    node->line = parser->previous.line;

    // Parse optional condition (if no condition, remove all)
    if (check(parser, TOKEN_WHEN) || check(parser, TOKEN_IF) || check(parser, TOKEN_WHERE)) {
        node->as.remove_action.condition = parse_condition(parser);
    } else {
        node->as.remove_action.condition = NULL;
    }

    return node;
}

/**
 * Parse UPDATE action.
 *
 * @param parser The parser instance.
 * @return The AST node representing the UPDATE action.
 */
static Node* parse_update_action(Parser* parser) {
    Node* node = create_node(NODE_UPDATE_ACTION);
    node->line = parser->previous.line;

    // Allocate initial space for field/value pairs
    int capacity                  = 4;
    node->as.update_action.fields = (Node**)malloc(capacity * sizeof(Node*));
    node->as.update_action.values = (Node**)malloc(capacity * sizeof(Node*));

    if (node->as.update_action.fields == NULL || node->as.update_action.values == NULL) {
        fprintf(stderr, "Error: Out of memory\n");
        exit(EXIT_FAILURE);
    }

    node->as.update_action.count = 0;

    // Parse first field=value pair
    if (check(parser, TOKEN_IDENTIFIER)) {
        Node* field                 = create_node(NODE_IDENTIFIER);
        field->line                 = parser->current.line;
        field->as.identifier.length = parser->current.length;
        field->as.identifier.name   = copy_token_string(&parser->current);

        node->as.update_action.fields[node->as.update_action.count] = field;

        advance(parser);
        consume(parser, TOKEN_EQUAL, "Expected '=' after field name");

        node->as.update_action.values[node->as.update_action.count] = parse_expression(parser);
        node->as.update_action.count++;

        // Parse additional field=value pairs
        while (match(parser, TOKEN_COMMA)) {
            if (node->as.update_action.count >= capacity) {
                capacity *= 2;
                node->as.update_action.fields =
                    (Node**)realloc(node->as.update_action.fields, capacity * sizeof(Node*));
                node->as.update_action.values =
                    (Node**)realloc(node->as.update_action.values, capacity * sizeof(Node*));
                if (node->as.update_action.fields == NULL ||
                    node->as.update_action.values == NULL) {
                    fprintf(stderr, "Error: Out of memory\n");
                    exit(EXIT_FAILURE);
                }
            }

            if (check(parser, TOKEN_IDENTIFIER)) {
                field                       = create_node(NODE_IDENTIFIER);
                field->line                 = parser->current.line;
                field->as.identifier.length = parser->current.length;
                field->as.identifier.name   = copy_token_string(&parser->current);

                node->as.update_action.fields[node->as.update_action.count] = field;

                advance(parser);
                consume(parser, TOKEN_EQUAL, "Expected '=' after field name");

                node->as.update_action.values[node->as.update_action.count] =
                    parse_expression(parser);
                node->as.update_action.count++;
            } else {
                error_at_current(parser, "Expected identifier after comma in UPDATE action");
                break;
            }
        }
    } else {
        error_at_current(parser, "Expected identifier for UPDATE action");
    }

    return node;
}

/**
 * Parse CREATE action.
 *
 * @param parser The parser instance.
 * @return The AST node representing the CREATE action.
 */
static Node* parse_create_action(Parser* parser) {
    Node* node = create_node(NODE_CREATE_ACTION);
    node->line = parser->previous.line;

    // Allocate initial space for field definitions
    int capacity                      = 4;
    node->as.create_action.field_defs = (Node**)malloc(capacity * sizeof(Node*));

    if (node->as.create_action.field_defs == NULL) {
        fprintf(stderr, "Error: Out of memory\n");
        exit(EXIT_FAILURE);
    }

    node->as.create_action.count = 0;

    // Parse first field definition
    Node* field_def                                                   = parse_field_def(parser);
    node->as.create_action.field_defs[node->as.create_action.count++] = field_def;

    // Parse additional field definitions
    while (match(parser, TOKEN_COMMA)) {
        if (node->as.create_action.count >= capacity) {
            capacity *= 2;
            node->as.create_action.field_defs =
                (Node**)realloc(node->as.create_action.field_defs, capacity * sizeof(Node*));
            if (node->as.create_action.field_defs == NULL) {
                fprintf(stderr, "Error: Out of memory\n");
                exit(EXIT_FAILURE);
            }
        }

        field_def                                                         = parse_field_def(parser);
        node->as.create_action.field_defs[node->as.create_action.count++] = field_def;
    }

    return node;
}

/**
 * Parse record specification.
 *
 * @param parser The parser instance.
 * @return The AST node representing the record specification.
 */
static Node* parse_record_spec(Parser* parser) {
    // TODO: Implement record specification parsing
    return parse_field_list(parser);
}

/**
 * Parse field definition.
 *
 * @param parser The parser instance.
 * @return The AST node representing the field definition.
 */
static Node* parse_field_def(Parser* parser) {
    Node* node = create_node(NODE_FIELD_DEF);
    node->line = parser->previous.line;

    // Parse field name
    if (check(parser, TOKEN_IDENTIFIER)) {
        node->as.field_def.name                       = create_node(NODE_IDENTIFIER);
        node->as.field_def.name->line                 = parser->current.line;
        node->as.field_def.name->as.identifier.length = parser->current.length;
        node->as.field_def.name->as.identifier.name   = copy_token_string(&parser->current);

        advance(parser);
    } else {
        error_at_current(parser, "Expected identifier for field name");
    }

    // Parse optional type
    if (match(parser, TOKEN_AS)) {
        if (check(parser, TOKEN_IDENTIFIER)) {
            node->as.field_def.type = copy_token_string(&parser->current);
            advance(parser);
        } else {
            error_at_current(parser, "Expected identifier for field type");
        }
    } else {
        node->as.field_def.type = NULL;
    }

    // Parse optional constraints
    if (match(parser, TOKEN_LPAREN)) {
        // Allocate initial space for constraints
        int capacity                   = 4;
        node->as.field_def.constraints = (Node**)malloc(capacity * sizeof(Node*));

        if (node->as.field_def.constraints == NULL) {
            fprintf(stderr, "Error: Out of memory\n");
            exit(EXIT_FAILURE);
        }

        node->as.field_def.constraint_count = 0;

        // Parse first constrain
        Node* constraint = parse_constraint(parser);
        node->as.field_def.constraints[node->as.field_def.constraint_count++] = constraint;

        // Parse additional constraints
        while (match(parser, TOKEN_COMMA)) {
            if (node->as.field_def.constraint_count >= capacity) {
                capacity *= 2;
                node->as.field_def.constraints =
                    (Node**)realloc(node->as.field_def.constraints, capacity * sizeof(Node*));
                if (node->as.field_def.constraints == NULL) {
                    fprintf(stderr, "Error: Out of memory\n");
                    exit(EXIT_FAILURE);
                }
            }

            constraint = parse_constraint(parser);
            node->as.field_def.constraints[node->as.field_def.constraint_count++] = constraint;
        }

        consume(parser, TOKEN_RPAREN, "Expected ')' after field constraints");
    } else {
        node->as.field_def.constraints      = NULL;  // No constraints specified
        node->as.field_def.constraint_count = 0;
    }

    return node;
}

/**
 * Parse constraint.
 *
 * @param parser The parser instance.
 * @return The AST node representing the constraint.
 */
static Node* parse_constraint(Parser* parser) {
    Node* node = create_node(NODE_CONSTRAINT);
    node->line = parser->previous.line;

    // Parse constraint type
    if (check(parser, TOKEN_IDENTIFIER)) {
        if (parser->current.length == 8 && strncmp(parser->current.start, "REQUIRED", 8) == 0) {
            node->as.constraint.type          = CONSTRAINT_REQUIRED;
            node->as.constraint.default_value = NULL;
            advance(parser);
        } else if (parser->current.length == 6 &&
                   strncmp(parser->current.start, "UNIQUE", 6) == 0) {
            node->as.constraint.type          = CONSTRAINT_UNIQUE;
            node->as.constraint.default_value = NULL;
            advance(parser);
        } else if (parser->current.length == 7 &&
                   strncmp(parser->current.start, "DEFAULT", 7) == 0) {
            node->as.constraint.type = CONSTRAINT_DEFAULT;
            advance(parser);

            // Parse default value
            node->as.constraint.default_value = parse_expression(parser);
        } else {
            error_at_current(parser, "Expected constraint type (REQUIRED, UNIQUE, DEFAULT)");
        }
    } else {
        error_at_current(parser, "Expected constraint type");
    }

    return node;
}

/**
 * Parse expression (for WHERE, HAVING, etc.).
 *
 * @param parser The parser instance.
 * @return The AST node representing the expression.
 */
static Node* parse_expression(Parser* parser) {
    return parse_logic_or(parser);
}

/**
 * Parse logical OR expression.
 *
 * @param parser The parser instance.
 * @return The AST node representing the logical OR expression.
 */
static Node* parse_logic_or(Parser* parser) {
    Node* left = parse_logic_and(parser);

    while (match(parser, TOKEN_OR)) {
        TokenType op               = parser->previous.type;
        Node*     right            = parse_logic_and(parser);
        Node*     node             = create_node(NODE_BINARY_EXPR);
        node->line                 = parser->previous.line;
        node->as.binary_expr.left  = left;
        node->as.binary_expr.right = right;
        node->as.binary_expr.op    = op;

        left = node;
    }

    return left;
}

/**
 * Parse logical AND expression.
 *
 * @param parser The parser instance.
 * @return The AST node representing the logical AND expression.
 */
static Node* parse_logic_and(Parser* parser) {
    Node* left = parse_equality(parser);

    while (match(parser, TOKEN_AND)) {
        TokenType op               = parser->previous.type;
        Node*     right            = parse_equality(parser);
        Node*     node             = create_node(NODE_BINARY_EXPR);
        node->line                 = parser->previous.line;
        node->as.binary_expr.left  = left;
        node->as.binary_expr.right = right;
        node->as.binary_expr.op    = op;

        left = node;
    }

    return left;
}

/**
 * Parse equality expression.
 *
 * @param parser The parser instance.
 * @return The AST node representing the equality expression.
 */
static Node* parse_equality(Parser* parser) {
    Node* left = parse_comparison(parser);

    while (match(parser, TOKEN_EQUAL) || match(parser, TOKEN_NEQ)) {
        TokenType op               = parser->previous.type;
        Node*     right            = parse_comparison(parser);
        Node*     node             = create_node(NODE_BINARY_EXPR);
        node->line                 = parser->previous.line;
        node->as.binary_expr.left  = left;
        node->as.binary_expr.right = right;
        node->as.binary_expr.op    = op;

        left = node;
    }

    return left;
}

/**
 * Parse comparison expression.
 *
 * @param parser The parser instance.
 * @return The AST node representing the comparison expression.
 */
static Node* parse_comparison(Parser* parser) {
    Node* left = parse_term(parser);

    while (match(parser, TOKEN_LT) || match(parser, TOKEN_LTE) || match(parser, TOKEN_GT) ||
           match(parser, TOKEN_GTE)) {
        TokenType op               = parser->previous.type;
        Node*     right            = parse_term(parser);
        Node*     node             = create_node(NODE_BINARY_EXPR);
        node->line                 = parser->previous.line;
        node->as.binary_expr.left  = left;
        node->as.binary_expr.right = right;
        node->as.binary_expr.op    = op;

        left = node;
    }

    return left;
}

/**
 * Parse term expression.
 *
 * @param parser The parser instance.
 * @return The AST node representing the term expression.
 */
static Node* parse_term(Parser* parser) {
    Node* left = parse_factor(parser);

    while (match(parser, TOKEN_PLUS) || match(parser, TOKEN_MINUS)) {
        TokenType op               = parser->previous.type;
        Node*     right            = parse_factor(parser);
        Node*     node             = create_node(NODE_BINARY_EXPR);
        node->line                 = parser->previous.line;
        node->as.binary_expr.left  = left;
        node->as.binary_expr.right = right;
        node->as.binary_expr.op    = op;

        left = node;
    }

    return left;
}

/**
 * Parse factor expression.
 *
 * @param parser The parser instance.
 * @return The AST node representing the factor expression.
 */
static Node* parse_factor(Parser* parser) {
    Node* left = parse_unary(parser);

    while (match(parser, TOKEN_STAR) || match(parser, TOKEN_SLASH) ||
           match(parser, TOKEN_PERCENT)) {
        TokenType op    = parser->previous.type;
        Node*     right = parse_unary(parser);

        Node* binary                 = create_node(NODE_BINARY_EXPR);
        binary->line                 = left->line;
        binary->as.binary_expr.left  = left;
        binary->as.binary_expr.op    = op;
        binary->as.binary_expr.right = right;
        left                         = binary;
    }

    return left;
}

/**
 * Parse unary expression.
 *
 * @param parser The parser instance.
 * @return The AST node representing the unary expression.
 */
static Node* parse_unary(Parser* parser) {
    if (match(parser, TOKEN_NOT) || match(parser, TOKEN_MINUS)) {
        TokenType op    = parser->previous.type;
        Node*     right = parse_unary(parser);

        Node* unary                  = create_node(NODE_UNARY_EXPR);
        unary->line                  = parser->previous.line;
        unary->as.unary_expr.op      = op;
        unary->as.unary_expr.operand = right;
        return unary;
    }

    return parse_primary(parser);
}

/**
 * Parse primary expression (identifiers, literals, etc.).
 *
 * @param parser The parser instance.
 * @return The AST node representing the primary expression.
 */
static Node* parse_primary(Parser* parser) {
    if (match(parser, TOKEN_STRING)) {
        Node* node                    = create_node(NODE_LITERAL);
        node->line                    = parser->previous.line;
        node->as.literal.literal_type = TOKEN_STRING;

        // Extract string value (excluding quotes)
        int length                          = parser->previous.length - 2;  // Exclude quotes
        node->as.literal.value.string_value = (char*)malloc(length + 1);
        if (node->as.literal.value.string_value == NULL) {
            fprintf(stderr, "Error: Out of memory\n");
            exit(EXIT_FAILURE);
        }

        memcpy(node->as.literal.value.string_value, parser->previous.start + 1,
               length);  // Skip opening quote
        node->as.literal.value.string_value[length] = '\0';

        return node;
    }

    if (match(parser, TOKEN_INTEGER)) {
        Node* node                    = create_node(NODE_LITERAL);
        node->line                    = parser->previous.line;
        node->as.literal.literal_type = TOKEN_INTEGER;

        // Convert string to integer
        char* end;
        node->as.literal.value.number_value = (int)strtol(parser->previous.start, &end, 10);

        return node;
    }

    if (match(parser, TOKEN_DECIMAL)) {
        Node* node                    = create_node(NODE_LITERAL);
        node->line                    = parser->previous.line;
        node->as.literal.literal_type = TOKEN_DECIMAL;

        // Convert string to decimal
        char* end;
        node->as.literal.value.number_value = strtod(parser->previous.start, &end);
        return node;
    }

    if (check(parser, TOKEN_IDENTIFIER)) {
        // Check if it's a function call or just an identifier
        Token id_token = parser->current;
        advance(parser);

        if (check(parser, TOKEN_LPAREN)) {
            // Function call
            parser->lexer->current = parser->lexer->start + (id_token.start - parser->lexer->start);
            parser->current        = id_token;
            return parse_function_call(parser);
        } else {
            Node* node                 = create_node(NODE_IDENTIFIER);
            node->line                 = id_token.line;
            node->as.identifier.length = id_token.length;
            node->as.identifier.name   = (char*)malloc(id_token.length + 1);
            if (node->as.identifier.name == NULL) {
                fprintf(stderr, "Error: Out of memory\n");
                exit(EXIT_FAILURE);
            }

            memcpy(node->as.identifier.name, id_token.start, id_token.length);
            node->as.identifier.name[id_token.length] = '\0';  // Null-terminate the string

            return node;
        }
    }

    if (match(parser, TOKEN_LPAREN)) {
        Node* expr = parse_expression(parser);
        consume(parser, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }

    error_at_current(parser, "Expected expression");
    return NULL;
}

/**
 * Parse function call.
 *
 * @param parser The parser instance.
 * @return The AST node representing the function call.
 */
static Node* parse_function_call(Parser* parser) {
    Node* node = create_node(NODE_FUNCTION_CALL);
    node->line = parser->current.line;

    // Parse function name
    node->as.function_call.name = copy_token_string(&parser->current);
    advance(parser);

    // Parse opening parenthesis
    consume(parser, TOKEN_LPAREN, "Expected '(' after function name");

    // Parse arguments
    int capacity                = 4;
    node->as.function_call.args = (Node**)malloc(sizeof(Node*) * capacity);

    if (node->as.function_call.args == NULL) {
        fprintf(stderr, "Error: Out of memory\n");
        exit(EXIT_FAILURE);
    }

    node->as.function_call.arg_count = 0;

    // Check for arguments
    if (!check(parser, TOKEN_RPAREN)) {
        // Parse first argument
        node->as.function_call.args[node->as.function_call.arg_count++] = parse_expression(parser);

        // Parse additional arguments
        while (match(parser, TOKEN_COMMA)) {
            if (node->as.function_call.arg_count >= capacity) {
                capacity *= 2;
                node->as.function_call.args =
                    (Node**)realloc(node->as.function_call.args, sizeof(Node*) * capacity);
                if (node->as.function_call.args == NULL) {
                    fprintf(stderr, "Error: Out of memory\n");
                    exit(EXIT_FAILURE);
                }
            }

            node->as.function_call.args[node->as.function_call.arg_count++] =
                parse_expression(parser);
        }
    }

    // Parse closing parenthesis
    consume(parser, TOKEN_RPAREN, "Expected ')' after function arguments");

    return node;
}

/**
 * Free AST nodes based on type.
 *
 * @param node The AST node to free.
 */
void free_node(Node* node) {
    if (node == NULL)
        return;

    switch (node->type) {
        case NODE_ASK_QUERY:
            free_node(node->as.ask_query.source);
            free_node(node->as.ask_query.fields);
            free_node(node->as.ask_query.condition);
            free_node(node->as.ask_query.group_by);
            free_node(node->as.ask_query.order_by);
            free_node(node->as.ask_query.limit);
            break;

        case NODE_TELL_QUERY:
            free_node(node->as.tell_query.source);
            free_node(node->as.tell_query.action);
            free_node(node->as.tell_query.condition);
            break;

        case NODE_FIND_QUERY:
            free_node(node->as.find_query.source);
            free_node(node->as.find_query.condition);
            free_node(node->as.find_query.group_by);
            free_node(node->as.find_query.order_by);
            free_node(node->as.find_query.limit);
            break;

        case NODE_SHOW_QUERY:
        case NODE_GET_QUERY:
            free_node(node->as.show_query.source);
            free_node(node->as.show_query.fields);
            free_node(node->as.show_query.condition);
            free_node(node->as.show_query.group_by);
            free_node(node->as.show_query.order_by);
            free_node(node->as.show_query.limit);
            break;

        case NODE_FIELD_LIST:
            for (int i = 0; i < node->as.field_list.count; i++) {
                free_node(node->as.field_list.fields[i]);
            }
            free(node->as.field_list.fields);
            break;

        case NODE_SOURCE:
            free_node(node->as.source.identifier);
            free_node(node->as.source.join);
            break;

        case NODE_JOIN:
            free_node(node->as.join.source);
            free_node(node->as.join.condition);
            break;

        case NODE_GROUP_BY:
            free_node(node->as.group_by.fields);
            free_node(node->as.group_by.having);
            break;

        case NODE_ORDER_BY:
            for (int i = 0; i < node->as.order_by.count; i++) {
                free_node(node->as.order_by.fields[i]);
            }
            free(node->as.order_by.fields);
            free(node->as.order_by.ascending);
            break;

        case NODE_ADD_ACTION:
            free_node(node->as.add_action.value);
            free_node(node->as.add_action.record_spec);
            break;

        case NODE_REMOVE_ACTION:
            free_node(node->as.remove_action.condition);
            break;

        case NODE_UPDATE_ACTION:
            for (int i = 0; i < node->as.update_action.count; i++) {
                free_node(node->as.update_action.fields[i]);
                free_node(node->as.update_action.values[i]);
            }
            free(node->as.update_action.fields);
            free(node->as.update_action.values);
            break;

        case NODE_CREATE_ACTION:
            for (int i = 0; i < node->as.create_action.count; i++) {
                free_node(node->as.create_action.field_defs[i]);
            }
            free(node->as.create_action.field_defs);
            break;

        case NODE_BINARY_EXPR:
            free_node(node->as.binary_expr.left);
            free_node(node->as.binary_expr.right);
            break;

        case NODE_UNARY_EXPR:
            free_node(node->as.unary_expr.operand);
            break;

        case NODE_IDENTIFIER:
            free(node->as.identifier.name);
            break;

        case NODE_FUNCTION_CALL:
            free(node->as.function_call.name);
            for (int i = 0; i < node->as.function_call.arg_count; i++) {
                free_node(node->as.function_call.args[i]);
            }
            free(node->as.function_call.args);
            break;

        case NODE_LITERAL:
            if (node->as.literal.literal_type == TOKEN_STRING) {
                free(node->as.literal.value.string_value);
            }
            break;

        case NODE_FIELD_DEF:
            free_node(node->as.field_def.name);
            if (node->as.field_def.type != NULL) {
                free(node->as.field_def.type);
            }
            if (node->as.field_def.constraints != NULL) {
                for (int i = 0; i < node->as.field_def.constraint_count; i++) {
                    free_node(node->as.field_def.constraints[i]);
                }
                free(node->as.field_def.constraints);
            }
            break;

        case NODE_CONSTRAINT:
            free_node(node->as.constraint.default_value);
            break;

        case NODE_ERROR:
            free(node->as.error.message);
            break;

        default:
            break;
    }

    free(node);
}

/**
 * Helper for printing indentation.
 *
 * @param indent The indentation level.
 */
static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

/**
 * Print AST for debugging.
 *
 * @param node The AST node to print.
 * @param indent The indentation level.
 */
// Print AST for debugging
void print_ast(Node* node, int indent) {
    if (node == NULL) {
        print_indent(indent);
        printf("NULL\n");
        return;
    }

    print_indent(indent);

    switch (node->type) {
        case NODE_ASK_QUERY:
            printf("ASK QUERY:\n");
            print_indent(indent + 1);
            printf("Source:\n");
            print_ast(node->as.ask_query.source, indent + 2);

            print_indent(indent + 1);
            printf("Fields:\n");
            print_ast(node->as.ask_query.fields, indent + 2);

            if (node->as.ask_query.condition != NULL) {
                print_indent(indent + 1);
                printf("Condition:\n");
                print_ast(node->as.ask_query.condition, indent + 2);
            }

            if (node->as.ask_query.group_by != NULL) {
                print_indent(indent + 1);
                printf("Group By:\n");
                print_ast(node->as.ask_query.group_by, indent + 2);
            }

            if (node->as.ask_query.order_by != NULL) {
                print_indent(indent + 1);
                printf("Order By:\n");
                print_ast(node->as.ask_query.order_by, indent + 2);
            }

            if (node->as.ask_query.limit != NULL) {
                print_indent(indent + 1);
                printf("Limit:\n");
                print_ast(node->as.ask_query.limit, indent + 2);
            }
            break;

        case NODE_TELL_QUERY:
            printf("TELL QUERY:\n");
            print_indent(indent + 1);
            printf("Source:\n");
            print_ast(node->as.tell_query.source, indent + 2);

            print_indent(indent + 1);
            printf("Action:\n");
            print_ast(node->as.tell_query.action, indent + 2);

            if (node->as.tell_query.condition != NULL) {
                print_indent(indent + 1);
                printf("Condition:\n");
                print_ast(node->as.tell_query.condition, indent + 2);
            }
            break;

        case NODE_FIND_QUERY:
            printf("FIND QUERY:\n");
            print_indent(indent + 1);
            printf("Source:\n");
            print_ast(node->as.find_query.source, indent + 2);

            if (node->as.find_query.condition != NULL) {
                print_indent(indent + 1);
                printf("Condition:\n");
                print_ast(node->as.find_query.condition, indent + 2);
            }

            if (node->as.find_query.group_by != NULL) {
                print_indent(indent + 1);
                printf("Group By:\n");
                print_ast(node->as.find_query.group_by, indent + 2);
            }

            if (node->as.find_query.order_by != NULL) {
                print_indent(indent + 1);
                printf("Order By:\n");
                print_ast(node->as.find_query.order_by, indent + 2);
            }

            if (node->as.find_query.limit != NULL) {
                print_indent(indent + 1);
                printf("Limit:\n");
                print_ast(node->as.find_query.limit, indent + 2);
            }
            break;

        case NODE_SHOW_QUERY:
            printf("SHOW QUERY:\n");
            print_indent(indent + 1);
            printf("Fields:\n");
            print_ast(node->as.show_query.fields, indent + 2);

            print_indent(indent + 1);
            printf("Source:\n");
            print_ast(node->as.show_query.source, indent + 2);

            if (node->as.show_query.condition != NULL) {
                print_indent(indent + 1);
                printf("Condition:\n");
                print_ast(node->as.show_query.condition, indent + 2);
            }

            if (node->as.show_query.group_by != NULL) {
                print_indent(indent + 1);
                printf("Group By:\n");
                print_ast(node->as.show_query.group_by, indent + 2);
            }

            if (node->as.show_query.order_by != NULL) {
                print_indent(indent + 1);
                printf("Order By:\n");
                print_ast(node->as.show_query.order_by, indent + 2);
            }

            if (node->as.show_query.limit != NULL) {
                print_indent(indent + 1);
                printf("Limit:\n");
                print_ast(node->as.show_query.limit, indent + 2);
            }
            break;

        case NODE_GET_QUERY:
            printf("GET QUERY:\n");
            print_indent(indent + 1);
            printf("Fields:\n");
            print_ast(node->as.get_query.fields, indent + 2);

            print_indent(indent + 1);
            printf("Source:\n");
            print_ast(node->as.get_query.source, indent + 2);

            if (node->as.get_query.condition != NULL) {
                print_indent(indent + 1);
                printf("Condition:\n");
                print_ast(node->as.get_query.condition, indent + 2);
            }

            if (node->as.get_query.group_by != NULL) {
                print_indent(indent + 1);
                printf("Group By:\n");
                print_ast(node->as.get_query.group_by, indent + 2);
            }

            if (node->as.get_query.order_by != NULL) {
                print_indent(indent + 1);
                printf("Order By:\n");
                print_ast(node->as.get_query.order_by, indent + 2);
            }

            if (node->as.get_query.limit != NULL) {
                print_indent(indent + 1);
                printf("Limit:\n");
                print_ast(node->as.get_query.limit, indent + 2);
            }
            break;

        case NODE_FIELD_LIST:
            printf("FIELD LIST (%d fields):\n", node->as.field_list.count);
            for (int i = 0; i < node->as.field_list.count; i++) {
                print_ast(node->as.field_list.fields[i], indent + 1);
            }
            break;

        case NODE_SOURCE:
            printf("SOURCE:\n");
            print_indent(indent + 1);
            printf("Name:\n");
            print_ast(node->as.source.identifier, indent + 2);

            if (node->as.source.join != NULL) {
                print_indent(indent + 1);
                printf("Join:\n");
                print_ast(node->as.source.join, indent + 2);
            }
            break;

        case NODE_JOIN:
            printf("JOIN:\n");
            print_indent(indent + 1);
            printf("Source:\n");
            print_ast(node->as.join.source, indent + 2);

            print_indent(indent + 1);
            printf("Condition:\n");
            print_ast(node->as.join.condition, indent + 2);
            break;

        case NODE_GROUP_BY:
            printf("GROUP BY:\n");
            print_indent(indent + 1);
            printf("Fields:\n");
            print_ast(node->as.group_by.fields, indent + 2);

            if (node->as.group_by.having != NULL) {
                print_indent(indent + 1);
                printf("Having:\n");
                print_ast(node->as.group_by.having, indent + 2);
            }
            break;

        case NODE_ORDER_BY:
            printf("ORDER BY (%d fields):\n", node->as.order_by.count);
            for (int i = 0; i < node->as.order_by.count; i++) {
                print_indent(indent + 1);
                printf("Field %d (%s):\n", i + 1, node->as.order_by.ascending[i] ? "ASC" : "DESC");
                print_ast(node->as.order_by.fields[i], indent + 2);
            }
            break;

        case NODE_LIMIT:
            printf("LIMIT: %d", node->as.limit.limit);
            if (node->as.limit.offset > 0) {
                printf(" OFFSET: %d", node->as.limit.offset);
            }
            printf("\n");
            break;

        case NODE_ADD_ACTION:
            printf("ADD ACTION:\n");
            print_indent(indent + 1);
            printf("Value:\n");
            print_ast(node->as.add_action.value, indent + 2);

            if (node->as.add_action.record_spec != NULL) {
                print_indent(indent + 1);
                printf("Record Spec:\n");
                print_ast(node->as.add_action.record_spec, indent + 2);
            }
            break;

        case NODE_REMOVE_ACTION:
            printf("REMOVE ACTION:\n");
            if (node->as.remove_action.condition != NULL) {
                print_indent(indent + 1);
                printf("Condition:\n");
                print_ast(node->as.remove_action.condition, indent + 2);
            } else {
                print_indent(indent + 1);
                printf("(Remove all)\n");
            }
            break;

        case NODE_UPDATE_ACTION:
            printf("UPDATE ACTION (%d fields):\n", node->as.update_action.count);
            for (int i = 0; i < node->as.update_action.count; i++) {
                print_indent(indent + 1);
                printf("Field %d:\n", i + 1);
                print_ast(node->as.update_action.fields[i], indent + 2);
                print_indent(indent + 1);
                printf("Value %d:\n", i + 1);
                print_ast(node->as.update_action.values[i], indent + 2);
            }
            break;

        case NODE_CREATE_ACTION:
            printf("CREATE ACTION (%d fields):\n", node->as.create_action.count);
            for (int i = 0; i < node->as.create_action.count; i++) {
                print_ast(node->as.create_action.field_defs[i], indent + 1);
            }
            break;

        case NODE_BINARY_EXPR:
            printf("BINARY EXPRESSION:\n");
            print_indent(indent + 1);
            printf("Operator: %s\n", token_type_to_op_string(node->as.binary_expr.op));
            print_indent(indent + 1);
            printf("Left:\n");
            print_ast(node->as.binary_expr.left, indent + 2);

            print_indent(indent + 1);
            printf("Right:\n");
            print_ast(node->as.binary_expr.right, indent + 2);
            break;

        case NODE_UNARY_EXPR:
            printf("UNARY EXPRESSION:\n");
            print_indent(indent + 1);
            printf("Operator: %s\n", token_type_to_op_string(node->as.unary_expr.op));
            print_indent(indent + 1);
            printf("Operand:\n");
            print_ast(node->as.unary_expr.operand, indent + 2);
            break;

        case NODE_IDENTIFIER:
            printf("IDENTIFIER: %s\n", node->as.identifier.name);
            break;

        case NODE_FUNCTION_CALL:
            printf("FUNCTION CALL: %s (%d args)\n", node->as.function_call.name,
                   node->as.function_call.arg_count);
            for (int i = 0; i < node->as.function_call.arg_count; i++) {
                print_indent(indent + 1);
                printf("Arg %d:\n", i + 1);
                print_ast(node->as.function_call.args[i], indent + 2);
            }
            break;

        case NODE_LITERAL:
            switch (node->as.literal.literal_type) {
                case TOKEN_STRING:
                    printf("STRING: \"%s\"\n", node->as.literal.value.string_value);
                    break;
                case TOKEN_INTEGER:
                    printf("INTEGER: %g\n", node->as.literal.value.number_value);
                    break;
                case TOKEN_DECIMAL:
                    printf("DECIMAL: %g\n", node->as.literal.value.number_value);
                    break;
                default:
                    printf("LITERAL: UNKNOWN TYPE\n");
                    break;
            }
            break;

        case NODE_FIELD_DEF:
            printf("FIELD DEFINITION:\n");
            print_indent(indent + 1);
            printf("Name:\n");
            print_ast(node->as.field_def.name, indent + 2);

            if (node->as.field_def.type != NULL) {
                print_indent(indent + 1);
                printf("Type: %s\n", node->as.field_def.type);
            }

            if (node->as.field_def.constraint_count > 0) {
                print_indent(indent + 1);
                printf("Constraints (%d):\n", node->as.field_def.constraint_count);
                for (int i = 0; i < node->as.field_def.constraint_count; i++) {
                    print_ast(node->as.field_def.constraints[i], indent + 2);
                }
            }
            break;

        case NODE_CONSTRAINT:
            printf("CONSTRAINT: ");
            switch (node->as.constraint.type) {
                case CONSTRAINT_REQUIRED:
                    printf("REQUIRED\n");
                    break;
                case CONSTRAINT_UNIQUE:
                    printf("UNIQUE\n");
                    break;
                case CONSTRAINT_DEFAULT:
                    printf("DEFAULT\n");
                    print_indent(indent + 1);
                    printf("Value:\n");
                    print_ast(node->as.constraint.default_value, indent + 2);
                    break;
            }
            break;

        case NODE_ERROR:
            printf("ERROR: %s\n", node->as.error.message);
            break;

        default:
            printf("UNKNOWN NODE TYPE: %d\n", node->type);
            break;
    }
}