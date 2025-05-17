/**
 * @file ast_printer.h
 * @brief AST printing utilities for NSQL
 */

#ifndef NSQL_AST_PRINTER_H
#define NSQL_AST_PRINTER_H

#include <nsql/ast.h>
#include <stdio.h>
#include <stddef.h>

/**
 * Output format for AST printing
 */
typedef enum {
    AST_FORMAT_TEXT,  // Human-readable text format
    AST_FORMAT_JSON,  // JSON format
    AST_FORMAT_XML,   // XML format
    AST_FORMAT_DOT    // GraphViz DOT format for visualization
} AstOutputFormat;

/**
 * Output type for AST printing
 */
typedef enum {
    AST_OUTPUT_FILE,      // Print to a file
    AST_OUTPUT_BUFFER,    // Print to a memory buffer
    AST_OUTPUT_CALLBACK   // Call a function for each node
} AstOutputType;

/**
 * Callback function type for AST printing
 * 
 * @param node The node being processed
 * @param depth The depth of the node in the tree
 * @param user_data User data passed to the printer
 * @return true to continue traversal, false to stop
 */
typedef bool (*AstPrintCallback)(const Node* node, int depth, void* user_data);

/**
 * AST printer configuration
 */
typedef struct {
    AstOutputFormat format;
    AstOutputType type;
    union {
        FILE* file;               // For AST_OUTPUT_FILE
        struct {
            char* buffer;        // For AST_OUTPUT_BUFFER
            size_t size;
            size_t written;
        } buf;
        struct {
            AstPrintCallback fn; // For AST_OUTPUT_CALLBACK
            void* user_data;
        } callback;
    } output;
    int indent_size;              // Number of spaces per indentation level
    bool pretty_print;            // Whether to format with indentation and newlines
    bool include_line_numbers;    // Whether to include line numbers in output
} AstPrinter;

/**
 * Initialize an AST printer for file output
 * 
 * @param printer The printer to initialize
 * @param format The output format
 * @param file The file to write to (must be opened for writing)
 * @return true if initialization succeeded
 */
bool ast_printer_init_file(AstPrinter* printer, AstOutputFormat format, FILE* file);

/**
 * Initialize an AST printer for buffer output
 * 
 * @param printer The printer to initialize
 * @param format The output format
 * @param buffer The buffer to write to
 * @param size The size of the buffer
 * @return true if initialization succeeded
 */
bool ast_printer_init_buffer(AstPrinter* printer, AstOutputFormat format, char* buffer, size_t size);

/**
 * Initialize an AST printer for callback output
 * 
 * @param printer The printer to initialize
 * @param format The output format
 * @param callback The callback function
 * @param user_data User data to pass to the callback
 * @return true if initialization succeeded
 */
bool ast_printer_init_callback(AstPrinter* printer, AstOutputFormat format, 
                               AstPrintCallback callback, void* user_data);

/**
 * Print an AST node and its children
 * 
 * @param printer The printer to use
 * @param node The node to print
 * @return true if printing succeeded
 */
bool ast_printer_print(AstPrinter* printer, const Node* node);

/**
 * Free any resources used by the printer
 * 
 * @param printer The printer to free
 */
void ast_printer_free(AstPrinter* printer);

/**
 * Get the number of bytes written to the buffer
 * 
 * @param printer The printer (must be initialized with ast_printer_init_buffer)
 * @return The number of bytes written, or 0 if the printer is not a buffer printer
 */
size_t ast_printer_get_written(const AstPrinter* printer);

#endif /* NSQL_AST_PRINTER_H */
