/**
 * @file ast_printer.c
 * @brief Implementation of AST printing utilities
 */

#include <nsql/ast_printer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Writes a string to the output destination specified in the AstPrinter.
 *
 * Writes the given string to a file or buffer, depending on the printer's output type.
 * For file output, writes using fputs. For buffer output, copies the string into the buffer,
 * ensuring null termination and preventing overflow. Returns false on write failure or if
 * called with an unsupported output type (such as callback).
 *
 * @param printer Pointer to the AstPrinter specifying the output destination.
 * @param str Null-terminated string to write. If NULL, nothing is written and the function returns
 * true.
 * @return true if the write succeeds or nothing is written; false on failure or unsupported output
 * type.
 */
static bool printer_write(AstPrinter* printer, const char* str) {
    if (!str)
        return true;  // Nothing to write

    size_t len = strlen(str);

    switch (printer->type) {
        case AST_OUTPUT_FILE:
            if (fputs(str, printer->output.file) == EOF) {
                return false;
            }
            break;

        case AST_OUTPUT_BUFFER: {
            size_t remaining = printer->output.buf.size - printer->output.buf.written;
            if (len >= remaining) {
                len = remaining > 0 ? remaining - 1 : 0;  // Reserve space for null terminator
            }
            if (len > 0) {
                memcpy(printer->output.buf.buffer + printer->output.buf.written, str, len);
                printer->output.buf.written += len;
                // Ensure null termination
                if (printer->output.buf.written < printer->output.buf.size) {
                    printer->output.buf.buffer[printer->output.buf.written] = '\0';
                } else if (printer->output.buf.size > 0) {
                    printer->output.buf.buffer[printer->output.buf.size - 1] = '\0';
                }
            }
            break;
        }

        case AST_OUTPUT_CALLBACK:
            /* Ignore and continue */
            return true;
    }

    return true;
}

/**
 * @brief Writes indentation spaces to the output based on depth and printer settings.
 *
 * Generates and writes a string of spaces corresponding to the current indentation level if pretty
 * printing is enabled. Indentation is capped to prevent buffer overflow.
 *
 * @param depth The current depth in the AST, used to calculate indentation.
 * @return true on successful write; false if writing fails.
 */
static bool printer_write_indent(AstPrinter* printer, int depth) {
    if (!printer->pretty_print)
        return true;

    char indent_buf[128];
    int  indent_size = depth * printer->indent_size;

    // Cap indentation to prevent buffer overflow
    if (indent_size > (int)sizeof(indent_buf) - 1) {
        indent_size = sizeof(indent_buf) - 1;
    }

    // Fill indent buffer with spaces
    memset(indent_buf, ' ', indent_size);
    indent_buf[indent_size] = '\0';

    return printer_write(printer, indent_buf);
}

// Forward declaration of recursive printer function
static bool print_node_recursive(AstPrinter* printer, const Node* node, int depth);

/**
 * @brief Prints a binary expression node in a human-readable text format.
 *
 * Outputs the operator and recursively prints the left and right operands with increased
 * indentation.
 *
 * @param node The binary expression node to print.
 * @param depth The current indentation depth for pretty printing.
 * @return true if printing succeeds for the node and its children, false otherwise.
 */
static bool print_binary_expr_text(AstPrinter* printer, const Node* node, int depth) {
    printer_write(printer, "BINARY EXPRESSION:\n");
    printer_write_indent(printer, depth + 1);

    // Print operator
    const char* op_str = "UNKNOWN";
    switch (node->as.binary_expr.op) {
        case TOKEN_PLUS:
            op_str = "+";
            break;
        case TOKEN_MINUS:
            op_str = "-";
            break;
        case TOKEN_STAR:
            op_str = "*";
            break;
        case TOKEN_SLASH:
            op_str = "/";
            break;
        case TOKEN_EQUAL:
            op_str = "=";
            break;
        case TOKEN_NEQ:
            op_str = "!=";
            break;
        case TOKEN_LT:
            op_str = "<";
            break;
        case TOKEN_GT:
            op_str = ">";
            break;
        case TOKEN_LTE:
            op_str = "<=";
            break;
        case TOKEN_GTE:
            op_str = ">=";
            break;
        case TOKEN_AND:
            op_str = "AND";
            break;
        case TOKEN_OR:
            op_str = "OR";
            break;
    }

    char op_buf[64];
    snprintf(op_buf, sizeof(op_buf), "Operator: %s\n", op_str);
    printer_write(printer, op_buf);

    // Print left operand
    printer_write_indent(printer, depth + 1);
    printer_write(printer, "Left:\n");
    if (!print_node_recursive(printer, node->as.binary_expr.left, depth + 2)) {
        return false;
    }

    // Print right operand
    printer_write_indent(printer, depth + 1);
    printer_write(printer, "Right:\n");
    if (!print_node_recursive(printer, node->as.binary_expr.right, depth + 2)) {
        return false;
    }

    return true;
}

