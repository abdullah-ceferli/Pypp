#pragma once
#include "lexer.h"
#include "ast.h"
#include <vector>

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    std::vector<StmtPtr> parse();

private:
    std::vector<Token> tokens;
    int pos;

    Token& peek(int offset = 0);
    Token consume();
    Token expect(TokenType t, const std::string& msg);
    bool at(TokenType t, int offset = 0);
    void skip_newlines();

    std::vector<StmtPtr> parse_block();
    StmtPtr parse_stmt();
    StmtPtr parse_say();
    StmtPtr parse_var_decl(const std::string& type_name);
    StmtPtr parse_assign(const std::string& name);
    StmtPtr parse_if();
    StmtPtr parse_while();
    StmtPtr parse_for();
    StmtPtr parse_var_dump();

    ExprPtr parse_expr();
    ExprPtr parse_or();
    ExprPtr parse_and();
    ExprPtr parse_not();
    ExprPtr parse_comparison();
    ExprPtr parse_addition();
    ExprPtr parse_multiplication();
    ExprPtr parse_unary();
    ExprPtr parse_primary();
    ExprPtr parse_array_literal();
};
