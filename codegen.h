#pragma once
#include "ast.h"
#include <string>
#include <sstream>
#include <unordered_map>

class CodeGen {
public:
    std::string generate(const std::vector<StmtPtr>& stmts);

private:
    std::ostringstream out;
    int indent_level = 0;
    std::unordered_map<std::string, std::string> var_types;

    void emit(const std::string& line);
    std::string ind();

    void gen_stmts(const std::vector<StmtPtr>& stmts);
    void gen_stmt(const Stmt& s);
    void gen_say(const Stmt& s);
    void gen_var_decl(const Stmt& s);
    void gen_assign(const Stmt& s);
    void gen_if(const Stmt& s);
    void gen_while(const Stmt& s);
    void gen_for(const Stmt& s);
    void gen_var_dump(const Stmt& s);

    std::string gen_expr(const Expr& e);
    std::string gen_value_expr(const Expr& e); // always returns PyPlusValue
    bool expr_needs_value(const Expr& e);
};
