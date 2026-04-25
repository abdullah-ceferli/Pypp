#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>
#include <variant>
#include <algorithm>
#include <functional>
#include <cmath>

// ============================================================
//  FORWARD DECLARATIONS
// ============================================================
struct PyPPValue;
using ValuePtr = std::shared_ptr<PyPPValue>;
using ObjMap   = std::map<std::string, ValuePtr>;
using ArrVec   = std::vector<ValuePtr>;

// ============================================================
//  VALUE TYPES
// ============================================================
enum class VType { INT, FLOAT, STR, ARRAY, OBJ, NIL };

struct PyPPValue {
    VType type;
    long long   ival = 0;
    double      fval = 0.0;
    std::string sval;
    ArrVec      arr;
    ObjMap      obj;

    PyPPValue() : type(VType::NIL) {}
    explicit PyPPValue(long long v)        : type(VType::INT),   ival(v) {}
    explicit PyPPValue(double v)           : type(VType::FLOAT), fval(v) {}
    explicit PyPPValue(const std::string&s): type(VType::STR),   sval(s) {}
    static ValuePtr makeArray() { auto v=std::make_shared<PyPPValue>(); v->type=VType::ARRAY; return v; }
    static ValuePtr makeObj()   { auto v=std::make_shared<PyPPValue>(); v->type=VType::OBJ;   return v; }
    static ValuePtr makeNil()   { return std::make_shared<PyPPValue>(); }

    std::string toString() const {
        switch(type){
            case VType::INT:   return std::to_string(ival);
            case VType::FLOAT:{
                std::ostringstream os; os<<fval; return os.str();
            }
            case VType::STR:   return sval;
            case VType::NIL:   return "Null";
            case VType::ARRAY:{
                std::string r="[";
                for(size_t i=0;i<arr.size();i++){ r+=arr[i]->toString(); if(i+1<arr.size()) r+=", "; }
                return r+"]";
            }
            case VType::OBJ:{
                std::string r="[";
                bool first=true;
                for(auto&[k,v]:obj){ if(!first)r+=", "; r+="\""+k+"\" => "+v->toString(); first=false; }
                return r+"]";
            }
        }
        return "?";
    }

    std::string varDumpStr(const std::string& label="", int depth=0) const {
        std::string indent(depth*2,' ');
        switch(type){
            case VType::INT:   return indent+"{int"+(label.empty()?"":"[\""+label+"\"]")+
                                      " => "+std::to_string(ival)+"}";
            case VType::FLOAT:{std::ostringstream os; os<<fval;
                               return indent+"{float"+(label.empty()?"":"[\""+label+"\"]")+
                                      " => "+os.str()+"}";}
            case VType::STR:   return indent+"{str"+(label.empty()?"":"[\""+label+"\"]")+
                                      " => \""+sval+"\"}";
            case VType::NIL:   return indent+"{Null"+(label.empty()?"":"[\""+label+"\"]")+"}";
            case VType::ARRAY:{
                std::string r=indent+"{array"+(label.empty()?"":"[\""+label+"\"]")+" => [\n";
                for(size_t i=0;i<arr.size();i++)
                    r+=arr[i]->varDumpStr(std::to_string(i),depth+1)+"\n";
                return r+indent+"]}";
            }
            case VType::OBJ:{
                std::string r=indent+"{obj"+(label.empty()?"":"[\""+label+"\"]")+" => [\n";
                for(auto&[k,v]:obj)
                    r+=v->varDumpStr(k,depth+1)+"\n";
                return r+indent+"]}";
            }
        }
        return indent+"{?}";
    }

    bool isTruthy() const {
        switch(type){
            case VType::INT:   return ival!=0;
            case VType::FLOAT: return fval!=0.0;
            case VType::STR:   return !sval.empty();
            case VType::ARRAY: return !arr.empty();
            case VType::OBJ:   return !obj.empty();
            case VType::NIL:   return false;
        }
        return false;
    }
};

// ============================================================
//  LEXER
// ============================================================
enum class TT {
    // literals
    INT_LIT, FLOAT_LIT, STR_LIT, NULL_LIT,
    // type keywords
    KW_INT, KW_FLOAT, KW_STR, KW_ARRAY, KW_OBJ,
    // statement keywords
    KW_SAY, KW_VAR_DUMPER,
    KW_IF, KW_ELSE, KW_IFELSE,
    KW_FOR, KW_IN, KW_WHILE, KW_DO,
    KW_NUMBER_LOOPER,
    // operators / punctuation
    IDENT,
    ASSIGN,       // =
    FAT_ARROW,    // =>
    PLUS, MINUS, STAR, SLASH, PERCENT,
    EQ, NEQ, LT, GT, LTE, GTE,
    AND, OR, NOT,
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,
    COMMA, SEMICOLON, COLON,
    PLUSPLUS, MINUSMINUS,
    DOT,
    END_OF_FILE
};

