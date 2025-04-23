#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "parser.h"  // For Node type def

// Format Constants
#define AST_HEADER_SIZE 28
#define AST_MAGIC_NUMBER 0x4E52514C  // "NSQL"
#define AST_VERSION 0x0001

// Engine types
#define ENGINE_AUTO 0x00   // Auto-select engine
#define ENGINE_SQL 0x01    // Use SQL engine
#define ENGINE_NOSQL 0x02  // Use NoSQL engine

// Execution hint flags
#define HINT_PARALLEL_EXEC 0x0001  // Query can be parallelized
#define HINT_INDEX_SCAN 0x0002     // Use an index scan
#define HINT_FULL_SCAN 0x0003      // Use full table scan
#define HINT_CACHE_RESULT 0x0004   // Cache query result
#define HINT_PRIORITY_HIGH 0x0010  // High priority execution
#define HINT_PRIORITY_LOW 0x0020   // Low priority execution
#define HINT_READ_ONLY 0x0040      // Read-only query

// Execution metadata
typedef struct {
    uint16_t    hint_flags;      // Execution hint flags
    uint8_t     priority;        // 0-255 priority level (higher = more priority)
    uint8_t     engine_type;     // Storage engine selection
    uint32_t    estimated_rows;  // Estimated result row count
    uint32_t    timeout_ms;      // Query timeout in milliseconds
    const char* target_index;    // Index to use (if applicable)
} ExecutionMetadata;

// AST handle
typedef struct SerializedAST SerializedAST;

// =======================================================
// Core Functions
// =======================================================

/**
 * Serialize an AST tree with accompanying execution metadata
 *
 * @param node Root node of the AST
 * @param metadata Execution metadata (NULL for default)
 * @return SerializedAST handle or NULL on failure
 */
SerializedAST* ast_serialize(Node* node, const ExecutionMetadata* metadata);

/**
 * Free a serialized AST
 *
 * @param ast The serialized AST to free
 */
void ast_free(SerializedAST* ast);

/**
 * Get raw binary data from serialized AST
 *
 * @param ast The serialized AST
 * @param size Pointer to store the size of the data
 * @return Pointer to binary data (do not free separately)
 */
const void* ast_get_data(const SerializedAST* ast, size_t* size);

/**
 * Deserialize AST from binary data
 *
 * @param data Binary serialized AST data
 * @param size Size of the data
 * @return SerializedData handle or NULL if invalid
 */
SerializedAST* ast_deserialize(const void* data, size_t size);

/**
 * Verify the checksum of a serialized AST
 *
 * @param ast The serialized AST
 * @return true if checksum is valid, false otherwise
 */
bool ast_verify_checksum(const SerializedAST* ast);

// =======================================================
// Metadata Functions
// =======================================================

/**
 * Create default execution metadata for a query
 *
 * @param node Root node of the AST
 * @return ExecutionMetadata with optimal settings
 */
ExecutionMetadata ast_create_metadata(const Node* node);

/**
 * Extract execution metadata from serialized AST
 *
 * @param ast The serialized AST
 * @param metadata Pointer to store extracted metadata
 * @return true if successful, false otherwise
 */
bool ast_extract_metadata(const SerializedAST* ast, ExecutionMetadata* metadata);

/**
 * Determine if a query should use NoSQL storage engine
 *
 * @param node The AST node
 * @return true if query should use NoSQL, false if SQL
 */
bool ast_is_nosql_query(const Node* node);