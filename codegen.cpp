#include "codegen.h"
#include <stdexcept>
#include <cmath>

std::string CodeGen::ind() { return std::string(indent_level * 4, ' '); }
void CodeGen::emit(const std::string& line) { out << ind() << line << "\n"; }

// Runtime header emitted once at top of every compiled program
static const char* RUNTIME = R"(
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>

// ── PyPlus Runtime Value ──────────────────────────────────────────────────────
struct PyPlusValue;
using PPV = std::shared_ptr<PyPlusValue>;
using PPArr = std::vector<PPV>;

enum class PPType { Int, Float, Str, Bool, Array, Null };

struct PyPlusValue {
    PPType type;
    long long  ival = 0;
    double     fval = 0.0;
    std::string sval;
    bool        bval = false;
    PPArr       arr;

    static PPV make_int(long long v)          { auto p=std::make_shared<PyPlusValue>(); p->type=PPType::Int;   p->ival=v; return p; }
    static PPV make_float(double v)           { auto p=std::make_shared<PyPlusValue>(); p->type=PPType::Float; p->fval=v; return p; }
    static PPV make_str(const std::string& v) { auto p=std::make_shared<PyPlusValue>(); p->type=PPType::Str;   p->sval=v; return p; }
    static PPV make_bool(bool v)              { auto p=std::make_shared<PyPlusValue>(); p->type=PPType::Bool;  p->bval=v; return p; }
    static PPV make_null()                    { auto p=std::make_shared<PyPlusValue>(); p->type=PPType::Null;             return p; }
    static PPV make_array(PPArr v)            { auto p=std::make_shared<PyPlusValue>(); p->type=PPType::Array; p->arr=v;  return p; }

    std::string type_name() const {
        switch(type) {
            case PPType::Int:   return "int";
            case PPType::Float: return "float";
            case PPType::Str:   return "str";
            case PPType::Bool:  return "bool";
            case PPType::Array: return "array";
            case PPType::Null:  return "null";
        }
        return "unknown";
    }

    std::string to_str() const {
        switch(type) {
            case PPType::Int:   return std::to_string(ival);
            case PPType::Float: { std::ostringstream ss; ss << std::setprecision(10) << fval; return ss.str(); }
            case PPType::Str:   return sval;
            case PPType::Bool:  return bval ? "true" : "false";
            case PPType::Null:  return "null";
            case PPType::Array: return "[array]";
        }
        return "";
    }

    bool truthy() const {
        switch(type) {
            case PPType::Int:   return ival != 0;
            case PPType::Float: return fval != 0.0;
            case PPType::Str:   return !sval.empty();
            case PPType::Bool:  return bval;
            case PPType::Null:  return false;
            case PPType::Array: return !arr.empty();
        }
        return false;
    }
};

// Arithmetic helpers
PPV pp_add(PPV a, PPV b) {
    if (a->type==PPType::Str || b->type==PPType::Str)
        return PyPlusValue::make_str(a->to_str() + b->to_str());
    if (a->type==PPType::Float || b->type==PPType::Float)
        return PyPlusValue::make_float(
            (a->type==PPType::Float?a->fval:(double)a->ival) +
            (b->type==PPType::Float?b->fval:(double)b->ival));
    return PyPlusValue::make_int(a->ival + b->ival);
}
PPV pp_sub(PPV a, PPV b) {
    if (a->type==PPType::Float || b->type==PPType::Float)
        return PyPlusValue::make_float(
            (a->type==PPType::Float?a->fval:(double)a->ival) -
            (b->type==PPType::Float?b->fval:(double)b->ival));
    return PyPlusValue::make_int(a->ival - b->ival);
}
PPV pp_mul(PPV a, PPV b) {
    if (a->type==PPType::Float || b->type==PPType::Float)
        return PyPlusValue::make_float(
            (a->type==PPType::Float?a->fval:(double)a->ival) *
            (b->type==PPType::Float?b->fval:(double)b->ival));
    return PyPlusValue::make_int(a->ival * b->ival);
}
PPV pp_div(PPV a, PPV b) {
    double av = a->type==PPType::Float?a->fval:(double)a->ival;
    double bv = b->type==PPType::Float?b->fval:(double)b->ival;
    return PyPlusValue::make_float(av / bv);
}
PPV pp_eq(PPV a, PPV b) {
    if (a->type != b->type) return PyPlusValue::make_bool(false);
    switch(a->type) {
        case PPType::Int:   return PyPlusValue::make_bool(a->ival == b->ival);
        case PPType::Float: return PyPlusValue::make_bool(a->fval == b->fval);
        case PPType::Str:   return PyPlusValue::make_bool(a->sval == b->sval);
        case PPType::Bool:  return PyPlusValue::make_bool(a->bval == b->bval);
        case PPType::Null:  return PyPlusValue::make_bool(true);
        default:            return PyPlusValue::make_bool(false);
    }
}
PPV pp_lt(PPV a, PPV b) {
    if (a->type==PPType::Float||b->type==PPType::Float)
        return PyPlusValue::make_bool((a->type==PPType::Float?a->fval:(double)a->ival) < (b->type==PPType::Float?b->fval:(double)b->ival));
    return PyPlusValue::make_bool(a->ival < b->ival);
}
PPV pp_gt(PPV a, PPV b) { return pp_lt(b, a); }
PPV pp_leq(PPV a, PPV b) {
    return PyPlusValue::make_bool(pp_lt(a,b)->bval || pp_eq(a,b)->bval);
}
PPV pp_geq(PPV a, PPV b) {
    return PyPlusValue::make_bool(pp_gt(a,b)->bval || pp_eq(a,b)->bval);
}
PPV pp_neq(PPV a, PPV b) { return PyPlusValue::make_bool(!pp_eq(a,b)->bval); }