struct Token { TT type; std::string val; int line=1; };

static const std::map<std::string,TT> KEYWORDS = {
    {"int",TT::KW_INT},{"float",TT::KW_FLOAT},{"str",TT::KW_STR},
    {"array",TT::KW_ARRAY},{"obj",TT::KW_OBJ},
    {"say",TT::KW_SAY},{"var_dumper",TT::KW_VAR_DUMPER},
    {"if",TT::KW_IF},{"else",TT::KW_ELSE},{"ifelse",TT::KW_IFELSE},
    {"for",TT::KW_FOR},{"in",TT::KW_IN},{"while",TT::KW_WHILE},{"do",TT::KW_DO},
    {"numberLooper",TT::KW_NUMBER_LOOPER},
    {"Null",TT::NULL_LIT},
};

std::vector<Token> lex(const std::string& src){
    std::vector<Token> tokens;
    size_t i=0; int line=1;
    while(i<src.size()){
        // skip whitespace
        if(src[i]=='\n'){line++;i++;continue;}
        if(isspace(src[i])){i++;continue;}
        // comments //
        if(i+1<src.size()&&src[i]=='/'&&src[i+1]=='/'){
            while(i<src.size()&&src[i]!='\n') i++;
            continue;
        }
        // string literals
        if(src[i]=='"'||src[i]=='\''){
            char q=src[i++]; std::string s;
            while(i<src.size()&&src[i]!=q){
                if(src[i]=='\\'&&i+1<src.size()){i++;
                    if(src[i]=='n') s+='\n';
                    else if(src[i]=='t') s+='\t';
                    else s+=src[i];
                    i++;
                } else s+=src[i++];
            }
            if(i<src.size()) i++;
            tokens.push_back({TT::STR_LIT,s,line});
            continue;
        }
        // numbers
        if(isdigit(src[i])||( src[i]=='-'&&i+1<src.size()&&isdigit(src[i+1]) && (tokens.empty()||tokens.back().type==TT::ASSIGN||tokens.back().type==TT::FAT_ARROW||tokens.back().type==TT::LPAREN||tokens.back().type==TT::COMMA) )){
            std::string num; if(src[i]=='-') num+=src[i++];
            while(i<src.size()&&isdigit(src[i])) num+=src[i++];
            if(i<src.size()&&src[i]=='.'){
                num+=src[i++];
                while(i<src.size()&&isdigit(src[i])) num+=src[i++];
                tokens.push_back({TT::FLOAT_LIT,num,line});
            } else {
                tokens.push_back({TT::INT_LIT,num,line});
            }
            continue;
        }
        // identifiers / keywords
        if(isalpha(src[i])||src[i]=='_'){
            std::string id;
            while(i<src.size()&&(isalnum(src[i])||src[i]=='_')) id+=src[i++];
            auto it=KEYWORDS.find(id);
            tokens.push_back({it!=KEYWORDS.end()?it->second:TT::IDENT, id, line});
            continue;
        }
        // two-char operators
        if(i+1<src.size()){
            std::string two={src[i],src[i+1]};
            if(two=="=>"){tokens.push_back({TT::FAT_ARROW,"=>",line});i+=2;continue;}
            if(two=="=="){tokens.push_back({TT::EQ,"==",line});i+=2;continue;}
            if(two=="!="){tokens.push_back({TT::NEQ,"!=",line});i+=2;continue;}
            if(two=="<="){tokens.push_back({TT::LTE,"<=",line});i+=2;continue;}
            if(two==">="){tokens.push_back({TT::GTE,">=",line});i+=2;continue;}
            if(two=="&&"){tokens.push_back({TT::AND,"&&",line});i+=2;continue;}
            if(two=="||"){tokens.push_back({TT::OR,"||",line});i+=2;continue;}
            if(two=="++"){tokens.push_back({TT::PLUSPLUS,"++",line});i+=2;continue;}
            if(two=="--"){tokens.push_back({TT::MINUSMINUS,"--",line});i+=2;continue;}
        }
        // single-char
        switch(src[i]){
            case '=': tokens.push_back({TT::ASSIGN,"=",line});break;
            case '+': tokens.push_back({TT::PLUS,"+",line});break;
            case '-': tokens.push_back({TT::MINUS,"-",line});break;
            case '*': tokens.push_back({TT::STAR,"*",line});break;
            case '/': tokens.push_back({TT::SLASH,"/",line});break;
            case '%': tokens.push_back({TT::PERCENT,"%",line});break;
            case '<': tokens.push_back({TT::LT,"<",line});break;
            case '>': tokens.push_back({TT::GT,">",line});break;
            case '!': tokens.push_back({TT::NOT,"!",line});break;
            case '(': tokens.push_back({TT::LPAREN,"(",line});break;
            case ')': tokens.push_back({TT::RPAREN,")",line});break;
            case '{': tokens.push_back({TT::LBRACE,"{",line});break;
            case '}': tokens.push_back({TT::RBRACE,"}",line});break;
            case '[': tokens.push_back({TT::LBRACKET,"[",line});break;
            case ']': tokens.push_back({TT::RBRACKET,"]",line});break;
            case ',': tokens.push_back({TT::COMMA,",",line});break;
            case ';': tokens.push_back({TT::SEMICOLON,";",line});break;
            case ':': tokens.push_back({TT::COLON,":",line});break;
            case '.': tokens.push_back({TT::DOT,".",line});break;
            default: break; // skip unknown
        }
        i++;
    }
    tokens.push_back({TT::END_OF_FILE,"",line});
    return tokens;
}

