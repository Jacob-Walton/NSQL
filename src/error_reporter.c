/**
 * @file error_reporter.c
 * @brief Implementation of the error reporting system
 */

#include <nsql/error_reporter.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void error_context_init(ErrorContext* ctx) {
    if (!ctx) return;
    ctx->first_error = NULL;
    ctx->last_error = NULL;
    ctx->error_count = 0;
    ctx->warning_count = 0;
    ctx->has_error = false;
    ctx->has_fatal = false;
}

void error_context_free(ErrorContext* ctx) {
    if (!ctx) return;
    
    ErrorReport* current = ctx->first_error;
    while (current != NULL) {
        ErrorReport* next = current->next;
        free((void*)current->message); // Free the duplicated message
        free(current);
        current = next;
    }
    ctx->first_error = NULL;
    ctx->last_error = NULL;
    ctx->error_count = 0;
    ctx->warning_count = 0;
    ctx->has_error = false;
    ctx->has_fatal = false;
}

bool report_error(ErrorContext* ctx, ErrorSeverity severity, ErrorSource source,
                 int line, int column, const char* message) {
    if (!ctx || !message) return false;
    
    // Create new error report
    ErrorReport* report = (ErrorReport*)malloc(sizeof(ErrorReport));
    if (!report) return false;

    // Make a copy of the message to ensure it remains valid
    char* message_copy = strdup(message);
    if (!message_copy) {
        free(report);
        return false;
    }

    // Initialize report
    report->severity = severity;
    report->source = source;
    report->line = line;
    report->column = column;
    report->message = message_copy;
    report->next = NULL;

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

// Helper function to get error source name
static const char* get_source_name(ErrorSource source) {
    switch (source) {
        case ERROR_SOURCE_LEXER:    return "Lexer";
        case ERROR_SOURCE_PARSER:   return "Parser";
        case ERROR_SOURCE_SEMANTIC: return "Semantic";
        case ERROR_SOURCE_RUNTIME:  return "Runtime";
        case ERROR_SOURCE_SYSTEM:   return "System";
        default:                    return "Unknown";
    }
}

// Helper function to get severity name
static const char* get_severity_name(ErrorSeverity severity) {
    switch (severity) {
        case ERROR_NONE:    return "Info";
        case ERROR_WARNING: return "Warning";
        case ERROR_ERROR:   return "Error";
        case ERROR_FATAL:   return "Fatal";
        default:            return "Unknown";
    }
}

size_t format_errors(const ErrorContext* ctx, char* buffer, size_t size) {
    if (!ctx || !buffer || size == 0) return 0;
    
    size_t written = 0;
    size_t remaining = size - 1; // Reserve space for null terminator
    
    // Write summary header
    int chars = snprintf(buffer, remaining, "NSQL Parsing Results: %d error(s), %d warning(s)\n\n",
                        ctx->error_count, ctx->warning_count);
    
    if (chars < 0) return 0;
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
        const char* source = get_source_name(current->source);
        
        chars = snprintf(buffer, remaining, 
                         "[%s] %s (line %d, col %d): %s\n",
                         severity, source, current->line, current->column, 
                         current->message);
        
        if (chars < 0) break;
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

size_t format_errors_json(const ErrorContext* ctx, char* buffer, size_t size) {
    if (!ctx || !buffer || size == 0) return 0;
    
    size_t written = 0;
    size_t remaining = size - 1; // Reserve space for null terminator
    
    // Start JSON array
    int chars = snprintf(buffer, remaining, 
                        "{\"summary\":{\"errors\":%d,\"warnings\":%d},\"details\":[",
                        ctx->error_count, ctx->warning_count);
    
    if (chars < 0) return 0;
    if ((size_t)chars >= remaining) {
        buffer[remaining] = '\0';
        return size - 1;
    }
    
    written += (size_t)chars;
    buffer += chars;
    remaining -= (size_t)chars;
    
    // Format each error as JSON
    ErrorReport* current = ctx->first_error;
    bool first = true;
    
    while (current && remaining > 0) {
        // Add comma between items
        if (!first) {
            if (remaining < 1) break;
            *buffer++ = ',';
            written++;
            remaining--;
        }
        first = false;
        
        // Convert message to JSON-safe string
        // This is a simplified version - a real implementation would need to properly escape
        // special characters like quotes, newlines, etc.
        
        chars = snprintf(buffer, remaining, 
                         "{\"severity\":\"%s\",\"source\":\"%s\",\"line\":%d,\"column\":%d,\"message\":\"%s\"}",
                         get_severity_name(current->severity),
                         get_source_name(current->source),
                         current->line, current->column,
                         current->message);
        
        if (chars < 0) break;
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
    
    // Close JSON array
    if (remaining >= 2) {
        strcpy(buffer, "]}");
        written += 2;
    }
    
    // Ensure null termination
    if (remaining > 0) {
        buffer[0] = '\0';
    }
    
    return written;
}