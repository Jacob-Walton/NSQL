#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

/**
 * Initialize the lexer with source code.
 * 
 * @param lexer The lexer instance.
 * @param source The source code to be lexed.
 */
void lexer_init(Lexer* lexer, const char* source) {
    lexer->start   = source;
    lexer->current = source;
    lexer->line    = 1;
}

/**
 * Check if the current lexeme is at the end of the source.
 * 
 * @param lexer The lexer instance.
 * @return true if at end, false otherwise.
 */
static bool is_at_end(Lexer* lexer) {
    return *lexer->current == '\0';
}

/**
 * Create a token with the current lexeme.
 * 
 * @param lexer The lexer instance.
 * @param type The token type.
 * @return The created token.
 */
static Token make_token(Lexer* lexer, TokenType type) {
    Token token;
    token.type   = type;
    token.start  = lexer->start;
    token.length = (int)(lexer->current - lexer->start);
    token.line   = lexer->line;
    return token;
}

/**
 * Create an error token.
 * 
 * @param lexer The lexer instance.
 * @param message The error message.
 * @return The created error token.
 */
static Token error_token(Lexer* lexer, const char* message) {
    Token token;
    token.type   = TOKEN_ERROR;
    token.start  = message;
    token.length = (int)strlen(message);
    token.line   = lexer->line;
    return token;
}

/**
 * Advance the current pointer and return the character.
 * 
 * @param lexer The lexer instance.
 * @return The character at the current position.
 */
static char advance(Lexer* lexer) {
    return *(lexer->current++);
}

/**
 * Look at the current character without advancing.
 * 
 * @param lexer The lexer instance.
 * @return The current character.
 */
static char peek(Lexer* lexer) {
    return *lexer->current;
}

/**
 * Look at the next character without advancing.
 * 
 * @param lexer The lexer instance.
 * @return The next character.
 */ 
static char peek_next(Lexer* lexer) {
    if (is_at_end(lexer))
        return '\0';
    return lexer->current[1];
}

/**
 * Skip whitespace and comments in the source code.
 * 
 * @param lexer The lexer instance.
 */
static void skip_whitespace(Lexer* lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
            case '\n':
                lexer->line++;
                advance(lexer);
                break;
            case '>':  // Comment
                if (peek_next(lexer) == '>') {
                    advance(lexer); // consume first '>'
                    advance(lexer); // consume second '>'
                    while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                        advance(lexer);
                    }
                    break;
                }
                // If not '>>', fall through to default
            default:
                return;
        }
    }
}

/**
 * Check if character is a valid identifier start.
 * 
 * @param c The character to check.
 * @return true if valid, false otherwise.
 */
static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/**
 * Check if character is a valid identifier character.
 * 
 * @param c The character to check.
 * @return true if valid, false otherwise.
 */
static bool is_alnum(char c) {
    return is_alpha(c) || isdigit(c);
}

/**
 * Check if the current lexeme matches a keyword.
 * 
 * @param lexer The lexer instance.
 * @param start The starting index of the keyword.
 * @param length The length of the keyword.
 * @param rest The remaining characters after the keyword.
 * @param type The token type for the keyword.
 * @return The token type if it matches, otherwise TOKEN_IDENTIFIER.
 */