// ============================================================
//  AST NODES
// ============================================================
struct ASTNode { virtual ~ASTNode()=default; virtual std::string kind()=0; };
using NodePtr = std::shared_ptr<ASTNode>;

struct ProgramNode    : ASTNode { std::vector<NodePtr> stmts; std::string kind()override{return"Program";} };
struct IntLitNode     : ASTNode { long long val; std::string kind()override{return"IntLit";} };
struct FloatLitNode   : ASTNode { double val; std::string kind()override{return"FloatLit";} };
struct StrLitNode     : ASTNode { std::string val; std::string kind()override{return"StrLit";} };
struct NullLitNode    : ASTNode { std::string kind()override{return"NullLit";} };
struct IdentNode      : ASTNode { std::string name; std::string kind()override{return"Ident";} };
struct ArrayLitNode   : ASTNode { std::vector<NodePtr> elems; std::string kind()override{return"ArrayLit";} };
struct ObjLitNode     : ASTNode { std::vector<std::pair<std::string,NodePtr>> pairs; std::string kind()override{return"ObjLit";} };
struct IndexNode      : ASTNode { NodePtr obj; NodePtr key; std::string kind()override{return"Index";} };
struct BinOpNode      : ASTNode { std::string op; NodePtr left,right; std::string kind()override{return"BinOp";} };
struct UnaryOpNode    : ASTNode { std::string op; NodePtr operand; std::string kind()override{return"UnaryOp";} };
struct PostfixNode    : ASTNode { std::string op; NodePtr operand; std::string kind()override{return"Postfix";} };
struct NumberLooperNode:ASTNode { NodePtr start,end_; std::string kind()override{return"NumberLooper";} };

// statements
struct VarDeclNode  : ASTNode { std::string dtype,name; NodePtr init; std::string kind()override{return"VarDecl";} };
struct AssignNode   : ASTNode { NodePtr target; NodePtr value; std::string kind()override{return"Assign";} };
struct SayNode      : ASTNode { NodePtr expr; std::string kind()override{return"Say";} };
struct VarDumperNode: ASTNode { NodePtr expr; std::string kind()override{return"VarDumper";} };
struct BlockNode    : ASTNode { std::vector<NodePtr> stmts; std::string kind()override{return"Block";} };
struct IfNode       : ASTNode { NodePtr cond; NodePtr then_; NodePtr else_; std::string kind()override{return"If";} };
struct ForInNode    : ASTNode { std::string var; NodePtr iter; NodePtr body; std::string kind()override{return"ForIn";} };
struct WhileNode    : ASTNode { NodePtr cond; NodePtr body; std::string kind()override{return"While";} };
struct ExprStmtNode : ASTNode { NodePtr expr; std::string kind()override{return"ExprStmt";} };

// ============================================================
//  PARSER
// ============================================================
struct Parser {
    std::vector<Token> toks;
    size_t pos=0;

    Token& cur(){ return toks[pos]; }
    Token& peek(int off=1){ size_t p=pos+off; if(p>=toks.size()) return toks.back(); return toks[p]; }
    Token consume(){ return toks[pos++]; }
    Token expect(TT t, const std::string& msg=""){
        if(cur().type!=t) throw std::runtime_error("Line "+std::to_string(cur().line)+": expected "+msg+" got '"+cur().val+"'");
        return consume();
    }
    bool check(TT t){ return cur().type==t; }
    bool match(TT t){ if(check(t)){consume();return true;} return false; }