/**
 * @brief Prints an AST node in JSON format to the configured output.
 *
 * Outputs the node's type, optional line number, and node-specific fields such as identifier names,
 * literal values, or binary operators. Indentation is applied if pretty printing is enabled.
 * Returns false if writing fails at any point.
 *
 * @param printer The AST printer configured for output.
 * @param node The AST node to print; prints "null" if the node is NULL.
 * @param depth The current depth in the AST, used for indentation when pretty printing.
 * @return true if the node was printed successfully; false on write failure.
 */
static bool print_node_json(AstPrinter* printer, const Node* node, int depth) {
    // Use depth parameter to avoid warning
    if (printer->pretty_print && depth > 0) {
        // Add indentation if pretty printing is enabled
        for (int i = 0; i < depth * printer->indent_size; i++) {
            if (!printer_write(printer, " "))
                return false;
        }
    }

    if (!node) {
        return printer_write(printer, "null");
    }

    // Start object
    if (!printer_write(printer, "{"))
        return false;

    // Node type
    if (!printer_write(printer, "\"type\":\""))
        return false;

    // Print type name based on enum
    const char* type_name = "unknown";
    switch (node->type) {
        case NODE_ASK_QUERY:
            type_name = "ask_query";
            break;
        case NODE_TELL_QUERY:
            type_name = "tell_query";
            break;
        case NODE_FIND_QUERY:
            type_name = "find_query";
            break;
        case NODE_SHOW_QUERY:
            type_name = "show_query";
            break;
        case NODE_GET_QUERY:
            type_name = "get_query";
            break;
        case NODE_FIELD_LIST:
            type_name = "field_list";
            break;
        case NODE_SOURCE:
            type_name = "source";
            break;
        case NODE_JOIN:
            type_name = "join";
            break;
        case NODE_GROUP_BY:
            type_name = "group_by";
            break;
        case NODE_ORDER_BY:
            type_name = "order_by";
            break;
        case NODE_LIMIT:
            type_name = "limit";
            break;
        case NODE_ADD_ACTION:
            type_name = "add_action";
            break;
        case NODE_REMOVE_ACTION:
            type_name = "remove_action";
            break;
        case NODE_UPDATE_ACTION:
            type_name = "update_action";
            break;
        case NODE_CREATE_ACTION:
            type_name = "create_action";
            break;
        case NODE_BINARY_EXPR:
            type_name = "binary_expr";
            break;
        case NODE_UNARY_EXPR:
            type_name = "unary_expr";
            break;
        case NODE_IDENTIFIER:
            type_name = "identifier";
            break;
        case NODE_LITERAL:
            type_name = "literal";
            break;
        case NODE_FIELD_DEF:
            type_name = "field_def";
            break;
        case NODE_CONSTRAINT:
            type_name = "constraint";
            break;
        case NODE_FUNCTION_CALL:
            type_name = "function_call";
            break;
        case NODE_ERROR:
            type_name = "error";
            break;
        case NODE_PROGRAM:
            type_name = "program";
            break;
    }

    if (!printer_write(printer, type_name))
        return false;
    if (!printer_write(printer, "\""))
        return false;

    // Line number
    if (printer->include_line_numbers) {
        char line_buf[32];
        snprintf(line_buf, sizeof(line_buf), ",\"line\":%d", node->line);
        if (!printer_write(printer, line_buf))
            return false;
    }

    // Specific node contents based on type
    switch (node->type) {
        case NODE_IDENTIFIER:
            if (!printer_write(printer, ",\"name\":\""))
                return false;
            if (!printer_write(printer, node->as.identifier.name))
                return false;
            if (!printer_write(printer, "\""))
                return false;
            break;

        case NODE_LITERAL:
            if (node->as.literal.literal_type == TOKEN_STRING) {
                if (!printer_write(printer, ",\"value\":\""))
                    return false;
                if (!printer_write(printer, node->as.literal.value.string_value))
                    return false;
                if (!printer_write(printer, "\",\"literalType\":\"string\""))
                    return false;
            } else {
                char value_buf[64];
                snprintf(value_buf, sizeof(value_buf), ",\"value\":%g,\"literalType\":\"%s\"",
                         node->as.literal.value.number_value,
                         node->as.literal.literal_type == TOKEN_INTEGER ? "integer" : "decimal");
                if (!printer_write(printer, value_buf))
                    return false;
            }
            break;

            // Add handling for other node types here
            // This is a simplified version - a full implementation would handle all node types

        default:
            // For complex node types, we'll add a children array
            if (node->type == NODE_BINARY_EXPR) {
                // For binary expressions, show the operator
                const char* op_str = "unknown";
                switch (node->as.binary_expr.op) {
                    case TOKEN_PLUS:
                        op_str = "+";
                        break;
                    case TOKEN_MINUS:
                        op_str = "-";
                        break;
                    case TOKEN_STAR:
                        op_str = "*";
                        break;
                    case TOKEN_SLASH:
                        op_str = "/";
                        break;
                    case TOKEN_EQUAL:
                        op_str = "=";
                        break;
                    case TOKEN_NEQ:
                        op_str = "!=";
                        break;
                    case TOKEN_LT:
                        op_str = "<";
                        break;
                    case TOKEN_GT:
                        op_str = ">";
                        break;
                    case TOKEN_LTE:
                        op_str = "<=";
                        break;
                    case TOKEN_GTE:
                        op_str = ">=";
                        break;
                    case TOKEN_AND:
                        op_str = "AND";
                        break;
                    case TOKEN_OR:
                        op_str = "OR";
                        break;
                    default:
                        break;
                }
                if (!printer_write(printer, ",\"operator\":\""))
                    return false;
                if (!printer_write(printer, op_str))
                    return false;
                if (!printer_write(printer, "\""))
                    return false;
            }
            break;
    }

    // Close the object
    if (!printer_write(printer, "}"))
        return false;

    return true;
}

