#include "parser.h"
#include <stdexcept>
#include <cstdlib>

Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)), pos(0) {}

Token& Parser::peek(int offset) {
    int i = pos + offset;
    if (i >= (int)tokens.size()) return tokens.back();
    return tokens[i];
}
Token Parser::consume() { return tokens[pos++]; }
Token Parser::expect(TokenType t, const std::string& msg) {
    if (!at(t)) throw std::runtime_error(msg + " (got '" + peek().value + "' on line " + std::to_string(peek().line) + ")");
    return consume();
}
bool Parser::at(TokenType t, int offset) { return peek(offset).type == t; }
void Parser::skip_newlines() { while (at(TokenType::NEWLINE)) consume(); }

std::vector<StmtPtr> Parser::parse() {
    std::vector<StmtPtr> stmts;
    skip_newlines();
    while (!at(TokenType::END_OF_FILE)) { stmts.push_back(parse_stmt()); skip_newlines(); }
    return stmts;
}

// Block: supports both indented block AND {} empty block
std::vector<StmtPtr> Parser::parse_block() {
    skip_newlines();
    // {} empty block
    if (at(TokenType::LBRACE)) {
        consume();
        skip_newlines();
        expect(TokenType::RBRACE, "Expected '}'");
        return {};
    }
    // indented block
    expect(TokenType::INDENT, "Expected indented block or {}");
    std::vector<StmtPtr> stmts;
    skip_newlines();
    while (!at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
        stmts.push_back(parse_stmt()); skip_newlines();
    }
    if (at(TokenType::DEDENT)) consume();
    return stmts;
}

StmtPtr Parser::parse_stmt() {
    skip_newlines();

    if (at(TokenType::TYPE_INT) || at(TokenType::TYPE_FLOAT) ||
        at(TokenType::TYPE_STR) || at(TokenType::TYPE_BOOL) || at(TokenType::TYPE_ARRAY)) {
        std::string type_name = consume().value;
        return parse_var_decl(type_name);
    }
    if (at(TokenType::SAY))      return parse_say();
    if (at(TokenType::IF))       return parse_if();
    if (at(TokenType::WHILE))    return parse_while();
    if (at(TokenType::FOR))      return parse_for();
    if (at(TokenType::VAR_DUMP)) return parse_var_dump();
    if (at(TokenType::BREAK)) {
        consume();
        skip_newlines();
        auto s = std::make_unique<Stmt>(); s->kind = StmtKind::Break; return s;
    }
    // {} empty block used as a no-op statement
    if (at(TokenType::LBRACE)) {
        consume();
        skip_newlines();
        expect(TokenType::RBRACE, "Expected '}'");
        skip_newlines();
        // return a dummy if-false block (no-op)
        auto s = std::make_unique<Stmt>(); s->kind = StmtKind::Break; // reuse as noop placeholder
        // Actually make a proper noop - just an empty while false
        auto noop = std::make_unique<Stmt>(); noop->kind = StmtKind::If;
        return noop; // empty if with no branches = no-op
    }
    if (at(TokenType::IDENT)) {
        std::string name = consume().value;
        if (at(TokenType::ASSIGN)) return parse_assign(name);
        throw std::runtime_error("Unexpected identifier '" + name + "' on line " + std::to_string(peek().line));
    }
    throw std::runtime_error("Unexpected token '" + peek().value + "' on line " + std::to_string(peek().line));
}

StmtPtr Parser::parse_say() {
    expect(TokenType::SAY, "Expected 'say'");
    auto s = std::make_unique<Stmt>(); s->kind = StmtKind::Say;
    s->expr = parse_expr(); skip_newlines(); return s;
}

StmtPtr Parser::parse_var_decl(const std::string& type_name) {
    auto s = std::make_unique<Stmt>(); s->kind = StmtKind::VarDecl;
    s->type_name = type_name;
    s->name = expect(TokenType::IDENT, "Expected variable name").value;
    expect(TokenType::ASSIGN, "Expected '='");
    s->expr = parse_expr(); skip_newlines(); return s;
}

StmtPtr Parser::parse_assign(const std::string& name) {
    consume(); // eat '='
    auto s = std::make_unique<Stmt>(); s->kind = StmtKind::Assign;
    s->name = name; s->expr = parse_expr(); skip_newlines(); return s;
}

StmtPtr Parser::parse_if() {
    auto s = std::make_unique<Stmt>(); s->kind = StmtKind::If;
    expect(TokenType::IF, "Expected 'if'");
    IfBranch first; first.condition = parse_expr();
    skip_newlines(); first.body = parse_block();
    s->branches.push_back(std::move(first));
    while (at(TokenType::IFELSE)) {
        consume(); IfBranch b; b.condition = parse_expr();
        skip_newlines(); b.body = parse_block();
        s->branches.push_back(std::move(b));
    }
    if (at(TokenType::ELSE)) {
        consume(); skip_newlines();
        IfBranch b; b.condition = nullptr;
        b.body = parse_block();
        s->branches.push_back(std::move(b));
    }
    return s;
}