    // ---- entry
    std::shared_ptr<ProgramNode> parse(){
        auto prog=std::make_shared<ProgramNode>();
        while(!check(TT::END_OF_FILE)) prog->stmts.push_back(parseStmt());
        return prog;
    }

    NodePtr parseStmt(){
        // var declarations
        if(check(TT::KW_INT)||check(TT::KW_FLOAT)||check(TT::KW_STR)||
           check(TT::KW_ARRAY)||check(TT::KW_OBJ)){
            return parseVarDecl();
        }
        if(check(TT::KW_SAY))     return parseSay();
        if(check(TT::KW_VAR_DUMPER)) return parseVarDumper();
        if(check(TT::KW_IF))      return parseIf();
        if(check(TT::KW_IFELSE))  return parseIfElse();
        if(check(TT::KW_FOR))     return parseFor();
        if(check(TT::KW_WHILE)||check(TT::KW_DO)) return parseWhile();
        if(check(TT::LBRACE))     return parseBlock();
        // assignment or expr-stmt
        return parseAssignOrExpr();
    }

    NodePtr parseVarDecl(){
        auto node=std::make_shared<VarDeclNode>();
        node->dtype=consume().val;  // consume type keyword
        node->name=expect(TT::IDENT,"identifier").val;
        NodePtr init;
        if(match(TT::ASSIGN)){
            init=parseExpr();
        } else {
            // default values
            if(node->dtype=="int")   { auto n=std::make_shared<IntLitNode>(); n->val=0; init=n; }
            else if(node->dtype=="float"){ auto n=std::make_shared<FloatLitNode>(); n->val=0.0; init=n; }
            else if(node->dtype=="str")  { auto n=std::make_shared<StrLitNode>(); init=n; }
            else init=std::make_shared<NullLitNode>();
        }
        node->init=init;
        match(TT::SEMICOLON);
        return node;
    }

    NodePtr parseSay(){
        consume(); // say
        auto node=std::make_shared<SayNode>();
        node->expr=parseExpr();
        match(TT::SEMICOLON);
        return node;
    }

    NodePtr parseVarDumper(){
        consume(); // var_dumper
        expect(TT::LPAREN,"(");
        auto node=std::make_shared<VarDumperNode>();
        node->expr=parseExpr();
        expect(TT::RPAREN,")");
        match(TT::SEMICOLON);
        return node;
    }

    NodePtr parseIf(){
        consume(); // if
        auto node=std::make_shared<IfNode>();
        node->cond=parseExpr();
        node->then_=parseBlock();
        if(check(TT::KW_IFELSE)){
            node->else_=parseIfElse();
        } else if(check(TT::KW_ELSE)){
            consume();
            node->else_=parseBlock();
        }
        return node;
    }

    NodePtr parseIfElse(){
        consume(); // ifelse
        auto node=std::make_shared<IfNode>();
        node->cond=parseExpr();
        node->then_=parseBlock();
        if(check(TT::KW_IFELSE)){
            node->else_=parseIfElse();
        } else if(check(TT::KW_ELSE)){
            consume();
            node->else_=parseBlock();
        }
        return node;
    }

    NodePtr parseFor(){
        consume(); // for
        auto node=std::make_shared<ForInNode>();
        node->var=expect(TT::IDENT,"variable").val;
        expect(TT::KW_IN,"in");
        node->iter=parseExpr();
        node->body=parseBlock();
        return node;
    }

    NodePtr parseWhile(){
        bool isDo=check(TT::KW_DO);
        consume(); // while or do
        if(isDo && check(TT::KW_WHILE)) consume(); // do while
        auto node=std::make_shared<WhileNode>();
        node->cond=parseExpr();
        node->body=parseBlock();
        return node;
    }

    std::shared_ptr<BlockNode> parseBlock(){
        expect(TT::LBRACE,"{");
        auto block=std::make_shared<BlockNode>();
        while(!check(TT::RBRACE)&&!check(TT::END_OF_FILE))
            block->stmts.push_back(parseStmt());
        expect(TT::RBRACE,"}");
        return block;
    }

    NodePtr parseAssignOrExpr(){
        auto left=parseExpr();
        if(check(TT::ASSIGN)){
            consume();
            auto val=parseExpr();
            auto node=std::make_shared<AssignNode>();
            node->target=left; node->value=val;
            match(TT::SEMICOLON);
            return node;
        }
        match(TT::SEMICOLON);
        auto es=std::make_shared<ExprStmtNode>(); es->expr=left; return es;
    }