/**
 * @brief Recursively prints an AST node in the selected format.
 *
 * Prints the given AST node and its children using the format specified in the printer
 * configuration (text or JSON). Handles indentation and pretty-printing as configured. Returns
 * false if printing fails or if the format is not supported.
 *
 * @param printer Configured AST printer specifying output format and destination.
 * @param node AST node to print; prints "NULL" if node is null.
 * @param depth Current depth in the AST, used for indentation.
 * @return true if the node and its children were printed successfully; false on failure or
 * unsupported format.
 */
static bool print_node_recursive(AstPrinter* printer, const Node* node, int depth) {
    if (!node) {
        printer_write_indent(printer, depth);
        return printer_write(printer, "NULL\n");
    }

    switch (printer->format) {
        case AST_FORMAT_TEXT:
            printer_write_indent(printer, depth);

            switch (node->type) {
                case NODE_BINARY_EXPR:
                    return print_binary_expr_text(printer, node, depth);

                case NODE_IDENTIFIER:
                    return printer_write(printer, "IDENTIFIER: ") &&
                           printer_write(printer, node->as.identifier.name) &&
                           printer_write(printer, "\n");

                case NODE_LITERAL:
                    if (node->as.literal.literal_type == TOKEN_STRING) {
                        return printer_write(printer, "STRING: \"") &&
                               printer_write(printer, node->as.literal.value.string_value) &&
                               printer_write(printer, "\"\n");
                    } else {
                        char        value_buf[64];
                        const char* type_str =
                            node->as.literal.literal_type == TOKEN_INTEGER ? "INTEGER" : "DECIMAL";
                        snprintf(value_buf, sizeof(value_buf), "%s: %g\n", type_str,
                                 node->as.literal.value.number_value);
                        return printer_write(printer, value_buf);
                    }

                    // TODO: Implement all node types

                default: {
                    // Generic node type printer
                    char type_buf[64];
                    snprintf(type_buf, sizeof(type_buf), "NODE TYPE %d\n", node->type);
                    return printer_write(printer, type_buf);
                }
            }
            break;

        case AST_FORMAT_JSON:
            printer_write_indent(printer, depth);
            if (!print_node_json(printer, node, depth)) {
                return false;
            }
            if (printer->pretty_print) {
                return printer_write(printer, "\n");
            }
            return true;

        case AST_FORMAT_XML:
            // XML format implementation would go here
            return false;

        case AST_FORMAT_DOT:
            // DOT format implementation would go here
            return false;
    }

    return false;
}

