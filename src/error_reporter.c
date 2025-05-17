/**
 * @file error_reporter.c
 * @brief Implementation of the error reporting system
 */

#include <nsql/error_reporter.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Initializes an ErrorContext structure for error reporting.
 *
 * Sets all fields of the ErrorContext to their initial states, clearing any existing error data and
 * resetting counters and flags. Does nothing if the context pointer is null.
 */
void error_context_init(ErrorContext* ctx) {
    if (!ctx)
        return;
    ctx->first_error   = NULL;
    ctx->last_error    = NULL;
    ctx->error_count   = 0;
    ctx->warning_count = 0;
    ctx->has_error     = false;
    ctx->has_fatal     = false;
}

/**
 * @brief Frees all error reports and resets the error context.
 *
 * Releases memory allocated for error messages and error reports in the given context,
 * and resets all context fields to their initial state. Does nothing if the context is NULL.
 */
void error_context_free(ErrorContext* ctx) {
    if (!ctx)
        return;

    ErrorReport* current = ctx->first_error;
    while (current != NULL) {
        ErrorReport* next = current->next;
        free((void*)current->message);  // Free the duplicated message
        free(current);
        current = next;
    }
    ctx->first_error   = NULL;
    ctx->last_error    = NULL;
    ctx->error_count   = 0;
    ctx->warning_count = 0;
    ctx->has_error     = false;
    ctx->has_fatal     = false;
}

/**
 * @brief Adds a new error or warning to the error context.
 *
 * Allocates and stores an error report with the specified severity, source, location, and message
 * in the given error context. Updates error and warning counts and flags accordingly.
 *
 * @param ctx Pointer to the error context to which the error will be added.
 * @param severity The severity level of the error or warning.
 * @param source The source component where the error originated.
 * @param line The line number associated with the error.
 * @param column The column number associated with the error.
 * @param message The error or warning message to record.
 * @return true if the error was successfully reported; false if the context or message is null, or
 * if memory allocation fails.
 */
bool report_error(ErrorContext* ctx, ErrorSeverity severity, ErrorSource source, int line,
                  int column, const char* message) {
    if (!ctx || !message)
        return false;

    // Create new error report
    ErrorReport* report = (ErrorReport*)malloc(sizeof(ErrorReport));
    if (!report)
        return false;

    // Make a copy of the message to ensure it remains valid
    char* message_copy = strdup(message);
    if (!message_copy) {
        free(report);
        return false;
    }

    // Initialize report
    report->severity = severity;
    report->source   = source;
    report->line     = line;
    report->column   = column;
    report->message  = message_copy;
    report->next     = NULL;

    // Add to list
    if (ctx->last_error) {
        ctx->last_error->next = report;
    } else {
        ctx->first_error = report;
    }
    ctx->last_error = report;

    // Update counts
    if (severity == ERROR_WARNING) {
        ctx->warning_count++;
    } else if (severity >= ERROR_ERROR) {
        ctx->error_count++;
        ctx->has_error = true;
        if (severity == ERROR_FATAL) {
            ctx->has_fatal = true;
        }
    }

    return true;
}

/**
 * @brief Returns the string name corresponding to an error source.
 *
 * Maps an ErrorSource enum value to its human-readable string representation.
 *
 * @param source The error source enum value.
 * @return const char* The name of the error source, or "Unknown" if unrecognized.
 */
static const char* get_source_name(ErrorSource source) {
    switch (source) {
        case ERROR_SOURCE_LEXER:
            return "Lexer";
        case ERROR_SOURCE_PARSER:
            return "Parser";
        case ERROR_SOURCE_SEMANTIC:
            return "Semantic";
        case ERROR_SOURCE_RUNTIME:
            return "Runtime";
        case ERROR_SOURCE_SYSTEM:
            return "System";
        default:
            return "Unknown";
    }
}

/**
 * @brief Returns the string name corresponding to an error severity level.
 *
 * @param severity The error severity enum value.
 * @return const char* String literal representing the severity ("Info", "Warning", "Error",
 * "Fatal", or "Unknown").
 */
static const char* get_severity_name(ErrorSeverity severity) {
    switch (severity) {
        case ERROR_NONE:
            return "Info";
        case ERROR_WARNING:
            return "Warning";
        case ERROR_ERROR:
            return "Error";
        case ERROR_FATAL:
            return "Fatal";
        default:
            return "Unknown";
    }
}

/**
 * @brief Formats all errors in the context as a human-readable string.
 *
 * Writes a summary of error and warning counts followed by detailed entries for each error in the
 * provided buffer. The output is truncated if it would exceed the buffer size and is always
 * null-terminated.
 *
 * @param ctx Pointer to the error context containing error reports.
 * @param buffer Destination buffer for the formatted string.
 * @param size Size of the destination buffer in bytes.
 * @return Number of characters written to the buffer, excluding the null terminator. Returns 0 if
 * inputs are invalid or the buffer size is zero.
 */