    // ---- expressions (precedence climbing)
    NodePtr parseExpr(){ return parseOr(); }

    NodePtr parseOr(){
        auto left=parseAnd();
        while(check(TT::OR)){
            auto op=consume().val;
            auto right=parseAnd();
            auto node=std::make_shared<BinOpNode>();
            node->op=op; node->left=left; node->right=right;
            left=node;
        }
        return left;
    }
    NodePtr parseAnd(){
        auto left=parseEquality();
        while(check(TT::AND)){
            auto op=consume().val;
            auto right=parseEquality();
            auto node=std::make_shared<BinOpNode>();
            node->op=op; node->left=left; node->right=right;
            left=node;
        }
        return left;
    }
    NodePtr parseEquality(){
        auto left=parseRelational();
        while(check(TT::EQ)||check(TT::NEQ)){
            auto op=consume().val;
            auto right=parseRelational();
            auto node=std::make_shared<BinOpNode>();
            node->op=op; node->left=left; node->right=right;
            left=node;
        }
        return left;
    }
    NodePtr parseRelational(){
        auto left=parseAddSub();
        while(check(TT::LT)||check(TT::GT)||check(TT::LTE)||check(TT::GTE)){
            auto op=consume().val;
            auto right=parseAddSub();
            auto node=std::make_shared<BinOpNode>();
            node->op=op; node->left=left; node->right=right;
            left=node;
        }
        return left;
    }
    NodePtr parseAddSub(){
        auto left=parseMulDiv();
        while(check(TT::PLUS)||check(TT::MINUS)){
            auto op=consume().val;
            auto right=parseMulDiv();
            auto node=std::make_shared<BinOpNode>();
            node->op=op; node->left=left; node->right=right;
            left=node;
        }
        return left;
    }
    NodePtr parseMulDiv(){
        auto left=parseUnary();
        while(check(TT::STAR)||check(TT::SLASH)||check(TT::PERCENT)){
            auto op=consume().val;
            auto right=parseUnary();
            auto node=std::make_shared<BinOpNode>();
            node->op=op; node->left=left; node->right=right;
            left=node;
        }
        return left;
    }
    NodePtr parseUnary(){
        if(check(TT::NOT)){
            auto op=consume().val;
            auto node=std::make_shared<UnaryOpNode>();
            node->op=op; node->operand=parseUnary();
            return node;
        }
        if(check(TT::MINUS)){
            auto op=consume().val;
            auto node=std::make_shared<UnaryOpNode>();
            node->op=op; node->operand=parseUnary();
            return node;
        }
        return parsePostfix();
    }
    NodePtr parsePostfix(){
        auto node=parsePrimary();
        while(true){
            if(check(TT::LBRACKET)){
                consume();
                auto key=parseExpr();
                expect(TT::RBRACKET,"]");
                auto idx=std::make_shared<IndexNode>();
                idx->obj=node; idx->key=key;
                node=idx;
            } else if(check(TT::PLUSPLUS)||check(TT::MINUSMINUS)){
                auto op=consume().val;
                auto p=std::make_shared<PostfixNode>();
                p->op=op; p->operand=node;
                node=p;
            } else break;
        }
        return node;
    }
    NodePtr parsePrimary(){
        if(check(TT::INT_LIT)){
            auto node=std::make_shared<IntLitNode>();
            node->val=std::stoll(consume().val);
            return node;
        }
        if(check(TT::FLOAT_LIT)){
            auto node=std::make_shared<FloatLitNode>();
            node->val=std::stod(consume().val);
            return node;
        }
        if(check(TT::STR_LIT)){
            auto node=std::make_shared<StrLitNode>();
            node->val=consume().val;
            return node;
        }
        if(check(TT::NULL_LIT)){ consume(); return std::make_shared<NullLitNode>(); }
        if(check(TT::LBRACKET)) return parseArrayOrObj();
        if(check(TT::KW_NUMBER_LOOPER)) return parseNumberLooper();
        if(check(TT::IDENT)){
            auto node=std::make_shared<IdentNode>();
            node->name=consume().val;
            return node;
        }
        if(check(TT::LPAREN)){
            consume();
            auto e=parseExpr();
            expect(TT::RPAREN,")");
            return e;
        }
        throw std::runtime_error("Line "+std::to_string(cur().line)+": unexpected token '"+cur().val+"'");
    }
    NodePtr parseArrayOrObj(){
        expect(TT::LBRACKET,"[");
        // peek: if empty or starts with str => => it's obj
        // if starts with STR_LIT followed by FAT_ARROW => obj
        bool isObj=false;
        if(check(TT::RBRACKET)){ consume(); return std::make_shared<ArrayLitNode>(); }
        // look ahead: type keyword "str"/"int"/"float" followed by string/ident then =>
        size_t saved=pos;
        // Check for obj literal: we expect pattern like  str "key" => val
        if(check(TT::KW_STR)||check(TT::KW_INT)||check(TT::KW_FLOAT)||check(TT::KW_ARRAY)||check(TT::KW_OBJ)){
            // skip type, skip key
            pos++; // skip type
            if(check(TT::STR_LIT)||check(TT::IDENT)) pos++;
            if(check(TT::FAT_ARROW)) isObj=true;
            pos=saved;
        }
        if(isObj) return parseObjBody();
        // array
        auto arr=std::make_shared<ArrayLitNode>();
        while(!check(TT::RBRACKET)&&!check(TT::END_OF_FILE)){
            arr->elems.push_back(parseExpr());
            if(!match(TT::COMMA)) break;
        }
        expect(TT::RBRACKET,"]");
        return arr;
    }
    NodePtr parseObjBody(){
        auto obj=std::make_shared<ObjLitNode>();
        while(!check(TT::RBRACKET)&&!check(TT::END_OF_FILE)){
            // optional type hint
            if(check(TT::KW_STR)||check(TT::KW_INT)||check(TT::KW_FLOAT)||
               check(TT::KW_ARRAY)||check(TT::KW_OBJ)) consume(); // skip type hint
            std::string key;
            if(check(TT::STR_LIT)) key=consume().val;
            else key=expect(TT::IDENT,"key").val;
            expect(TT::FAT_ARROW,"=>");
            auto val=parseExpr();
            obj->pairs.push_back({key,val});
            if(!match(TT::COMMA)) break;
        }
        expect(TT::RBRACKET,"]");
        return obj;
    }
    NodePtr parseNumberLooper(){
        consume(); // numberLooper
        expect(TT::LPAREN,"(");
        auto node=std::make_shared<NumberLooperNode>();
        node->start=parseExpr();
        expect(TT::COMMA,",");
        node->end_=parseExpr();
        expect(TT::RPAREN,")");
        return node;
    }
};

