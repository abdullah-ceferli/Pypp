#pragma once
#include <string>
#include <vector>

enum class TokenType {
    // Literals
    NUMBER, STRING, IDENT,
    // Types
    TYPE_INT, TYPE_FLOAT, TYPE_STR, TYPE_BOOL, TYPE_ARRAY,
    // Special values
    KW_NULL,
    // Keywords
    SAY, IF, IFELSE, ELSE, WHILE, FOR, IN, BREAK, VAR_DUMP,
    // Operators
    PLUS, MINUS, STAR, SLASH, PERCENT,
    EQ, NEQ, LT, GT, LEQ, GEQ,
    AND, OR, NOT,
    ASSIGN,
    DOTDOT,
    // Delimiters
    LBRACKET, RBRACKET,
    LBRACE,   RBRACE,
    COMMA,
    // Structure
    NEWLINE, INDENT, DEDENT,
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    std::string src;
    int pos;
    int line;
    std::vector<int> indent_stack;

    char peek(int offset = 0);
    char advance();
    Token make_token(TokenType t, const std::string& v);
    Token read_string();
    Token read_number();
    Token read_ident_or_keyword();
    int count_indent();
};