StmtPtr Parser::parse_while() {
    expect(TokenType::WHILE, "Expected 'while'");
    auto s = std::make_unique<Stmt>(); s->kind = StmtKind::While;
    s->expr = parse_expr(); skip_newlines(); s->body = parse_block(); return s;
}

StmtPtr Parser::parse_for() {
    expect(TokenType::FOR, "Expected 'for'");
    auto s = std::make_unique<Stmt>(); s->kind = StmtKind::For;
    s->iter_var = expect(TokenType::IDENT, "Expected loop variable").value;
    expect(TokenType::IN, "Expected 'in'");
    if (at(TokenType::NUMBER)) {
        double sv = std::atof(consume().value.c_str());
        if (!at(TokenType::DOTDOT)) throw std::runtime_error("Expected '..' for range");
        consume(); s->is_range = true;
        s->range_start = make_num(sv); s->range_end = parse_expr();
    } else {
        s->is_range = false; s->iter_target = parse_expr();
    }
    skip_newlines(); s->body = parse_block(); return s;
}

StmtPtr Parser::parse_var_dump() {
    expect(TokenType::VAR_DUMP, "Expected 'var_dump'");
    // expect var_dump(x)
    if (!at(TokenType::LBRACKET) && !at(TokenType::IDENT)) {
        // handle var_dump(x) with actual parens via LBRACKET workaround
        // we use ( ) but lexer doesn't have LPAREN — let's handle IDENT directly
    }
    // Since we don't have parens in lexer, support: var_dump x  OR  var_dump(x) 
    // We'll just parse next expression
    auto s = std::make_unique<Stmt>(); s->kind = StmtKind::VarDump;
    s->expr = parse_expr(); skip_newlines(); return s;
}

// ─── Expressions ─────────────────────────────────────────────────────────────

ExprPtr Parser::parse_expr()           { return parse_or(); }
ExprPtr Parser::parse_or() {
    auto l = parse_and();
    while (at(TokenType::OR)) { consume(); l = make_binop("||", std::move(l), parse_and()); }
    return l;
}
ExprPtr Parser::parse_and() {
    auto l = parse_not();
    while (at(TokenType::AND)) { consume(); l = make_binop("&&", std::move(l), parse_not()); }
    return l;
}
ExprPtr Parser::parse_not() {
    if (at(TokenType::NOT)) { consume(); return make_unary("!", parse_not()); }
    return parse_comparison();
}
ExprPtr Parser::parse_comparison() {
    auto l = parse_addition();
    while (at(TokenType::EQ)||at(TokenType::NEQ)||at(TokenType::LT)||
           at(TokenType::GT)||at(TokenType::LEQ)||at(TokenType::GEQ)) {
        std::string op = consume().value; l = make_binop(op, std::move(l), parse_addition());
    }
    return l;
}
ExprPtr Parser::parse_addition() {
    auto l = parse_multiplication();
    while (at(TokenType::PLUS)||at(TokenType::MINUS)) {
        std::string op = consume().value; l = make_binop(op, std::move(l), parse_multiplication());
    }
    return l;
}
ExprPtr Parser::parse_multiplication() {
    auto l = parse_unary();
    while (at(TokenType::STAR)||at(TokenType::SLASH)||at(TokenType::PERCENT)) {
        std::string op = consume().value; l = make_binop(op, std::move(l), parse_unary());
    }
    return l;
}
ExprPtr Parser::parse_unary() {
    if (at(TokenType::MINUS)) { consume(); return make_unary("-", parse_primary()); }
    return parse_primary();
}
ExprPtr Parser::parse_primary() {
    if (at(TokenType::NUMBER))  return make_num(std::atof(consume().value.c_str()));
    if (at(TokenType::STRING))  return make_str(consume().value);
    if (at(TokenType::KW_NULL)) { consume(); return make_null(); }
    if (at(TokenType::LBRACKET)) return parse_array_literal();
    if (at(TokenType::IDENT))   return make_ident(consume().value);
    throw std::runtime_error("Expected expression, got '" + peek().value + "' on line " + std::to_string(peek().line));
}

ExprPtr Parser::parse_array_literal() {
    expect(TokenType::LBRACKET, "Expected '['");
    std::vector<ExprPtr> elems;
    skip_newlines();
    while (!at(TokenType::RBRACKET) && !at(TokenType::END_OF_FILE)) {
        elems.push_back(parse_expr());
        skip_newlines();
        if (at(TokenType::COMMA)) consume();
        skip_newlines();
    }
    expect(TokenType::RBRACKET, "Expected ']'");
    return make_array(std::move(elems));
}