// var_dump recursive printer
void pp_var_dump(PPV val, int depth=0) {
    std::string pad(depth * 2, ' ');
    if (val->type == PPType::Array) {
        std::cout << "{\n";
        for (int i = 0; i < (int)val->arr.size(); i++) {
            auto& elem = val->arr[i];
            std::cout << pad << "  " << elem->type_name() << "[" << i << "] => ";
            pp_var_dump(elem, depth + 1);
        }
        std::cout << pad << "}\n";
    } else if (val->type == PPType::Null) {
        std::cout << "null\n";
    } else if (val->type == PPType::Str) {
        std::cout << "\"" << val->sval << "\"\n";
    } else {
        std::cout << val->to_str() << "\n";
    }
}
// ─────────────────────────────────────────────────────────────────────────────
)";

// ── CodeGen implementation ────────────────────────────────────────────────────

std::string CodeGen::gen_value_expr(const Expr& e) {
    switch (e.kind) {
        case ExprKind::Null:
            return "PyPlusValue::make_null()";
        case ExprKind::Number: {
            double v = e.nval;
            if (v == std::floor(v) && std::abs(v) < 1e15)
                return "PyPlusValue::make_int(" + std::to_string((long long)v) + "LL)";
            return "PyPlusValue::make_float(" + std::to_string(v) + ")";
        }
        case ExprKind::String:
            return "PyPlusValue::make_str(\"" + e.sval + "\")";
        case ExprKind::Ident:
            return e.sval; // already a PPV variable
        case ExprKind::ArrayLit: {
            std::string s = "PyPlusValue::make_array(PPArr{";
            for (int i = 0; i < (int)e.elements.size(); i++) {
                if (i) s += ", ";
                s += gen_value_expr(*e.elements[i]);
            }
            s += "})";
            return s;
        }
        case ExprKind::BinOp: {
            std::string l = gen_value_expr(*e.left);
            std::string r = gen_value_expr(*e.right);
            if (e.sval == "+")  return "pp_add(" + l + ", " + r + ")";
            if (e.sval == "-")  return "pp_sub(" + l + ", " + r + ")";
            if (e.sval == "*")  return "pp_mul(" + l + ", " + r + ")";
            if (e.sval == "/")  return "pp_div(" + l + ", " + r + ")";
            if (e.sval == "==") return "pp_eq("  + l + ", " + r + ")";
            if (e.sval == "!=") return "pp_neq(" + l + ", " + r + ")";
            if (e.sval == "<")  return "pp_lt("  + l + ", " + r + ")";
            if (e.sval == ">")  return "pp_gt("  + l + ", " + r + ")";
            if (e.sval == "<=") return "pp_leq(" + l + ", " + r + ")";
            if (e.sval == ">=") return "pp_geq(" + l + ", " + r + ")";
            if (e.sval == "&&") return "PyPlusValue::make_bool((" + l + ")->truthy() && (" + r + ")->truthy())";
            if (e.sval == "||") return "PyPlusValue::make_bool((" + l + ")->truthy() || (" + r + ")->truthy())";
            throw std::runtime_error("Unknown operator: " + e.sval);
        }
        case ExprKind::UnaryOp: {
            std::string operand = gen_value_expr(*e.operand);
            if (e.sval == "!") return "PyPlusValue::make_bool(!(" + operand + ")->truthy())";
            if (e.sval == "-") return "PyPlusValue::make_int(-(" + operand + ")->ival)";
            throw std::runtime_error("Unknown unary op: " + e.sval);
        }
    }
    return "PyPlusValue::make_null()";
}

