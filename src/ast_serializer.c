#include <nsql/ast_serializer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Format Constants
#define AST_HEADER_SIZE 28

// Serialized AST structure
struct SerializedAST {
    void*    data;      // Raw binary data
    size_t   size;      // Total size in bytes
    uint32_t checksum;  // CRC32 checksum
    bool     is_valid;  // Validation state
};

// Serialization buffer
typedef struct {
    char*  buffer;
    size_t capacity;
    size_t size;
} SerializeBuffer;

/**
 * Initialize serialization buffer.
 *
 * @param initial_capacity The initial capacity of the buffer.
 * @return Pointer to the initialized buffer, or NULL on failure.
 * @note The buffer must be freed using free_buffer() after use.
 */
static SerializeBuffer* init_buffer(size_t initial_capacity) {
    SerializeBuffer* buf = (SerializeBuffer*)malloc(sizeof(SerializeBuffer));
    if (!buf)
        return NULL;

    buf->buffer = (char*)malloc(initial_capacity);
    if (!buf->buffer) {
        free(buf);
        return NULL;
    }

    buf->capacity = initial_capacity;
    buf->size     = 0;
    return buf;
}

/**
 * Free serialization buffer.
 *
 * @param buf The buffer to free.
 * @note This function frees the buffer and its internal data.
 */
static void free_buffer(SerializeBuffer* buf) {
    if (buf) {
        free(buf->buffer);
        free(buf);
    }
}

/**
 * Ensure buffer has enough space for additional data.
 *
 * @param buf The buffer to check.
 * @param additional The additional size needed.
 * @return true if successful, false on failure.
 */
static bool ensure_capacity(SerializeBuffer* buf, size_t additional) {
    if (buf->size + additional > buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        if (new_capacity < buf->size + additional)
            new_capacity = buf->size + additional + 1024;  // Extra padding

        char* new_buffer = (char*)realloc(buf->buffer, new_capacity);
        if (!new_buffer)
            return false;

        buf->buffer   = new_buffer;
        buf->capacity = new_capacity;
    }
    return true;
}

/**
 * Write bytes to buffer.
 *
 * @param buf The buffer to write to.
 * @param data The data to write.
 * @param size The size of the data in bytes.
 * @return true if successful, false on failure.
 */
static bool write_bytes(SerializeBuffer* buf, const void* data, size_t size) {
    if (!ensure_capacity(buf, size))
        return false;
    memcpy(buf->buffer + buf->size, data, size);
    buf->size += size;
    return true;
}

/**
 * Write an 8-bit unsigned integer to the buffer.
 *
 * @param buf The buffer to write to.
 * @param value The value to write.
 * @return true if successful, false on failure.
 */
static bool write_uint8(SerializeBuffer* buf, uint8_t value) {
    return write_bytes(buf, &value, sizeof(uint8_t));
}

/**
 * Write a 16-bit unsigned integer to the buffer.
 *
 * @param buf The buffer to write to.
 * @param value The value to write.
 * @return true if successful, false on failure.
 */
static bool write_uint16(SerializeBuffer* buf, uint16_t value) {
    return write_bytes(buf, &value, sizeof(uint16_t));
}

/**
 * Write a 32-bit unsigned integer to the buffer.
 *
 * @param buf The buffer to write to.
 * @param value The value to write.
 * @return true if successful, false on failure.
 */
static bool write_uint32(SerializeBuffer* buf, uint32_t value) {
    return write_bytes(buf, &value, sizeof(uint32_t));
}

/**
 * Write a 32-bit signed integer to the buffer.
 *
 * @param buf The buffer to write to.
 * @param value The value to write.
 * @return true if successful, false on failure.
 */
static bool write_int32(SerializeBuffer* buf, int32_t value) {
    return write_bytes(buf, &value, sizeof(int32_t));
}

/**
 * Write a double to the buffer.
 *
 * @param buf The buffer to write to.
 * @param value The value to write.
 * @return true if successful, false on failure.
 */