static TokenType check_keyword(Lexer* lexer, int start, int length, const char* rest,
                                TokenType type) {
    if (lexer->current - lexer->start == start + length &&
        memcmp(lexer->start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

/**
 * Scan an identifier or keyword.
 */
static TokenType identifier_type(Lexer* lexer) {
    switch (lexer->start[0]) {
        case 'A':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'S':
                        if (lexer->current - lexer->start == 2)
                            return TOKEN_AS;
                        return check_keyword(lexer, 2, 1, "K", TOKEN_ASK);
                    case 'N':
                        return check_keyword(lexer, 2, 1, "D", TOKEN_AND);
                    case 'D':
                        return check_keyword(lexer, 2, 1, "D", TOKEN_ADD);
                }
            }
            break;
        case 'B':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'Y':
                        return check_keyword(lexer, 2, 0, "", TOKEN_BY);
                }
            }
            break;
        case 'C':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'R':
                        return check_keyword(lexer, 2, 4, "EATE", TOKEN_CREATE);
                }
            }
            break;
        case 'F':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'I':
                        return check_keyword(lexer, 2, 2, "ND", TOKEN_FIND);
                    case 'O':
                        return check_keyword(lexer, 2, 1, "R", TOKEN_FOR);
                    case 'R':
                        return check_keyword(lexer, 2, 2, "OM", TOKEN_FROM);
                }
            }
            break;
        case 'G':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'E':
                        return check_keyword(lexer, 2, 1, "T", TOKEN_GET);
                    case 'R':
                        return check_keyword(lexer, 2, 3, "OUP", TOKEN_GROUP);
                }
            }
            break;
        case 'H':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'A':
                        return check_keyword(lexer, 2, 4, "VING", TOKEN_HAVING);
                }
            }
            break;
        case 'L':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'I':
                        if (lexer->current - lexer->start == 4 && lexer->start[2] == 'K' &&
                            lexer->start[3] == 'E') {
                            return TOKEN_LIKE;
                        } else if (lexer->current - lexer->start == 5 && lexer->start[2] == 'M' &&
                                   lexer->start[3] == 'I' && lexer->start[4] == 'T') {
                            return TOKEN_LIMIT;
                        }
                        break;
                }
            }
            break;
        case 'N':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'O':
                        return check_keyword(lexer, 2, 1, "T", TOKEN_NOT);
                }
            }
            break;
        case 'O':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'R':
                        if (lexer->current - lexer->start == 2)
                            return TOKEN_OR;
                        return check_keyword(lexer, 2, 3, "DER", TOKEN_ORDER);
                }
            }
            break;
        case 'R':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'E':
                        return check_keyword(lexer, 2, 4, "MOVE", TOKEN_REMOVE);
                }
            }
            break;
        case 'S':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'H':
                        return check_keyword(lexer, 2, 2, "OW", TOKEN_SHOW);
                    case 'O':
                        return check_keyword(lexer, 2, 2, "RT", TOKEN_SORT);
                }
            }
            break;
        case 'T':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'E':
                        return check_keyword(lexer, 2, 2, "LL", TOKEN_TELL);
                    case 'H':
                        return check_keyword(lexer, 2, 2, "AT", TOKEN_THAT);
                    case 'O':
                        return check_keyword(lexer, 2, 0, "", TOKEN_TO);
                }
            }
            break;
        case 'U':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'P':
                        return check_keyword(lexer, 2, 4, "DATE", TOKEN_UPDATE);
                }
            }
            break;
        case 'W':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'H':
                        if (lexer->current - lexer->start > 2) {
                            switch (lexer->start[2]) {
                                case 'E':
                                    return check_keyword(lexer, 3, 2, "RE", TOKEN_WHERE);
                                case 'N':
                                    return check_keyword(lexer, 3, 0, "", TOKEN_WHEN);
                                case 'I':
                                    return check_keyword(lexer, 3, 2, "CH", TOKEN_WHICH);
                            }
                        }
                        break;
                    case 'I':
                        return check_keyword(lexer, 2, 2, "TH", TOKEN_WITH);
                }
            }
            break;
    }

    return TOKEN_IDENTIFIER;
}

/**
 * Scan an identifier.
 * 
 * @param lexer The lexer instance.
 */
static Token identifier(Lexer* lexer) {
    while (is_alnum(peek(lexer))) advance(lexer);

    return make_token(lexer, identifier_type(lexer));
}

/**
 * Scan a number.
 * 
 * @param lexer The lexer instance.
 */
static Token number(Lexer* lexer) {
    while (isdigit(peek(lexer))) advance(lexer);

    // Look for a fractional part
    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        advance(lexer);  // Consume the '.'

        while (isdigit(peek(lexer))) advance(lexer);
        return make_token(lexer, TOKEN_DECIMAL);
    }

    return make_token(lexer, TOKEN_INTEGER);
}

/**
 * Scan a string literal.
 * 
 * @param lexer The lexer instance.
 * @return The created string token.
 */
static Token string(Lexer* lexer) {
    while (peek(lexer) != '"' && !is_at_end(lexer)) {
        if (peek(lexer) == '\n')
            lexer->line++;
        advance(lexer);
    }

    if (is_at_end(lexer))
        return error_token(lexer, "Unterminated string.");

    // The closing quote
    advance(lexer);
    return make_token(lexer, TOKEN_STRING);
}

/**
 * Get the next token from the lexer.
 * 
 * @param lexer The lexer instance.
 * @return The next token.
 */
Token lexer_next_token(Lexer* lexer) {
    skip_whitespace(lexer);

    lexer->start = lexer->current;

    if (is_at_end(lexer))
        return make_token(lexer, TOKEN_EOF);

    char c = advance(lexer);

    if (is_alpha(c))
        return identifier(lexer);
    if (isdigit(c))
        return number(lexer);

    switch (c) {
        case '(':
            return make_token(lexer, TOKEN_LPAREN);
        case ')':
            return make_token(lexer, TOKEN_RPAREN);
        case ',':
            return make_token(lexer, TOKEN_COMMA);
        case '=':
            return make_token(lexer, TOKEN_EQUAL);
        case '<':
            return make_token(lexer, peek(lexer) == '=' ? (advance(lexer), TOKEN_LTE) : TOKEN_LT);
        case '>':
            return make_token(lexer, peek(lexer) == '=' ? (advance(lexer), TOKEN_GTE) : TOKEN_GT);
        case '!':
            return make_token(lexer,
                              peek(lexer) == '=' ? (advance(lexer), TOKEN_NEQ) : TOKEN_ERROR);
        case '+':
            return make_token(lexer, TOKEN_PLUS);
        case '-':
            return make_token(lexer, TOKEN_MINUS);
        case '*':
            return make_token(lexer, TOKEN_STAR);
        case '/':
            return make_token(lexer, TOKEN_SLASH);
        case '%':
            return make_token(lexer, TOKEN_PERCENT);
        case '"':
            return string(lexer);
    }

    return error_token(lexer, "Unexpected character.");
}