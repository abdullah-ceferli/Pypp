#pragma once
#include <string>
#include <vector>
#include <memory>

struct Stmt;
struct Expr;
using StmtPtr = std::unique_ptr<Stmt>;
using ExprPtr = std::unique_ptr<Expr>;

// ─── Expressions ─────────────────────────────────────────────────────────────

enum class ExprKind { Number, String, Ident, BinOp, UnaryOp, ArrayLit, Null };

struct Expr {
    ExprKind kind;
    std::string sval;
    double nval = 0;
    ExprPtr left, right;
    ExprPtr operand;
    std::vector<ExprPtr> elements; // for ArrayLit
};

ExprPtr make_num(double v);
ExprPtr make_str(const std::string& s);
ExprPtr make_ident(const std::string& name);
ExprPtr make_binop(const std::string& op, ExprPtr l, ExprPtr r);
ExprPtr make_unary(const std::string& op, ExprPtr operand);
ExprPtr make_null();
ExprPtr make_array(std::vector<ExprPtr> elems);

// ─── Statements ──────────────────────────────────────────────────────────────

enum class StmtKind { Say, VarDecl, Assign, If, While, For, Break, VarDump };

struct IfBranch {
    ExprPtr condition;
    std::vector<StmtPtr> body;
};

struct Stmt {
    StmtKind kind;
    std::string name;
    std::string type_name;
    ExprPtr expr;
    std::vector<StmtPtr> body;
    std::vector<IfBranch> branches;
    // For loop
    std::string iter_var;
    ExprPtr iter_target;
    ExprPtr range_start;
    ExprPtr range_end;
    bool is_range = false;
};