// ============================================================
//  INTERPRETER / ENVIRONMENT
// ============================================================
struct Environment {
    std::map<std::string,ValuePtr> vars;
    std::shared_ptr<Environment> parent;
    Environment(std::shared_ptr<Environment> p=nullptr):parent(p){}

    void set(const std::string& name, ValuePtr val){
        // check if exists in any scope
        Environment* e=this;
        while(e){ if(e->vars.count(name)){e->vars[name]=val; return;} e=e->parent.get(); }
        vars[name]=val;
    }
    ValuePtr get(const std::string& name){
        Environment* e=this;
        while(e){ auto it=e->vars.find(name); if(it!=e->vars.end()) return it->second; e=e->parent.get(); }
        throw std::runtime_error("Undefined variable '"+name+"'");
    }
    bool has(const std::string& name){
        Environment* e=this;
        while(e){ if(e->vars.count(name)) return true; e=e->parent.get(); }
        return false;
    }
    void define(const std::string& name, ValuePtr val){ vars[name]=val; }
};

struct Interpreter {
    std::shared_ptr<Environment> globalEnv=std::make_shared<Environment>();

    void run(std::shared_ptr<ProgramNode> prog){
        for(auto& s: prog->stmts) execStmt(s, globalEnv);
    }

    void execStmt(NodePtr node, std::shared_ptr<Environment> env){
        auto k=node->kind();
        if(k=="VarDecl"){
            auto n=std::static_pointer_cast<VarDeclNode>(node);
            auto val=evalExpr(n->init,env);
            env->define(n->name,val);
        } else if(k=="Assign"){
            auto n=std::static_pointer_cast<AssignNode>(node);
            setValue(n->target, evalExpr(n->value,env), env);
        } else if(k=="Say"){
            auto n=std::static_pointer_cast<SayNode>(node);
            auto val=evalExpr(n->expr,env);
            std::cout<<val->toString()<<"\n";
        } else if(k=="VarDumper"){
            auto n=std::static_pointer_cast<VarDumperNode>(node);
            auto val=evalExpr(n->expr,env);
            std::cout<<val->varDumpStr()<<"\n";
        } else if(k=="If"){
            auto n=std::static_pointer_cast<IfNode>(node);
            auto cond=evalExpr(n->cond,env);
            if(cond->isTruthy()){
                auto child=std::make_shared<Environment>(env);
                execStmt(n->then_,child);
            } else if(n->else_){
                auto child=std::make_shared<Environment>(env);
                execStmt(n->else_,child);
            }
        } else if(k=="Block"){
            auto n=std::static_pointer_cast<BlockNode>(node);
            for(auto& s:n->stmts) execStmt(s,env);
        } else if(k=="ForIn"){
            auto n=std::static_pointer_cast<ForInNode>(node);
            auto iter=evalExpr(n->iter,env);
            auto child=std::make_shared<Environment>(env);
            if(iter->type==VType::ARRAY){
                for(auto& v:iter->arr){
                    child->define(n->var,v);
                    execStmt(n->body,child);
                }
            } else if(iter->type==VType::OBJ){
                // iterate over values — treat like array of values
                for(auto&[key,v]:iter->obj){
                    // expose both key and var
                    child->define(n->var, std::make_shared<PyPPValue>(key));
                    execStmt(n->body,child);
                }
            } else {
                throw std::runtime_error("for..in requires array or obj");
            }
        } else if(k=="While"){
            auto n=std::static_pointer_cast<WhileNode>(node);
            auto child=std::make_shared<Environment>(env);
            while(evalExpr(n->cond,child)->isTruthy()){
                execStmt(n->body,child);
            }
        } else if(k=="ExprStmt"){
            auto n=std::static_pointer_cast<ExprStmtNode>(node);
            evalExpr(n->expr,env);
        }
    }

