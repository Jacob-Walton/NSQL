#pragma once

#include <nsql/ast_serializer.h>
#include <nsql/lexer.h>
#include <nsql/parser.h>
#include <stdbool.h>

/**
 * Initialize the NSQL query processor.
 */
bool nsql_processor_init(void);

/**
 * Process an NSQL query
 *
 * @param query The NSQL query string.
 * @return true if successful, false otherwise.
 */
bool nsql_process_query(const char* query);

/**
 * Shut down the NSQL processor.
 */
void nsql_processor_shutdown(void);