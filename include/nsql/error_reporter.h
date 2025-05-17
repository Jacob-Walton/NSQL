/**
 * @file error_reporter.h
 * @brief Error reporting system for NSQL
 */

#ifndef NSQL_ERROR_REPORTER_H
#define NSQL_ERROR_REPORTER_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Error severity levels
 */
typedef enum { ERROR_NONE, ERROR_WARNING, ERROR_ERROR, ERROR_FATAL } ErrorSeverity;

/**
 * Error source types
 */
typedef enum {
    ERROR_SOURCE_LEXER,
    ERROR_SOURCE_PARSER,
    ERROR_SOURCE_SEMANTIC,
    ERROR_SOURCE_RUNTIME,
    ERROR_SOURCE_SYSTEM
} ErrorSource;

/**
 * Error report structure
 */
typedef struct ErrorReport {
    ErrorSeverity       severity;
    ErrorSource         source;
    int                 line;
    int                 column;
    const char*         message;
    struct ErrorReport* next;  // For linked list of multiple errors
} ErrorReport;

/**
 * Error context structure
 */
typedef struct {
    ErrorReport* first_error;
    ErrorReport* last_error;
    int          error_count;
    int          warning_count;
    bool         has_error;
    bool         has_fatal;
} ErrorContext;

/**
 * Initialize error context
 *
 * @param ctx The error context to initialize
 */
void error_context_init(ErrorContext* ctx);

/**
 * Free error context and all reports
 *
 * @param ctx The error context to free
 */
void error_context_free(ErrorContext* ctx);

/**
 * Report an error
 *
 * @param ctx The error context
 * @param severity The error severity
 * @param source The error source
 * @param line The line number where the error occurred
 * @param column The column number where the error occurred
 * @param message The error message
 * @return true if the report was added successfully
 */
bool report_error(ErrorContext* ctx, ErrorSeverity severity, ErrorSource source, int line,
                  int column, const char* message);

/**
 * Format all errors in the context into a string
 *
 * @param ctx The error context
 * @param buffer The buffer to write to
 * @param size The size of the buffer
 * @return The number of bytes written (excluding null terminator)
 */
size_t format_errors(const ErrorContext* ctx, char* buffer, size_t size);

/**
 * Format all errors in the context into a string in JSON format
 *
 * @param ctx The error context
 * @param buffer The buffer to write to
 * @param size The size of the buffer
 * @return The number of bytes written (excluding null terminator)
 */
size_t format_errors_json(const ErrorContext* ctx, char* buffer, size_t size);

#endif /* NSQL_ERROR_REPORTER_H */