    void setValue(NodePtr target, ValuePtr val, std::shared_ptr<Environment> env){
        if(target->kind()=="Ident"){
            auto n=std::static_pointer_cast<IdentNode>(target);
            env->set(n->name,val);
        } else if(target->kind()=="Index"){
            auto n=std::static_pointer_cast<IndexNode>(target);
            auto obj=evalExpr(n->obj,env);
            auto key=evalExpr(n->key,env);
            if(obj->type==VType::ARRAY){
                if(key->type!=VType::INT) throw std::runtime_error("Array index must be int");
                size_t idx=(size_t)key->ival;
                while(obj->arr.size()<=idx) obj->arr.push_back(PyPPValue::makeNil());
                obj->arr[idx]=val;
            } else if(obj->type==VType::OBJ){
                obj->obj[key->toString()]=val;
            }
        }
    }

    ValuePtr evalExpr(NodePtr node, std::shared_ptr<Environment> env){
        auto k=node->kind();
        if(k=="IntLit")   return std::make_shared<PyPPValue>(std::static_pointer_cast<IntLitNode>(node)->val);
        if(k=="FloatLit") return std::make_shared<PyPPValue>(std::static_pointer_cast<FloatLitNode>(node)->val);
        if(k=="StrLit")   return std::make_shared<PyPPValue>(std::static_pointer_cast<StrLitNode>(node)->val);
        if(k=="NullLit")  return PyPPValue::makeNil();
        if(k=="Ident"){
            auto n=std::static_pointer_cast<IdentNode>(node);
            return env->get(n->name);
        }
        if(k=="ArrayLit"){
            auto n=std::static_pointer_cast<ArrayLitNode>(node);
            auto arr=PyPPValue::makeArray();
            for(auto& e:n->elems) arr->arr.push_back(evalExpr(e,env));
            return arr;
        }
        if(k=="ObjLit"){
            auto n=std::static_pointer_cast<ObjLitNode>(node);
            auto obj=PyPPValue::makeObj();
            for(auto&[key,val]:n->pairs) obj->obj[key]=evalExpr(val,env);
            return obj;
        }
        if(k=="Index"){
            auto n=std::static_pointer_cast<IndexNode>(node);
            auto obj=evalExpr(n->obj,env);
            auto key=evalExpr(n->key,env);
            if(obj->type==VType::ARRAY){
                if(key->type!=VType::INT) throw std::runtime_error("Array index must be int");
                size_t idx=(size_t)key->ival;
                if(idx>=obj->arr.size()) return PyPPValue::makeNil();
                return obj->arr[idx];
            } else if(obj->type==VType::OBJ){
                auto it=obj->obj.find(key->toString());
                if(it==obj->obj.end()) return PyPPValue::makeNil();
                return it->second;
            }
            throw std::runtime_error("Cannot index non-array/obj");
        }
        if(k=="NumberLooper"){
            auto n=std::static_pointer_cast<NumberLooperNode>(node);
            auto s=evalExpr(n->start,env);
            auto e=evalExpr(n->end_,env);
            auto arr=PyPPValue::makeArray();
            long long from=s->ival, to=e->ival;
            for(long long i=from;i<to;i++)
                arr->arr.push_back(std::make_shared<PyPPValue>(i));
            return arr;
        }
        if(k=="BinOp"){
            auto n=std::static_pointer_cast<BinOpNode>(node);
            auto L=evalExpr(n->left,env);
            auto R=evalExpr(n->right,env);
            return applyBinOp(n->op,L,R);
        }
        if(k=="UnaryOp"){
            auto n=std::static_pointer_cast<UnaryOpNode>(node);
            auto v=evalExpr(n->operand,env);
            if(n->op=="!") return std::make_shared<PyPPValue>((long long)(!v->isTruthy()));
            if(n->op=="-"){
                if(v->type==VType::INT)   return std::make_shared<PyPPValue>(-v->ival);
                if(v->type==VType::FLOAT) return std::make_shared<PyPPValue>(-v->fval);
            }
            return v;
        }
        if(k=="Postfix"){
            auto n=std::static_pointer_cast<PostfixNode>(node);
            auto v=evalExpr(n->operand,env);
            ValuePtr newv;
            if(v->type==VType::INT){
                newv=std::make_shared<PyPPValue>(v->ival+(n->op=="++"?1:-1));
            } else if(v->type==VType::FLOAT){
                newv=std::make_shared<PyPPValue>(v->fval+(n->op=="++"?1.0:-1.0));
            } else newv=v;
            setValue(n->operand,newv,env);
            return v; // return old value
        }
        throw std::runtime_error("Unknown expr node: "+k);
    }

