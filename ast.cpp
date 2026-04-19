#include "ast.h"

ExprPtr make_num(double v) {
    auto e = std::make_unique<Expr>(); e->kind = ExprKind::Number; e->nval = v; return e;
}
ExprPtr make_str(const std::string& s) {
    auto e = std::make_unique<Expr>(); e->kind = ExprKind::String; e->sval = s; return e;
}
ExprPtr make_ident(const std::string& name) {
    auto e = std::make_unique<Expr>(); e->kind = ExprKind::Ident; e->sval = name; return e;
}
ExprPtr make_binop(const std::string& op, ExprPtr l, ExprPtr r) {
    auto e = std::make_unique<Expr>(); e->kind = ExprKind::BinOp;
    e->sval = op; e->left = std::move(l); e->right = std::move(r); return e;
}
ExprPtr make_unary(const std::string& op, ExprPtr operand) {
    auto e = std::make_unique<Expr>(); e->kind = ExprKind::UnaryOp;
    e->sval = op; e->operand = std::move(operand); return e;
}
ExprPtr make_null() {
    auto e = std::make_unique<Expr>(); e->kind = ExprKind::Null; return e;
}
ExprPtr make_array(std::vector<ExprPtr> elems) {
    auto e = std::make_unique<Expr>(); e->kind = ExprKind::ArrayLit;
    e->elements = std::move(elems); return e;
}