static bool write_double(SerializeBuffer* buf, double value) {
    return write_bytes(buf, &value, sizeof(double));
}

/**
 * Writes a string to the buffer, prefixed by its length as a 16-bit unsigned integer.
 *
 * If the string is NULL, writes a zero-length marker. Strings longer than 65535 bytes are truncated.
 *
 * @return true if the string is written successfully, false on failure.
 */
static bool write_string(SerializeBuffer* buf, const char* str) {
    if (!str) {
        // Write a null string marker
        return write_uint16(buf, 0);
    }

    size_t len = strlen(str);
    // Check for truncation
    if (len > UINT16_MAX) {
        fprintf(stderr, "Warning: String too long, truncating to %u bytes\n", UINT16_MAX);
        len = UINT16_MAX;
    }
    if (!write_uint16(buf, (uint16_t)len))
        return false;
    if (len > 0) {
        return write_bytes(buf, str, len);
    }
    return true;
}

// CRC32 table
static uint32_t crc32_table[256];
static bool     crc32_table_computed = false;

/**
 * Initialize CRC32 table.
 *
 * @note This function is called automatically when needed.
 */
static void make_crc32_table(void) {
    uint32_t c;
    int      n, k;

    for (n = 0; n < 256; n++) {
        c = (uint32_t)n;
        for (k = 0; k < 8; k++) {
            if (c & 1)
                c = 0xEDB88320U ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc32_table[n] = c;
    }
    crc32_table_computed = true;
}

/**
 * Calculate CRC32 checksum for the given data.
 *
 * @param data The data to calculate the checksum for.
 * @param length The length of the data in bytes.
 * @return The calculated checksum.
 */
static uint32_t calculate_crc32(const void* data, size_t length) {
    if (!crc32_table_computed)
        make_crc32_table();

    const unsigned char* buf = (const unsigned char*)data;
    uint32_t             c   = 0xFFFFFFFFU;

    for (size_t n = 0; n < length; n++) {
        c = crc32_table[(c ^ buf[n]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFU;
}

// Forward declaration for serialization (recursive)
static bool serialize_node(SerializeBuffer* buf, const Node* node);

/**
 * Serialize a field list node.
 *
 * @param buf The buffer to write to.
 * @param node The field list node to serialize.
 * @return true if successful, false on failure.
 */
static bool serialize_field_list(SerializeBuffer* buf, const Node* node) {
    if (!write_uint16(buf, node->as.field_list.count))
        return false;

    for (int i = 0; i < node->as.field_list.count; i++) {
        if (!serialize_node(buf, node->as.field_list.fields[i]))
            return false;
    }

    return true;
}

/**
 * Serialize a source node.
 *
 * @param buf The buffer to write to.
 * @param node The source node to serialize.
 * @return true if successful, false on failure.
 */
static bool serialize_source(SerializeBuffer* buf, const Node* node) {
    if (!write_string(buf, node->as.source.identifier->as.identifier.name))
        return false;

    // Serialize join or NULL
    if (node->as.source.join) {
        if (!write_uint8(buf, 1))  // Has join
            return false;
        if (!serialize_node(buf, node->as.source.join))
            return false;
    } else {
        if (!write_uint8(buf, 0))  // No join
            return false;
    }

    return true;
}

/**
 * Serialize a binary expression node.
 *
 * @param buf The buffer to write to.
 * @param node The binary expression node to serialize.
 * @return true if successful, false on failure.
 */
static bool serialize_binary_expr(SerializeBuffer* buf, const Node* node) {
    if (!write_uint8(buf, node->as.binary_expr.op))
        return false;
    if (!serialize_node(buf, node->as.binary_expr.left))
        return false;
    if (!serialize_node(buf, node->as.binary_expr.right))
        return false;
    return true;
}

/**
 * Serialize an identifier node.
 *
 * @param buf The buffer to write to.
 * @param node The identifier node to serialize.
 * @return true if successful, false on failure.
 */
static bool serialize_identifier(SerializeBuffer* buf, const Node* node) {
    return write_string(buf, node->as.identifier.name);
}

/**
 * Serialize a literal node.
 *
 * @param buf The buffer to write to.
 * @param node The literal node to serialize.
 * @return true if successful, false on failure.
 */
static bool serialize_literal(SerializeBuffer* buf, const Node* node) {
    if (!write_uint8(buf, node->as.literal.literal_type))
        return false;

    switch (node->as.literal.literal_type) {
        case TOKEN_STRING:
            return write_string(buf, node->as.literal.value.string_value);
        case TOKEN_INTEGER:
        case TOKEN_DECIMAL:
            return write_double(buf, node->as.literal.value.number_value);
        default:
            return false;
    }
}

/**
 * Main node serialization logic.
 *
 * @param buf The buffer to write to.
 * @param node The function call node to serialize.
 * @return true if successful, false on failure.
 */
static bool serialize_node(SerializeBuffer* buf, const Node* node) {
    if (!node) {
        // Write a null node marker
        return write_uint8(buf, 0xFF);
    }

    // Write node type
    if (!write_uint8(buf, node->type))
        return false;

    // Write line number for debugging
    if (!write_uint32(buf, node->line))
        return false;

    // Serialize node-specific data
    switch (node->type) {
        case NODE_ASK_QUERY:
            if (!serialize_node(buf, node->as.ask_query.source))
                return false;
            if (!serialize_node(buf, node->as.ask_query.fields))
                return false;
            if (!serialize_node(buf, node->as.ask_query.condition))
                return false;
            if (!serialize_node(buf, node->as.ask_query.group_by))
                return false;
            if (!serialize_node(buf, node->as.ask_query.order_by))
                return false;
            if (!serialize_node(buf, node->as.ask_query.limit))
                return false;
            break;

        case NODE_TELL_QUERY:
            if (!serialize_node(buf, node->as.tell_query.source))
                return false;
            if (!serialize_node(buf, node->as.tell_query.action))
                return false;
            if (!serialize_node(buf, node->as.tell_query.condition))
                return false;
            break;

        case NODE_FIND_QUERY:
            if (!serialize_node(buf, node->as.find_query.source))
                return false;
            if (!serialize_node(buf, node->as.find_query.condition))
                return false;
            if (!serialize_node(buf, node->as.find_query.group_by))
                return false;
            if (!serialize_node(buf, node->as.find_query.order_by))
                return false;
            if (!serialize_node(buf, node->as.find_query.limit))
                return false;
            break;

        case NODE_SHOW_QUERY:
        case NODE_GET_QUERY:
            if (!serialize_node(buf, node->as.show_query.source))
                return false;
            if (!serialize_node(buf, node->as.show_query.fields))
                return false;
            if (!serialize_node(buf, node->as.show_query.condition))
                return false;
            if (!serialize_node(buf, node->as.show_query.group_by))
                return false;
            if (!serialize_node(buf, node->as.show_query.order_by))
                return false;
            if (!serialize_node(buf, node->as.show_query.limit))
                return false;
            break;

        case NODE_FIELD_LIST:
            if (!serialize_field_list(buf, node))
                return false;
            break;

        case NODE_SOURCE:
            if (!serialize_source(buf, node))
                return false;
            break;

        case NODE_JOIN:
            if (!serialize_node(buf, node->as.join.source))
                return false;
            if (!serialize_node(buf, node->as.join.condition))
                return false;
            break;

        case NODE_GROUP_BY:
            if (!serialize_node(buf, node->as.group_by.fields))
                return false;
            if (!serialize_node(buf, node->as.group_by.having))
                return false;
            break;

        case NODE_ORDER_BY:
            if (!write_uint16(buf, node->as.order_by.count))
                return false;

            for (int i = 0; i < node->as.order_by.count; i++) {
                if (!serialize_node(buf, node->as.order_by.fields[i]))
                    return false;
                if (!write_uint8(buf, node->as.order_by.ascending[i]))
                    return false;
            }
            break;

        case NODE_LIMIT:
            if (!write_int32(buf, node->as.limit.limit))
                return false;
            if (!write_int32(buf, node->as.limit.offset))
                return false;
            break;

        case NODE_BINARY_EXPR:
            if (!serialize_binary_expr(buf, node))
                return false;
            break;

        case NODE_UNARY_EXPR:
            if (!write_uint8(buf, node->as.unary_expr.op))
                return false;
            if (!serialize_node(buf, node->as.unary_expr.operand))
                return false;
            break;

        case NODE_IDENTIFIER:
            if (!serialize_identifier(buf, node))
                return false;
            break;

        case NODE_LITERAL:
            if (!serialize_literal(buf, node))
                return false;
            break;

        case NODE_ADD_ACTION:
            if (!serialize_node(buf, node->as.add_action.value))
                return false;
            if (!serialize_node(buf, node->as.add_action.record_spec))
                return false;
            break;

        case NODE_REMOVE_ACTION:
            if (!serialize_node(buf, node->as.remove_action.condition))
                return false;
            break;

        case NODE_UPDATE_ACTION:
            if (!write_uint16(buf, node->as.update_action.count))
                return false;
            for (int i = 0; i < node->as.update_action.count; i++) {
                if (!serialize_node(buf, node->as.update_action.fields[i]))
                    return false;
                if (!serialize_node(buf, node->as.update_action.values[i]))
                    return false;
            }
            break;

        case NODE_CREATE_ACTION:
            if (!write_uint16(buf, node->as.create_action.count))
                return false;
            for (int i = 0; i < node->as.create_action.count; i++) {
                if (!serialize_node(buf, node->as.create_action.field_defs[i]))
                    return false;
            }
            break;

        case NODE_FIELD_DEF:
            if (!serialize_node(buf, node->as.field_def.name))
                return false;
            if (!write_string(buf, node->as.field_def.type))
                return false;
            if (!write_uint16(buf, node->as.field_def.constraint_count))
                return false;
            for (int i = 0; i < node->as.field_def.constraint_count; i++) {
                if (!serialize_node(buf, node->as.field_def.constraints[i]))
                    return false;
            }
            break;

        case NODE_CONSTRAINT:
            if (!write_uint8(buf, node->as.constraint.type))
                return false;
            if (!serialize_node(buf, node->as.constraint.default_value))
                return false;
            break;

        case NODE_FUNCTION_CALL:
            if (!write_string(buf, node->as.function_call.name))
                return false;
            if (!write_uint16(buf, node->as.function_call.arg_count))
                return false;
            for (int i = 0; i < node->as.function_call.arg_count; i++) {
                if (!serialize_node(buf, node->as.function_call.args[i]))
                    return false;
            }
            break;

        case NODE_ERROR:
            if (!write_string(buf, node->as.error.message))
                return false;
            break;

        default:
            return false;  // Unsupported node type
    }

    return true;
}

/**
 * Serialize execution metadata.
 *
 * @param buf The buffer to write to.
 * @param metadata The metadata to serialize.
 * @return true if successful, false on failure.
 */
static bool serialize_metadata(SerializeBuffer* buf, const ExecutionMetadata* metadata) {
    if (!metadata) {
        // No metadata, write zeros
        if (!write_uint16(buf, 0))
            return false;  // hints
        if (!write_uint8(buf, 128))
            return false;  // priority (default 128)
        if (!write_uint8(buf, ENGINE_AUTO))
            return false;  // engine
        if (!write_uint32(buf, 0))
            return false;  // rows
        if (!write_uint32(buf, 30000))
            return false;  // timeout (30s default)
        if (!write_string(buf, NULL))
            return false;  // index
        return true;
    }

    if (!write_uint16(buf, metadata->hint_flags))
        return false;
    if (!write_uint8(buf, metadata->priority))
        return false;
    if (!write_uint8(buf, metadata->engine_type))
        return false;
    if (!write_uint32(buf, metadata->estimated_rows))
        return false;
    if (!write_uint32(buf, metadata->timeout_ms))
        return false;
    if (!write_string(buf, metadata->target_index))
        return false;

    return true;
}

ExecutionMetadata ast_create_metadata(const Node* node) {
    // TODO: Revisit this logic
    ExecutionMetadata metadata;
    metadata.hint_flags     = 0;
    metadata.priority       = 128;
    metadata.engine_type    = ENGINE_AUTO;
    metadata.estimated_rows = 0;
    metadata.timeout_ms     = 30000;
    metadata.target_index   = NULL;

    if (!node)
        return metadata;

    if (ast_is_nosql_query(node)) {
        // NoSQL: favor concurrency, parallelism, and lightness
        metadata.engine_type    = ENGINE_NOSQL;
        metadata.hint_flags    |= HINT_PARALLEL_EXEC | HINT_READ_ONLY;
        metadata.priority       = 128; // Balanced
        metadata.timeout_ms     = 10000; // Faster timeout for NoSQL
        // FIND queries: expect many rows, so set estimated_rows high
        if (node->type == NODE_FIND_QUERY) {
            metadata.estimated_rows = 10000;
            metadata.hint_flags |= HINT_FULL_SCAN;
        }
        // SHOW/GET: reporting, so cache results
        if (node->type == NODE_SHOW_QUERY || node->type == NODE_GET_QUERY) {
            metadata.estimated_rows = 1000;
            metadata.hint_flags |= HINT_CACHE_RESULT;
            metadata.priority = 96;
        }
    } else {
        // SQL: favor consistency, indexing, and correctness
        metadata.engine_type = ENGINE_SQL;
        switch (node->type) {
            case NODE_ASK_QUERY:
                metadata.hint_flags |= HINT_READ_ONLY;
                metadata.priority = 128;
                // If condition exists, prefer index scan
                if (node->as.ask_query.condition != NULL) {
                    metadata.hint_flags |= HINT_INDEX_SCAN;
                    metadata.estimated_rows = 100;
                } else {
                    metadata.hint_flags |= HINT_FULL_SCAN;
                    metadata.estimated_rows = 1000;
                }
                // If limit exists, cache result
                if (node->as.ask_query.limit != NULL) {
                    metadata.hint_flags |= HINT_CACHE_RESULT;
                }
                break;
            case NODE_TELL_QUERY:
                metadata.priority = 192; // Higher priority for writes
                metadata.hint_flags = 0; // No read-only
                metadata.estimated_rows = 1;
                break;
            default:
                break;
        }
    }

    return metadata;
}

/**
 * Serializes an AST and its execution metadata into a binary format with integrity verification.
 *
 * Serializes the provided AST node and optional execution metadata into a contiguous binary buffer,
 * prepends a fixed-size header containing metadata and a CRC32 checksum, and returns a handle to the
 * resulting SerializedAST structure. Returns NULL if serialization fails at any stage.
 *
 * @param node Root node of the AST to serialize.
 * @param metadata Optional execution metadata; if NULL, default values are used.
 * @return Pointer to a SerializedAST structure containing the serialized data, or NULL on failure.
 */
SerializedAST* ast_serialize(Node* node, const ExecutionMetadata* metadata) {
    if (!node)
        return NULL;

    // Initialize serialized AST
    SerializedAST* ast = (SerializedAST*)malloc(sizeof(SerializedAST));
    if (!ast)
        return NULL;

    ast->data     = NULL;
    ast->size     = 0;
    ast->checksum = 0;
    ast->is_valid = false;

    // Initialize buffer for node serialization
    SerializeBuffer* data_buf = init_buffer(4096);
    if (!data_buf) {
        free(ast);
        return NULL;
    }

    // Serialize the AST
    if (!serialize_node(data_buf, node)) {
        free_buffer(data_buf);
        free(ast);
        return NULL;
    }

    // Serialize metadata
    if (!serialize_metadata(data_buf, metadata)) {
        free_buffer(data_buf);
        free(ast);
        return NULL;
    }

    // Calculate checksum
    uint32_t checksum = calculate_crc32(data_buf->buffer, data_buf->size);

    // Create buffer for final output with header
    SerializeBuffer* final_buf = init_buffer(AST_HEADER_SIZE + data_buf->size);
    if (!final_buf) {
        free_buffer(data_buf);
        free(ast);
        return NULL;
    }

    // Write header
    if (!write_uint32(final_buf, AST_MAGIC_NUMBER))  // Magic number
        return NULL;
    if (!write_uint32(final_buf, AST_VERSION))       // Version
        return NULL;
    if (!write_uint32(final_buf, 0))                 // Reserved
        return NULL;
    if (!write_uint32(final_buf, data_buf->size))    // Data size
        return NULL;
    // Either use original_size or remove it
    if (!write_uint32(final_buf, data_buf->size))    // Original size (same, no compression)
        return NULL;
    if (!write_uint32(final_buf, checksum))          // Checksum
        return NULL;
    if (!write_uint32(final_buf, 0))                 // Reserved
        return NULL;

    // Write data
    write_bytes(final_buf, data_buf->buffer, data_buf->size);

    // Set serialized AST fields
    ast->data     = final_buf->buffer;
    ast->size     = final_buf->size;
    ast->checksum = checksum;
    ast->is_valid = true;  // Mark as valid

    // Free temporary buffers
    free(data_buf->buffer);
    free(data_buf);
    free(final_buf);  // Only free the struct, not buffer which is now owned by ast

    return ast;
}

/**
 * Free serialized AST.
 *
 * @param ast The serialized AST to free.
 */
void ast_free(SerializedAST* ast) {
    if (ast) {
        free(ast->data);
        free(ast);
    }
}

/**
 * Get raw binary data from serialized AST.
 *
 * @param ast The serialized AST.
 * @param size Pointer to store the size of the data.
 *
 * @return Pointer to binary data (do not free separately).
 * @note The caller is responsible for freeing the serialized AST using ast_free().
 */
const void* ast_get_data(const SerializedAST* ast, size_t* size) {
    if (!ast || !ast->is_valid) {
        if (size)
            *size = 0;
        return NULL;
    }

    if (size)
        *size = ast->size;

    return ast->data;
}

/**
 * Deserialize AST from binary data.
 *
 * @param data Binary serialized AST data.
 * @param size Size of the data.
 * @return SerializedAST handle or NULL if invalid.
 */
SerializedAST* ast_deserialize(const void* data, size_t size) {
    if (!data || size < AST_HEADER_SIZE)
        return NULL;

    const char* char_data = (const char*)data;

    // Parse header (all fields are uint32_t, 4 bytes each)
    uint32_t magic           = *((uint32_t*)(char_data + 0));
    uint32_t version         = *((uint32_t*)(char_data + 4));
    // uint32_t reserved1    = *((uint32_t*)(char_data + 8));
    uint32_t data_size       = *((uint32_t*)(char_data + 12));
    uint32_t original_size   = *((uint32_t*)(char_data + 16));
    uint32_t stored_checksum = *((uint32_t*)(char_data + 20));
    // uint32_t reserved2    = *((uint32_t*)(char_data + 24));

    if (magic != AST_MAGIC_NUMBER) {
        return NULL;  // Invalid magic number
    }

    if (version > AST_VERSION) {
        return NULL;  // Incompatible version
    }

    // Check if data size matches
    if (size != AST_HEADER_SIZE + data_size) {
        return NULL;  // Size mismatch
    }

    // Get data pointer
    const char* ast_data = char_data + AST_HEADER_SIZE;

    // Compute checksum
    uint32_t computed_checksum = calculate_crc32(ast_data, data_size);

    // Create serialized AST
    SerializedAST* ast = (SerializedAST*)malloc(sizeof(SerializedAST));
    if (!ast)
        return NULL;

    // Make a copy of the data
    ast->data = malloc(size);
    if (!ast->data) {
        free(ast);
        return NULL;
    }

    memcpy(ast->data, data, size);
    ast->size     = size;
    ast->checksum = computed_checksum;
    ast->is_valid = (computed_checksum == stored_checksum);

    return ast;
}

bool ast_verify_checksum(const SerializedAST* ast) {
    if (!ast || !ast->data || ast->size < AST_HEADER_SIZE)
        return false;

    const char* char_data       = (const char*)ast->data;
    uint32_t    stored_checksum = *((uint32_t*)(char_data + 20));
    const char* ast_data        = char_data + AST_HEADER_SIZE;
    uint32_t    data_size       = *((uint32_t*)(char_data + 12));

    uint32_t computed_checksum = calculate_crc32(ast_data, data_size);

    return computed_checksum == stored_checksum;
}

bool ast_extract_metadata(const SerializedAST* ast, ExecutionMetadata* metadata) {
    if (!ast || !metadata || !ast->is_valid) {
        return false;  // Invalid AST or metadata pointer
    }

    const char* char_data = (const char*)ast->data;
    const char* ast_data  = char_data + AST_HEADER_SIZE;
    uint32_t data_size    = *((uint32_t*)(char_data + 12));

    // Find the start of metadata (at the end of the AST data)
    if (data_size < 8) {
        return false;
    }
    size_t offset = data_size;

    // Find string length
    size_t pos = offset;
    // Step back to read string length
    pos -= 2;
    uint16_t str_len = *((uint16_t*)(ast_data + pos));
    // Step back to start of string
    pos -= str_len;
    // Allocate and copy string (may be NULL)
    char* target_index = NULL;
    if (str_len > 0) {
        target_index = (char*)malloc(str_len + 1);
        if (!target_index) return false;
        memcpy(target_index, ast_data + pos, str_len);
        target_index[str_len] = '\0';
    }

    // Step back for timeout_ms (uint32_t)
    pos -= 4;
    uint32_t timeout_ms = *((uint32_t*)(ast_data + pos));
    // Step back for estimated_rows (uint32_t)
    pos -= 4;
    uint32_t estimated_rows = *((uint32_t*)(ast_data + pos));
    // Step back for engine_type (uint8_t)
    pos -= 1;
    uint8_t engine_type = *((uint8_t*)(ast_data + pos));
    // Step back for priority (uint8_t)
    pos -= 1;
    uint8_t priority = *((uint8_t*)(ast_data + pos));
    // Step back for hint_flags (uint16_t)
    pos -= 2;
    uint16_t hint_flags = *((uint16_t*)(ast_data + pos));

    // Fill metadata struct
    metadata->hint_flags     = hint_flags;
    metadata->priority       = priority;
    metadata->engine_type    = engine_type;
    metadata->estimated_rows = estimated_rows;
    metadata->timeout_ms     = timeout_ms;
    metadata->target_index   = target_index;

    return true;
}

/**
 * Determine if a query should use NoSQL storage engine.
 *
 * @param node The AST node.
 * @return true if query should use NoSQL, false if SQL.
 */
bool ast_is_nosql_query(const Node* node) {
    // TODO: Revisit this logic
    if (!node)
        return false;

    switch (node->type) {
        case NODE_FIND_QUERY:
            // FIND queries are always NoSQL
            return true;
        case NODE_SHOW_QUERY:
        case NODE_GET_QUERY:
            // SHOW/GET queries are NoSQL if they use NoSQL types or sources
            // For now, treat as NoSQL if the source or fields are not SQL-like
            return true;
        case NODE_TELL_QUERY:
            // TELL is SQL unless the action is ADD/REMOVE/UPDATE on a NoSQL table
            // For now, treat as SQL
            return false;
        case NODE_ASK_QUERY:
            // ASK is SQL unless the source or fields are NoSQL types
            // For now, treat as SQL
            return false;
        default:
            return false;
    }
}