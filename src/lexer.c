#include <ctype.h>
#include <nsql/lexer.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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
 * Creates a token representing the current lexeme in the source.
 *
 * The token includes its type, starting position, length, and line number.
 *
 * @param type The type to assign to the created token.
 * @return A token corresponding to the current lexeme.
 */
static Token make_token(Lexer* lexer, NsqlTokenType type) {
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
                    advance(lexer);  // consume first '>'
                    advance(lexer);  // consume second '>'
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
 * Determines if the current lexeme matches a specific keyword and returns its token type.
 *
 * Compares a substring of the current lexeme to a given keyword. If it matches exactly, returns the
 * specified keyword token type; otherwise, returns TOKEN_IDENTIFIER.
 *
 * @return The keyword token type if matched; otherwise, TOKEN_IDENTIFIER.
 */
static NsqlTokenType check_keyword(Lexer* lexer, int start, int length, const char* rest,
                                   NsqlTokenType type) {
    if (lexer->current - lexer->start == start + length &&
        memcmp(lexer->start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

/**
 * @brief Determines the token type for an identifier or keyword.
 *
 * Checks the current lexeme in the lexer to see if it matches any reserved SQL-like keywords and
 * returns the corresponding token type. If no keyword matches, returns TOKEN_IDENTIFIER.
 *
 * @return NsqlTokenType The token type corresponding to the matched keyword, or TOKEN_IDENTIFIER if
 * no keyword is matched.
 */
static NsqlTokenType identifier_type(Lexer* lexer) {
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
        case 'P':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'L':
                        return check_keyword(lexer, 2, 4, "EASE", TOKEN_TERMINATOR);
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
    // Save the opening quote character to verify closing quote later
    char quote = lexer->start[0];

    while ((peek(lexer) != quote) && !is_at_end(lexer)) {
        if (peek(lexer) == '\n')
            lexer->line++;
        advance(lexer);
    }

    if (is_at_end(lexer))
        return error_token(lexer, "Unterminated string.");

    // Closing quote
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
        case '\'':
            return string(lexer);
        case ';':
            return make_token(lexer, TOKEN_TERMINATOR);
    }

    return error_token(lexer, "Unexpected character.");
}

/**
 * Get the starting position of a specific line in the source code.
 *
 * @param lexer The lexer instance.
 * @param line The line number (1-based).
 * @return Pointer to the beginning of the specified line,
 *         or start of source if line < 1,
 *         or end of source (terminating '\0' char) if line > total lines.
 */
const char* lexer_get_line_start(Lexer* lexer, int line) {
    if (!lexer || line < 1) {
        return lexer ? lexer->start : NULL;
    }

    // Start from the beginning of the source
    const char* current      = lexer->start;
    int         current_line = 1;

    // Find the start of the requested line
    while (current_line < line && *current) {
        // If we find a newline, we've found the end of a line
        if (*current == '\n') {
            current_line++;
            if (current_line == line) {
                // Return the character after the newline
                return current + 1;
            }
        }
        current++;
    }

    // If the requested line is greater than available lines,
    // return the start of the last line
    return current;
}