// gen_expr is now just gen_value_expr since everything is PPV
std::string CodeGen::gen_expr(const Expr& e) { return gen_value_expr(e); }

void CodeGen::gen_say(const Stmt& s) {
    emit("std::cout << (" + gen_expr(*s.expr) + ")->to_str() << std::endl;");
}

void CodeGen::gen_var_decl(const Stmt& s) {
    var_types[s.name] = s.type_name;
    emit("PPV " + s.name + " = " + gen_expr(*s.expr) + ";");
}

void CodeGen::gen_assign(const Stmt& s) {
    emit(s.name + " = " + gen_expr(*s.expr) + ";");
}

void CodeGen::gen_if(const Stmt& s) {
    if (s.branches.empty()) return; // noop {}
    bool first = true;
    for (const auto& branch : s.branches) {
        if (branch.condition) {
            std::string cond = "(" + gen_expr(*branch.condition) + ")->truthy()";
            if (first) emit("if (" + cond + ") {");
            else       emit("else if (" + cond + ") {");
            first = false;
        } else {
            emit("else {");
        }
        indent_level++;
        gen_stmts(branch.body);
        indent_level--;
        emit("}");
    }
}

void CodeGen::gen_while(const Stmt& s) {
    emit("while ((" + gen_expr(*s.expr) + ")->truthy()) {");
    indent_level++;
    gen_stmts(s.body);
    indent_level--;
    emit("}");
}

void CodeGen::gen_for(const Stmt& s) {
    if (s.is_range) {
        std::string start = "(" + gen_expr(*s.range_start) + ")->ival";
        std::string end   = "(" + gen_expr(*s.range_end)   + ")->ival";
        emit("for (long long _raw_" + s.iter_var + " = " + start + "; _raw_" + s.iter_var + " < " + end + "; _raw_" + s.iter_var + "++) {");
        indent_level++;
        emit("PPV " + s.iter_var + " = PyPlusValue::make_int(_raw_" + s.iter_var + ");");
        gen_stmts(s.body);
        indent_level--;
        emit("}");
    } else {
        std::string iterable = gen_expr(*s.iter_target);
        emit("for (auto& _elem_" + s.iter_var + " : (" + iterable + ")->arr) {");
        indent_level++;
        emit("PPV " + s.iter_var + " = _elem_" + s.iter_var + ";");
        gen_stmts(s.body);
        indent_level--;
        emit("}");
    }
}

void CodeGen::gen_var_dump(const Stmt& s) {
    emit("std::cout << \"" + (s.expr->kind == ExprKind::Ident ? s.expr->sval : "value") + " \";");
    emit("pp_var_dump(" + gen_expr(*s.expr) + ");");
}

void CodeGen::gen_stmt(const Stmt& s) {
    switch (s.kind) {
        case StmtKind::Say:     gen_say(s);      break;
        case StmtKind::VarDecl: gen_var_decl(s); break;
        case StmtKind::Assign:  gen_assign(s);   break;
        case StmtKind::If:      gen_if(s);       break;
        case StmtKind::While:   gen_while(s);    break;
        case StmtKind::For:     gen_for(s);      break;
        case StmtKind::VarDump: gen_var_dump(s); break;
        case StmtKind::Break:   emit("break;");  break;
    }
}

void CodeGen::gen_stmts(const std::vector<StmtPtr>& stmts) {
    for (const auto& s : stmts) gen_stmt(*s);
}

std::string CodeGen::generate(const std::vector<StmtPtr>& stmts) {
    out.str(""); out.clear(); indent_level = 0;
    out << RUNTIME << "\n";
    out << "int main() {\n";
    indent_level++;
    gen_stmts(stmts);
    emit("return 0;");
    indent_level--;
    out << "}\n";
    return out.str();
}
