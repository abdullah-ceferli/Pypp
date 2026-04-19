#include "lexer.h"
#include <stdexcept>
#include <cctype>

Lexer::Lexer(const std::string& source)
    : src(source), pos(0), line(1) {
    indent_stack.push_back(0);
}

char Lexer::peek(int offset) {
    int i = pos + offset;
    if (i >= (int)src.size()) return '\0';
    return src[i];
}

char Lexer::advance() {
    char c = src[pos++];
    if (c == '\n') line++;
    return c;
}

Token Lexer::make_token(TokenType t, const std::string& v) {
    return {t, v, line};
}

int Lexer::count_indent() {
    int count = 0, i = pos;
    while (i < (int)src.size() && src[i] == ' ') { count++; i++; }
    return count;
}

Token Lexer::read_string() {
    advance(); // skip opening "
    std::string val;
    while (pos < (int)src.size() && peek() != '"') {
        char c = advance();
        if (c == '\\' && peek() == 'n') { advance(); val += '\n'; }
        else val += c;
    }
    if (peek() != '"') throw std::runtime_error("Unterminated string on line " + std::to_string(line));
    advance();
    return make_token(TokenType::STRING, val);
}

Token Lexer::read_number() {
    std::string val;
    while (pos < (int)src.size() && (std::isdigit(peek()) || peek() == '.')) {
        if (peek() == '.' && peek(1) == '.') break;
        val += advance();
    }
    return make_token(TokenType::NUMBER, val);
}

Token Lexer::read_ident_or_keyword() {
    std::string val;
    while (pos < (int)src.size() && (std::isalnum(peek()) || peek() == '_')) val += advance();

    if (val == "say")      return make_token(TokenType::SAY,      val);
    if (val == "if")       return make_token(TokenType::IF,       val);
    if (val == "ifelse")   return make_token(TokenType::IFELSE,   val);
    if (val == "else")     return make_token(TokenType::ELSE,     val);
    if (val == "while")    return make_token(TokenType::WHILE,    val);
    if (val == "for")      return make_token(TokenType::FOR,      val);
    if (val == "in")       return make_token(TokenType::IN,       val);
    if (val == "break")    return make_token(TokenType::BREAK,    val);
    if (val == "var_dump") return make_token(TokenType::VAR_DUMP, val);
    if (val == "and")      return make_token(TokenType::AND,      val);
    if (val == "or")       return make_token(TokenType::OR,       val);
    if (val == "not")      return make_token(TokenType::NOT,      val);
    if (val == "Null" || val == "null") return make_token(TokenType::KW_NULL, val);
    if (val == "int")      return make_token(TokenType::TYPE_INT,   val);
    if (val == "float")    return make_token(TokenType::TYPE_FLOAT, val);
    if (val == "str")      return make_token(TokenType::TYPE_STR,   val);
    if (val == "bool")     return make_token(TokenType::TYPE_BOOL,  val);
    if (val == "array")    return make_token(TokenType::TYPE_ARRAY, val);
    return make_token(TokenType::IDENT, val);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    bool line_start = true;

    while (pos < (int)src.size()) {
        if (line_start) {
            line_start = false;
            int indent = count_indent();
            int current = indent_stack.back();
            if (indent > current) {
                indent_stack.push_back(indent);
                pos += indent;
                tokens.push_back(make_token(TokenType::INDENT, ""));
            } else {
                pos += indent;
                while (indent < indent_stack.back()) {
                    indent_stack.pop_back();
                    tokens.push_back(make_token(TokenType::DEDENT, ""));
                }
            }
            continue;
        }

        char c = peek();

        if (c == '#') { while (pos < (int)src.size() && peek() != '\n') advance(); continue; }
        if (c == '\n') { advance(); line_start = true; tokens.push_back(make_token(TokenType::NEWLINE, "")); continue; }
        if (c == ' ' || c == '\t') { advance(); continue; }
        if (c == '"') { tokens.push_back(read_string()); continue; }
        if (std::isdigit(c)) { tokens.push_back(read_number()); continue; }
        if (std::isalpha(c) || c == '_') { tokens.push_back(read_ident_or_keyword()); continue; }

        advance();
        switch (c) {
            case '+': tokens.push_back(make_token(TokenType::PLUS,    "+")); break;
            case '-': tokens.push_back(make_token(TokenType::MINUS,   "-")); break;
            case '*': tokens.push_back(make_token(TokenType::STAR,    "*")); break;
            case '/': tokens.push_back(make_token(TokenType::SLASH,   "/")); break;
            case '%': tokens.push_back(make_token(TokenType::PERCENT, "%")); break;
            case ',': tokens.push_back(make_token(TokenType::COMMA,   ",")); break;
            case '[': tokens.push_back(make_token(TokenType::LBRACKET,"[")); break;
            case ']': tokens.push_back(make_token(TokenType::RBRACKET,"]")); break;
            case '{': tokens.push_back(make_token(TokenType::LBRACE,  "{")); break;
            case '}': tokens.push_back(make_token(TokenType::RBRACE,  "}")); break;
            case '.':
                if (peek() == '.') { advance(); tokens.push_back(make_token(TokenType::DOTDOT, "..")); }
                break;
            case '=':
                if (peek() == '=') { advance(); tokens.push_back(make_token(TokenType::EQ,  "==")); }
                else tokens.push_back(make_token(TokenType::ASSIGN, "="));
                break;
            case '!':
                if (peek() == '=') { advance(); tokens.push_back(make_token(TokenType::NEQ, "!=")); }
                break;
            case '<':
                if (peek() == '=') { advance(); tokens.push_back(make_token(TokenType::LEQ, "<=")); }
                else tokens.push_back(make_token(TokenType::LT, "<"));
                break;
            case '>':
                if (peek() == '=') { advance(); tokens.push_back(make_token(TokenType::GEQ, ">=")); }
                else tokens.push_back(make_token(TokenType::GT, ">"));
                break;
            default:
                throw std::runtime_error(std::string("Unknown character '") + c + "' on line " + std::to_string(line));
        }
    }

    while (indent_stack.size() > 1) {
        indent_stack.pop_back();
        tokens.push_back(make_token(TokenType::DEDENT, ""));
    }
    tokens.push_back(make_token(TokenType::END_OF_FILE, ""));
    return tokens;
}