    ValuePtr applyBinOp(const std::string& op, ValuePtr L, ValuePtr R){
        // string concat with +
        if(op=="+"&&(L->type==VType::STR||R->type==VType::STR)){
            return std::make_shared<PyPPValue>(L->toString()+R->toString());
        }
        // numeric ops
        bool isFloat=(L->type==VType::FLOAT||R->type==VType::FLOAT);
        double lf= L->type==VType::INT?(double)L->ival:L->fval;
        double rf= R->type==VType::INT?(double)R->ival:R->fval;
        long long li=L->type==VType::INT?L->ival:(long long)L->fval;
        long long ri=R->type==VType::INT?R->ival:(long long)R->fval;

        if(op=="+"){ return isFloat?std::make_shared<PyPPValue>(lf+rf):std::make_shared<PyPPValue>(li+ri); }
        if(op=="-"){ return isFloat?std::make_shared<PyPPValue>(lf-rf):std::make_shared<PyPPValue>(li-ri); }
        if(op=="*"){ return isFloat?std::make_shared<PyPPValue>(lf*rf):std::make_shared<PyPPValue>(li*ri); }
        if(op=="/"){
            if(rf==0) throw std::runtime_error("Division by zero");
            return isFloat?std::make_shared<PyPPValue>(lf/rf):std::make_shared<PyPPValue>(li/ri);
        }
        if(op=="%"){ return std::make_shared<PyPPValue>(li%ri); }
        // comparisons
        if(op=="=="){ return std::make_shared<PyPPValue>((long long)(lf==rf)); }
        if(op=="!="){ return std::make_shared<PyPPValue>((long long)(lf!=rf)); }
        if(op=="<") { return std::make_shared<PyPPValue>((long long)(lf<rf)); }
        if(op==">") { return std::make_shared<PyPPValue>((long long)(lf>rf)); }
        if(op=="<="){ return std::make_shared<PyPPValue>((long long)(lf<=rf)); }
        if(op==">="){ return std::make_shared<PyPPValue>((long long)(lf>=rf)); }
        if(op=="&&"){ return std::make_shared<PyPPValue>((long long)(L->isTruthy()&&R->isTruthy())); }
        if(op=="||"){ return std::make_shared<PyPPValue>((long long)(L->isTruthy()||R->isTruthy())); }
        throw std::runtime_error("Unknown operator: "+op);
    }
};

// ============================================================
//  MAIN
// ============================================================
int main(int argc, char* argv[]){
    if(argc<2){
        std::cout<<"PyPP Interpreter v1.0\n";
        std::cout<<"Usage: pypp <file.pypp>\n";
        return 1;
    }
    std::ifstream f(argv[1]);
    if(!f){ std::cerr<<"Error: cannot open file '"<<argv[1]<<"'\n"; return 1; }
    std::ostringstream ss; ss<<f.rdbuf();
    std::string src=ss.str();

    try {
        auto tokens=lex(src);
        Parser parser; parser.toks=tokens;
        auto ast=parser.parse();
        Interpreter interp;
        interp.run(ast);
    } catch(const std::exception& e){
        std::cerr<<"[PyPP Error] "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}