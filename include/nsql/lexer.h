#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>

typedef enum {
    TOKEN_ASK,     // ASK
    TOKEN_TELL,    // TELL
    TOKEN_FIND,    // FIND
    TOKEN_SHOW,    // SHOW
    TOKEN_GET,     // GET
    TOKEN_FOR,     // FOR
    TOKEN_FROM,    // FROM
    TOKEN_TO,      // TO
    TOKEN_IF,      // IF
    TOKEN_WHEN,    // WHEN
    TOKEN_WHERE,   // WHERE
    TOKEN_THAT,    // THAT
    TOKEN_GROUP,   // GROUP
    TOKEN_SORT,    // SORT
    TOKEN_BY,      // BY
    TOKEN_LIMIT,   // LIMIT
    TOKEN_AND,     // AND
    TOKEN_OR,      // OR
    TOKEN_HAVING,  // HAVING
    TOKEN_ORDER,   // ORDER
    TOKEN_ADD,     // ADD
    TOKEN_REMOVE,  // REMOVE
    TOKEN_UPDATE,  // UPDATE
    TOKEN_CREATE,  // CREATE
    TOKEN_WITH,    // WITH
    TOKEN_AS,      // AS
    TOKEN_IN,      // IN
    TOKEN_NOT,     // NOT
    TOKEN_WHICH,   // WHICH

    // Operators
    TOKEN_PLUS,     // +
    TOKEN_MINUS,    // -
    TOKEN_STAR,     // *
    TOKEN_SLASH,    // /
    TOKEN_PERCENT,  // %
    TOKEN_EQUAL,    // =
    TOKEN_GT,       // >
    TOKEN_LT,       //
    TOKEN_GTE,      // >=
    TOKEN_LTE,      // <=
    TOKEN_NEQ,      // !=
    TOKEN_LIKE,     // LIKE

    // Literals and others
    TOKEN_IDENTIFIER,  // Names
    TOKEN_STRING,      // String literals
    TOKEN_INTEGER,     // Integer literals
    TOKEN_DECIMAL,     // Decimal literals
    TOKEN_COMMA,       // ,
    TOKEN_LPAREN,      // (
    TOKEN_RPAREN,      // )
    TOKEN_EOF,         // End of input
    TOKEN_ERROR,       // Error token
    TOKEN_TERMINATOR   // ; or PLEASE (depending on how polite you are)
} NsqlTokenType;

typedef struct {
    NsqlTokenType   type;
    const char* start;
    size_t      length;
    int         line;
} Token;

typedef struct {
    const char* start;
    const char* current;
    int         line;
} Lexer;

void  lexer_init(Lexer* lexer, const char* source);
Token lexer_next_token(Lexer* lexer);
const char* lexer_get_line_start(Lexer* lexer, int line);

#ifdef __cplusplus
}
#endif