/**
 * @brief Traverses the AST in depth-first order and invokes a callback for each node.
 *
 * For each node in the AST, calls the user-provided callback function with the node and its depth.
 * Traversal is performed using an explicit stack to avoid recursion. Currently, only binary
 * expression nodes have their children pushed for traversal; support for other node types can be
 * added as needed.
 *
 * @return true if traversal completes successfully; false if memory allocation fails or the printer
 * is misconfigured.
 */
static bool print_node_callback(AstPrinter* printer, const Node* node) {
    if (printer->type != AST_OUTPUT_CALLBACK || !printer->output.callback.fn) {
        return false;
    }

    // Visit the AST in a depth-first manner
    typedef struct VisitItem {
        const Node*       node;
        int               depth;
        struct VisitItem* next;
    } VisitItem;

    VisitItem* stack = NULL;
    VisitItem* item  = malloc(sizeof(VisitItem));
    if (!item) {
        /* Free remaining stack */
        while (stack) {
            VisitItem* temp = stack;
            stack           = stack->next;
            free(temp);
        }
        return false;
    }

    item->node  = node;
    item->depth = 0;
    item->next  = NULL;
    stack       = item;

    while (stack) {
        // Pop from stack
        item  = stack;
        stack = stack->next;

        // Process current node
        const Node* current = item->node;
        int         depth   = item->depth;
        free(item);

        // Call the callback
        if (current) {
            // Check the return value of the callback - if false, stop traversal
            if (!printer->output.callback.fn(current, depth, printer->output.callback.user_data)) {
                // Free any remaining stack items
                while (stack) {
                    item  = stack;
                    stack = stack->next;
                    free(item);
                }
                return false;  // Propagate the return value to caller
            }

            // Push children to stack (in reverse order so they get processed in the right order)
            // This would need to be customized for each node type
            // Here's an example for binary expressions
            if (current->type == NODE_BINARY_EXPR) {
                // Push right child first (will be processed second)
                if (current->as.binary_expr.right) {
                    item = malloc(sizeof(VisitItem));
                    if (!item)
                        return false;
                    item->node  = current->as.binary_expr.right;
                    item->depth = depth + 1;
                    item->next  = stack;
                    stack       = item;
                }

                // Push left child second (will be processed first)
                if (current->as.binary_expr.left) {
                    item = malloc(sizeof(VisitItem));
                    if (!item)
                        return false;
                    item->node  = current->as.binary_expr.left;
                    item->depth = depth + 1;
                    item->next  = stack;
                    stack       = item;
                }
            }
            // Add pushing logic for other node types
        }
    }

    return true;
}

/**
 * @brief Initializes an AstPrinter for file output with the specified format.
 *
 * Configures the printer to write AST output to a given file stream, enabling pretty printing and
 * line numbers by default.
 *
 * @param printer Pointer to the AstPrinter to initialize.
 * @param format Output format for the AST (e.g., text, JSON).
 * @param file File stream to which the AST will be written.
 * @return true if initialization succeeds; false if printer or file is NULL.
 */