size_t format_errors(const ErrorContext* ctx, char* buffer, size_t size) {
    if (!ctx || !buffer || size == 0)
        return 0;

    size_t written   = 0;
    size_t remaining = size - 1;  // Reserve space for null terminator

    // Write summary header
    int chars = snprintf(buffer, remaining, "NSQL Parsing Results: %d error(s), %d warning(s)\n\n",
                         ctx->error_count, ctx->warning_count);

    if (chars < 0)
        return 0;
    if ((size_t)chars >= remaining) {
        buffer[remaining] = '\0';
        return size - 1;
    }

    written += (size_t)chars;
    buffer += chars;
    remaining -= (size_t)chars;

    // Format each error
    ErrorReport* current = ctx->first_error;
    while (current && remaining > 0) {
        const char* severity = get_severity_name(current->severity);
        const char* source   = get_source_name(current->source);

        chars = snprintf(buffer, remaining, "[%s] %s (line %d, col %d): %s\n", severity, source,
                         current->line, current->column, current->message);

        if (chars < 0)
            break;
        if ((size_t)chars >= remaining) {
            buffer[remaining] = '\0';
            written += remaining;
            break;
        }

        written += (size_t)chars;
        buffer += chars;
        remaining -= (size_t)chars;
        current = current->next;
    }

    // Ensure null termination
    *buffer = '\0';
    return written;
}

/**
 * @brief Appends a JSON-escaped string to a buffer.
 *
 * Escapes special characters in JSON strings such as quotes, backslashes, and control characters.
 * Updates the buffer pointer and remaining size accordingly.
 *
 * @param buffer Pointer to the buffer pointer which will be updated
 * @param remaining Pointer to the remaining size which will be updated
 * @param str The string to escape and append
 * @return size_t Number of characters written
 */
static size_t append_json_escaped(char** buffer, size_t* remaining, const char* str) {
    if (!buffer || !*buffer || !remaining || !str || *remaining == 0)
        return 0;
    
    size_t written = 0;
    char* dst = *buffer;
    
    while (*str && *remaining > 1) {
        char c = *str++;
        
        // Escape special characters
        if (c == '\"' || c == '\\') {
            if (*remaining < 3) break; // Need space for escape sequence + null
            
            *dst++ = '\\';
            *dst++ = c;
            written += 2;
            *remaining -= 2;
        }
        // Escape control characters
        else if (c < 32) {
            const char* escape = NULL; // Initialize to NULL to prevent uninitialized use
            switch (c) {
                case '\b': escape = "\\b"; break;
                case '\f': escape = "\\f"; break;
                case '\n': escape = "\\n"; break;
                case '\r': escape = "\\r"; break;
                case '\t': escape = "\\t"; break;
                default: {
                    // For other control chars, use \uXXXX format
                    if (*remaining < 7) break; // Need space for \uXXXX + null
                    *dst++ = '\\';
                    *dst++ = 'u';
                    *dst++ = '0';
                    *dst++ = '0';
                    *dst++ = "0123456789ABCDEF"[(c >> 4) & 0xF];
                    *dst++ = "0123456789ABCDEF"[c & 0xF];
                    written += 6;
                    *remaining -= 6;
                    continue;
                }
            }
            
            // Only use escape if it was set
            if (escape != NULL && *remaining >= 3) {
                *dst++ = escape[0];
                *dst++ = escape[1];
                written += 2;
                *remaining -= 2;
            }
        }
        // Normal character
        else {
            *dst++ = c;
            written += 1;
            *remaining -= 1;
        }
    }
    
    *dst = '\0';
    *buffer = dst;
    return written;
}

/**
 * @brief Formats all errors in the context as a JSON string.
 *
 * Writes a JSON object containing a summary of error and warning counts and an array of detailed
 * error objects to the provided buffer. Each error includes severity, source, line, column, and
 * message fields. The output is truncated if the buffer is too small and is always null-terminated.
 *
 * @param ctx Pointer to the error context containing errors to format.
 * @param buffer Destination buffer for the JSON output.
 * @param size Size of the destination buffer in bytes.
 * @return size_t Number of characters written to the buffer, excluding the null terminator. Returns
 * 0 if inputs are invalid or buffer size is zero.
 */
size_t format_errors_json(const ErrorContext* ctx, char* buffer, size_t size) {
    if (!ctx || !buffer || size == 0)
        return 0;

    size_t written   = 0;
    size_t remaining = size - 1;  // Reserve space for null terminator

    // Start JSON array
    int chars =
        snprintf(buffer, remaining, "{\"summary\":{\"errors\":%d,\"warnings\":%d},\"details\":[",
                 ctx->error_count, ctx->warning_count);

    if (chars < 0)
        return 0;
    if ((size_t)chars >= remaining) {
        buffer[remaining] = '\0';
        return size - 1;
    }

    written += (size_t)chars;
    buffer += chars;
    remaining -= (size_t)chars;

    // Format each error as JSON
    ErrorReport* current = ctx->first_error;
    bool         first   = true;

    while (current && remaining > 0) {
        // Add comma between items
        if (!first) {
            if (remaining < 1)
                break;
            *buffer++ = ',';
            written++;
            remaining--;
        }
        first = false;

        chars = snprintf(
            buffer, remaining,
            "{\"severity\":\"%s\",\"source\":\"%s\",\"line\":%d,\"column\":%d,\"message\":\"",
            get_severity_name(current->severity), get_source_name(current->source), current->line,
            current->column);

        if (chars < 0)
            break;
        if ((size_t)chars >= remaining) {
            buffer[remaining] = '\0';
            written += remaining;
            break;
        }

        written += (size_t)chars;
        buffer += chars;
        remaining -= (size_t)chars;

        // Escape the message
        size_t escape_written = append_json_escaped(&buffer, &remaining, current->message);
        written += escape_written;

        // Add closing quote and brace for this error object
        if (remaining >= 2) {
            *buffer++ = '"';
            *buffer++ = '}';
            written += 2;
            remaining -= 2;
        }

        current = current->next;
    }

    // Close JSON array
    if (remaining >= 3) {
        memcpy(buffer, "]}", 2);
        buffer[2] = '\0';
        written += 2;
    } else if (remaining > 0) {
        buffer[0] = '\0';
    }

    return written;
}