bool ast_printer_init_file(AstPrinter* printer, AstOutputFormat format, FILE* file) {
    if (!printer || !file)
        return false;

    memset(printer, 0, sizeof(AstPrinter));
    printer->format               = format;
    printer->type                 = AST_OUTPUT_FILE;
    printer->output.file          = file;
    printer->indent_size          = 2;
    printer->pretty_print         = true;
    printer->include_line_numbers = true;

    return true;
}

/**
 * @brief Initializes an AstPrinter for output to a character buffer.
 *
 * Configures the printer to write AST output in the specified format to a provided buffer,
 * enabling pretty printing and line numbers by default. The buffer is null-terminated and
 * its size is respected to prevent overflow.
 *
 * @param printer Pointer to the AstPrinter to initialize.
 * @param format Output format for the AST (e.g., text, JSON).
 * @param buffer Destination buffer for output.
 * @param size Size of the buffer in bytes.
 * @return true if initialization succeeds; false if arguments are invalid.
 */
bool ast_printer_init_buffer(AstPrinter* printer, AstOutputFormat format, char* buffer,
                             size_t size) {
    if (!printer || !buffer || size == 0)
        return false;

    memset(printer, 0, sizeof(AstPrinter));
    printer->format               = format;
    printer->type                 = AST_OUTPUT_BUFFER;
    printer->output.buf.buffer    = buffer;
    printer->output.buf.size      = size;
    printer->output.buf.written   = 0;
    printer->indent_size          = 2;
    printer->pretty_print         = true;
    printer->include_line_numbers = true;

    // Ensure the buffer is null-terminated initially
    buffer[0] = '\0';

    return true;
}

/**
 * @brief Initializes an AstPrinter for callback-based output.
 *
 * Configures the printer to traverse the AST and invoke a user-provided callback function for each
 * node, using the specified output format.
 *
 * @param printer Pointer to the AstPrinter to initialize.
 * @param format Output format to use when printing nodes.
 * @param callback Function to call for each node during traversal.
 * @param user_data User-defined data passed to the callback function.
 * @return true if initialization succeeds; false if printer or callback is NULL.
 */
bool ast_printer_init_callback(AstPrinter* printer, AstOutputFormat format,
                               AstPrintCallback callback, void* user_data) {
    if (!printer || !callback)
        return false;

    memset(printer, 0, sizeof(AstPrinter));
    printer->format                    = format;
    printer->type                      = AST_OUTPUT_CALLBACK;
    printer->output.callback.fn        = callback;
    printer->output.callback.user_data = user_data;
    printer->indent_size               = 2;
    printer->pretty_print              = true;
    printer->include_line_numbers      = true;

    return true;
}

/**
 * @brief Prints an AST node using the specified printer configuration.
 *
 * Selects the appropriate output method (file, buffer, or callback) and format (text, JSON, etc.)
 * as configured in the printer. Returns false if the printer is invalid or if printing fails.
 *
 * @param printer Configured AST printer specifying output type and format.
 * @param node Root AST node to print.
 * @return true if printing succeeds; false otherwise.
 */
bool ast_printer_print(AstPrinter* printer, const Node* node) {
    if (!printer)
        return false;

    if (printer->type == AST_OUTPUT_CALLBACK) {
        return print_node_callback(printer, node);
    } else {
        // For file and buffer output, use the recursive printer
        return print_node_recursive(printer, node, 0);
    }
}

/**
 * @brief Releases resources associated with an AstPrinter.
 *
 * Currently a placeholder; does not perform any operations.
 */
void ast_printer_free(AstPrinter* printer) {
    // Nothing to free for now, but implemented for future expansion
    (void)printer;
}

/**
 * @brief Returns the number of bytes written to the output buffer.
 *
 * If the printer is not configured for buffer output or is invalid, returns 0.
 *
 * @param printer Pointer to the AstPrinter instance.
 * @return Number of bytes written to the buffer, or 0 if not applicable.
 */
size_t ast_printer_get_written(const AstPrinter* printer) {
    if (!printer || printer->type != AST_OUTPUT_BUFFER) {
        return 0;
    }
    return printer->output.buf.written;
}
