/*  PyPP Interpreter  v2.0
    Features:
      data types : int  float  str  bool  array  obj  Null
      say  with r"...\{var}" string interpolation
      var_dumper(x)  structured dump
      dataType(x)    returns type name as str
      if / ifelse / else   with  && ||  !  (also  and  or  not)
      for i in numberLooper(start,end){}  /  for i in list{}
      while cond {}  /  do while cond {}
      i++  i--   **  (power operator, right-associative)
      obj type-hint enforcement: str "key" => value
      obj x = ["plain"]  gives parse error
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────
//  VALUE
// ─────────────────────────────────────────────
enum class VType { INT, FLOAT, STR, BOOL, ARRAY, OBJ, NIL };

struct PyPPValue;
using ValuePtr = std::shared_ptr<PyPPValue>;
using ObjMap   = std::map<std::string,ValuePtr>;
using ArrVec   = std::vector<ValuePtr>;

struct PyPPValue {
    VType       type = VType::NIL;
    long long   ival = 0;
    double      fval = 0.0;
    std::string sval;
    bool        bval = false;
    ArrVec      arr;
    ObjMap      obj;

    PyPPValue(){}
    explicit PyPPValue(long long v)          : type(VType::INT),   ival(v){}
    explicit PyPPValue(double    v)          : type(VType::FLOAT), fval(v){}
    explicit PyPPValue(const std::string& s) : type(VType::STR),   sval(s){}
    explicit PyPPValue(bool b)               : type(VType::BOOL),  bval(b){}

    static ValuePtr makeArray(){ auto v=std::make_shared<PyPPValue>(); v->type=VType::ARRAY; return v; }
    static ValuePtr makeObj()  { auto v=std::make_shared<PyPPValue>(); v->type=VType::OBJ;   return v; }
    static ValuePtr makeNil()  { return std::make_shared<PyPPValue>(); }

    std::string typeName() const {
        switch(type){
            case VType::INT:   return "int";
            case VType::FLOAT: return "float";
            case VType::STR:   return "str";
            case VType::BOOL:  return "bool";
            case VType::ARRAY: return "array";
            case VType::OBJ:   return "obj";
            case VType::NIL:   return "Null";
        }
        return "?";
    }

    std::string toString() const {
        switch(type){
            case VType::INT:   return std::to_string(ival);
            case VType::FLOAT: { std::ostringstream o; o<<fval; return o.str(); }
            case VType::STR:   return sval;
            case VType::BOOL:  return bval ? "true" : "false";
            case VType::NIL:   return "Null";
            case VType::ARRAY: {
                std::string r="[";
                for(size_t i=0;i<arr.size();i++){ r+=arr[i]->toString(); if(i+1<arr.size()) r+=", "; }
                return r+"]";
            }
            case VType::OBJ: {
                std::string r="["; bool first=true;
                for(auto& p:obj){ if(!first)r+=", "; r+="\""+p.first+"\" => "+p.second->toString(); first=false; }
                return r+"]";
            }
        }
        return "?";
    }

    std::string varDump(const std::string& label="", int depth=0) const {
        std::string ind(depth*2,' ');
        std::string lbl = label.empty() ? "" : "[\""+label+"\"]";
        switch(type){
            case VType::INT:   return ind+"{int"+lbl+" => "+std::to_string(ival)+"}";
            case VType::FLOAT: { std::ostringstream o; o<<fval; return ind+"{float"+lbl+" => "+o.str()+"}"; }
            case VType::STR:   return ind+"{str"+lbl+" => \""+sval+"\"}";
            case VType::BOOL:  return ind+"{bool"+lbl+" => "+(bval?"true":"false")+"}";
            case VType::NIL:   return ind+"{Null"+lbl+"}";
            case VType::ARRAY: {
                std::string r=ind+"{array"+lbl+" => [\n";
                for(size_t i=0;i<arr.size();i++) r+=arr[i]->varDump(std::to_string(i),depth+1)+"\n";
                return r+ind+"]}";
            }
            case VType::OBJ: {
                std::string r=ind+"{obj"+lbl+" => [\n";
                for(auto& p:obj) r+=p.second->varDump(p.first,depth+1)+"\n";
                return r+ind+"]}";
            }
        }
        return ind+"{?}";
    }

    bool isTruthy() const {
        switch(type){
            case VType::INT:   return ival!=0;
            case VType::FLOAT: return fval!=0.0;
            case VType::STR:   return !sval.empty();
            case VType::BOOL:  return bval;
            case VType::ARRAY: return !arr.empty();
            case VType::OBJ:   return !obj.empty();
            case VType::NIL:   return false;
        }
        return false;
    }
};

// ─────────────────────────────────────────────
//  LEXER
// ─────────────────────────────────────────────
enum class TT {
    INT_LIT, FLOAT_LIT, STR_LIT, BOOL_LIT, NULL_LIT,
    KW_INT, KW_FLOAT, KW_STR, KW_BOOL, KW_ARRAY, KW_OBJ,
    KW_SAY, KW_VAR_DUMPER, KW_DATA_TYPE,
    KW_IF, KW_ELSE, KW_IFELSE,
    KW_FOR, KW_IN, KW_WHILE, KW_DO,
    KW_NUMBER_LOOPER,
    KW_AND, KW_OR, KW_NOT,
    IDENT,
    ASSIGN, FAT_ARROW,
    PLUS, MINUS, STAR, SLASH, PERCENT, POWER,
    EQ, NEQ, LT, GT, LTE, GTE,
    AND, OR, NOT,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    COMMA, SEMICOLON,
    PLUSPLUS, MINUSMINUS,
    END_OF_FILE
};

struct Token { TT type; std::string val; int line=1; };

static const std::map<std::string,TT> KEYWORDS = {
    {"int",TT::KW_INT},{"float",TT::KW_FLOAT},{"str",TT::KW_STR},
    {"bool",TT::KW_BOOL},{"array",TT::KW_ARRAY},{"obj",TT::KW_OBJ},
    {"say",TT::KW_SAY},{"var_dumper",TT::KW_VAR_DUMPER},{"dataType",TT::KW_DATA_TYPE},
    {"if",TT::KW_IF},{"else",TT::KW_ELSE},{"ifelse",TT::KW_IFELSE},
    {"for",TT::KW_FOR},{"in",TT::KW_IN},{"while",TT::KW_WHILE},{"do",TT::KW_DO},
    {"numberLooper",TT::KW_NUMBER_LOOPER},
    {"and",TT::KW_AND},{"or",TT::KW_OR},{"not",TT::KW_NOT},
    {"true",TT::BOOL_LIT},{"false",TT::BOOL_LIT},
    {"Null",TT::NULL_LIT},
};

// Parse r"text \{varName} more" — returns raw string with \x01varName\x02 placeholders
static std::string lexRString(const std::string& src, size_t& i){
    std::string result;
    while(i<src.size() && src[i]!='"'){
        if(src[i]=='\\' && i+1<src.size() && src[i+1]=='{'){
            i+=2;
            std::string varExpr;
            while(i<src.size() && src[i]!='}') varExpr+=src[i++];
            if(i<src.size()) i++;
            result+='\x01'; result+=varExpr; result+='\x02';
        } else if(src[i]=='\\' && i+1<src.size()){
            i++;
            if(src[i]=='n') result+='\n';
            else if(src[i]=='t') result+='\t';
            else result+=src[i];
            i++;
        } else {
            result+=src[i++];
        }
    }
    if(i<src.size()) i++;
    return result;
}

std::vector<Token> lex(const std::string& src){
    std::vector<Token> tokens;
    size_t i=0; int line=1;
    while(i<src.size()){
        if(src[i]=='\n'){line++;i++;continue;}
        if(isspace((unsigned char)src[i])){i++;continue;}
        // line comment
        if(i+1<src.size()&&src[i]=='/'&&src[i+1]=='/'){
            while(i<src.size()&&src[i]!='\n') i++;
            continue;
        }
        // r"…" interpolated string
        if(src[i]=='r' && i+1<src.size() && src[i+1]=='"'){
            i+=2;
            std::string s=lexRString(src,i);
            // prefix with r + NUL so evaluator knows it's interpolated
            tokens.push_back({TT::STR_LIT, std::string("r\0",2)+s, line});
            continue;
        }
        // regular string
        if(src[i]=='"'||src[i]=='\''){
            char q=src[i++]; std::string s;
            while(i<src.size()&&src[i]!=q){
                if(src[i]=='\\'&&i+1<src.size()){
                    i++;
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
        bool negOK = tokens.empty() ||
            tokens.back().type==TT::ASSIGN ||
            tokens.back().type==TT::FAT_ARROW ||
            tokens.back().type==TT::LPAREN ||
            tokens.back().type==TT::COMMA ||
            tokens.back().type==TT::LBRACKET;
        if(isdigit((unsigned char)src[i]) ||
           (src[i]=='-' && negOK && i+1<src.size() && isdigit((unsigned char)src[i+1]))){
            std::string num; if(src[i]=='-') num+=src[i++];
            while(i<src.size()&&isdigit((unsigned char)src[i])) num+=src[i++];
            if(i<src.size()&&src[i]=='.'){
                num+=src[i++];
                while(i<src.size()&&isdigit((unsigned char)src[i])) num+=src[i++];
                tokens.push_back({TT::FLOAT_LIT,num,line});
            } else {
                tokens.push_back({TT::INT_LIT,num,line});
            }
            continue;
        }
        // identifiers / keywords
        if(isalpha((unsigned char)src[i])||src[i]=='_'){
            std::string id;
            while(i<src.size()&&(isalnum((unsigned char)src[i])||src[i]=='_')) id+=src[i++];
            auto it=KEYWORDS.find(id);
            tokens.push_back({it!=KEYWORDS.end()?it->second:TT::IDENT, id, line});
            continue;
        }
        // two-char
        if(i+1<src.size()){
            std::string two={src[i],src[i+1]};
            if(two=="**"){tokens.push_back({TT::POWER,"**",line});i+=2;continue;}
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
            case '=': tokens.push_back({TT::ASSIGN,"=",line});   break;
            case '+': tokens.push_back({TT::PLUS,"+",line});     break;
            case '-': tokens.push_back({TT::MINUS,"-",line});    break;
            case '*': tokens.push_back({TT::STAR,"*",line});     break;
            case '/': tokens.push_back({TT::SLASH,"/",line});    break;
            case '%': tokens.push_back({TT::PERCENT,"%",line});  break;
            case '<': tokens.push_back({TT::LT,"<",line});       break;
            case '>': tokens.push_back({TT::GT,">",line});       break;
            case '!': tokens.push_back({TT::NOT,"!",line});      break;
            case '(': tokens.push_back({TT::LPAREN,"(",line});   break;
            case ')': tokens.push_back({TT::RPAREN,")",line});   break;
            case '{': tokens.push_back({TT::LBRACE,"{",line});   break;
            case '}': tokens.push_back({TT::RBRACE,"}",line});   break;
            case '[': tokens.push_back({TT::LBRACKET,"[",line}); break;
            case ']': tokens.push_back({TT::RBRACKET,"]",line}); break;
            case ',': tokens.push_back({TT::COMMA,",",line});    break;
            case ';': tokens.push_back({TT::SEMICOLON,";",line});break;
            default:  break;
        }
        i++;
    }
    tokens.push_back({TT::END_OF_FILE,"",line});
    return tokens;
}

// ─────────────────────────────────────────────
//  AST NODES
// ─────────────────────────────────────────────
struct ASTNode { virtual ~ASTNode()=default; virtual std::string kind()=0; };
using NodePtr = std::shared_ptr<ASTNode>;

struct ProgramNode      : ASTNode { std::vector<NodePtr> stmts; std::string kind()override{return"Program";} };
struct IntLitNode       : ASTNode { long long val=0; std::string kind()override{return"IntLit";} };
struct FloatLitNode     : ASTNode { double val=0;    std::string kind()override{return"FloatLit";} };
struct StrLitNode       : ASTNode { std::string val; bool interp=false; std::string kind()override{return"StrLit";} };
struct BoolLitNode      : ASTNode { bool val=false;  std::string kind()override{return"BoolLit";} };
struct NullLitNode      : ASTNode { std::string kind()override{return"NullLit";} };
struct IdentNode        : ASTNode { std::string name; std::string kind()override{return"Ident";} };
struct ArrayLitNode     : ASTNode { std::vector<NodePtr> elems; std::string kind()override{return"ArrayLit";} };
// ObjLitNode pairs = (key, declaredType, valueNode)
struct ObjLitNode : ASTNode {
    std::vector<std::tuple<std::string,std::string,NodePtr>> pairs;
    std::string kind()override{return"ObjLit";}
};
struct IndexNode        : ASTNode { NodePtr obj,key; std::string kind()override{return"Index";} };
struct BinOpNode        : ASTNode { std::string op; NodePtr left,right; std::string kind()override{return"BinOp";} };
struct UnaryOpNode      : ASTNode { std::string op; NodePtr operand; std::string kind()override{return"UnaryOp";} };
struct PostfixNode      : ASTNode { std::string op; NodePtr operand; std::string kind()override{return"Postfix";} };
struct NumberLooperNode : ASTNode { NodePtr start,end_; std::string kind()override{return"NumberLooper";} };
struct DataTypeNode     : ASTNode { NodePtr expr; std::string kind()override{return"DataType";} };

struct VarDeclNode  : ASTNode { std::string dtype,name; NodePtr init; std::string kind()override{return"VarDecl";} };
struct AssignNode   : ASTNode { NodePtr target,value; std::string kind()override{return"Assign";} };
struct SayNode      : ASTNode { NodePtr expr; std::string kind()override{return"Say";} };
struct VarDumperNode: ASTNode { NodePtr expr; std::string kind()override{return"VarDumper";} };
struct BlockNode    : ASTNode { std::vector<NodePtr> stmts; std::string kind()override{return"Block";} };
struct IfNode       : ASTNode { NodePtr cond,then_,else_; std::string kind()override{return"If";} };
struct ForInNode    : ASTNode { std::string var; NodePtr iter,body; std::string kind()override{return"ForIn";} };
struct WhileNode    : ASTNode { NodePtr cond,body; std::string kind()override{return"While";} };
struct ExprStmtNode : ASTNode { NodePtr expr; std::string kind()override{return"ExprStmt";} };

// ─────────────────────────────────────────────
//  PARSER
// ─────────────────────────────────────────────
struct Parser {
    std::vector<Token> toks;
    size_t pos=0;

    Token& cur(){ return toks[pos]; }
    Token consume(){ return toks[pos++]; }
    Token expect(TT t,const std::string& msg){
        if(cur().type!=t)
            throw std::runtime_error("Line "+std::to_string(cur().line)+
                ": expected "+msg+", got '"+cur().val+"'");
        return consume();
    }
    bool check(TT t){ return cur().type==t; }
    bool match(TT t){ if(check(t)){consume();return true;} return false; }

    bool isTypeKw(){
        return check(TT::KW_INT)||check(TT::KW_FLOAT)||check(TT::KW_STR)||
               check(TT::KW_BOOL)||check(TT::KW_ARRAY)||check(TT::KW_OBJ);
    }

    std::shared_ptr<ProgramNode> parse(){
        auto p=std::make_shared<ProgramNode>();
        while(!check(TT::END_OF_FILE)) p->stmts.push_back(parseStmt());
        return p;
    }

    NodePtr parseStmt(){
        if(isTypeKw())               return parseVarDecl();
        if(check(TT::KW_SAY))        return parseSay();
        if(check(TT::KW_VAR_DUMPER)) return parseVarDumper();
        if(check(TT::KW_IF))         return parseIf();
        if(check(TT::KW_IFELSE))     return parseIfElse();
        if(check(TT::KW_FOR))        return parseFor();
        if(check(TT::KW_WHILE)||check(TT::KW_DO)) return parseWhile();
        if(check(TT::LBRACE))        return parseBlock();
        return parseAssignOrExpr();
    }

    NodePtr parseVarDecl(){
        int declLine=cur().line;
        std::string dtype=consume().val;
        std::string name =expect(TT::IDENT,"identifier").val;
        NodePtr init;
        if(match(TT::ASSIGN)){
            init=parseExpr();
            // parse-time structural checks
            if(dtype=="obj" && init->kind()=="ArrayLit")
                throw std::runtime_error(
                    "Line "+std::to_string(declLine)+
                    ": Type error: '"+name+"' is declared as obj but you gave it a plain array [ ].\n"
                    "  Obj fields need type + key => value, e.g.:  [ str \"name\" => \"Alice\" ]");
            if(dtype=="array" && init->kind()=="ObjLit")
                throw std::runtime_error(
                    "Line "+std::to_string(declLine)+
                    ": Type error: '"+name+"' is declared as array but you gave it an obj literal.");
        } else {
            if(dtype=="int")  { auto n=std::make_shared<IntLitNode>();   n->val=0;     init=n; }
            else if(dtype=="float"){ auto n=std::make_shared<FloatLitNode>(); n->val=0.0;  init=n; }
            else if(dtype=="str")  { auto n=std::make_shared<StrLitNode>();               init=n; }
            else if(dtype=="bool") { auto n=std::make_shared<BoolLitNode>(); n->val=false; init=n; }
            else init=std::make_shared<NullLitNode>();
        }
        match(TT::SEMICOLON);
        auto n=std::make_shared<VarDeclNode>(); n->dtype=dtype; n->name=name; n->init=init;
        return n;
    }

    NodePtr parseSay(){
        consume();
        auto n=std::make_shared<SayNode>(); n->expr=parseExpr();
        match(TT::SEMICOLON); return n;
    }

    NodePtr parseVarDumper(){
        consume();
        expect(TT::LPAREN,"(");
        auto n=std::make_shared<VarDumperNode>(); n->expr=parseExpr();
        expect(TT::RPAREN,")");
        match(TT::SEMICOLON); return n;
    }

    NodePtr parseIf(){
        consume(); // if
        auto n=std::make_shared<IfNode>(); n->cond=parseExpr(); n->then_=parseBlock();
        if(check(TT::KW_IFELSE))      n->else_=parseIfElse();
        else if(check(TT::KW_ELSE)){ consume(); n->else_=parseBlock(); }
        return n;
    }
    NodePtr parseIfElse(){
        consume(); // ifelse
        auto n=std::make_shared<IfNode>(); n->cond=parseExpr(); n->then_=parseBlock();
        if(check(TT::KW_IFELSE))      n->else_=parseIfElse();
        else if(check(TT::KW_ELSE)){ consume(); n->else_=parseBlock(); }
        return n;
    }

    NodePtr parseFor(){
        consume();
        auto n=std::make_shared<ForInNode>();
        n->var=expect(TT::IDENT,"variable").val;
        expect(TT::KW_IN,"in");
        n->iter=parseExpr(); n->body=parseBlock();
        return n;
    }

    NodePtr parseWhile(){
        bool isDo=check(TT::KW_DO); consume();
        if(isDo&&check(TT::KW_WHILE)) consume();
        auto n=std::make_shared<WhileNode>(); n->cond=parseExpr(); n->body=parseBlock();
        return n;
    }

    std::shared_ptr<BlockNode> parseBlock(){
        expect(TT::LBRACE,"{");
        auto b=std::make_shared<BlockNode>();
        while(!check(TT::RBRACE)&&!check(TT::END_OF_FILE)) b->stmts.push_back(parseStmt());
        expect(TT::RBRACE,"}");
        return b;
    }

    NodePtr parseAssignOrExpr(){
        auto left=parseExpr();
        if(check(TT::ASSIGN)){
            consume();
            auto n=std::make_shared<AssignNode>(); n->target=left; n->value=parseExpr();
            match(TT::SEMICOLON); return n;
        }
        match(TT::SEMICOLON);
        auto n=std::make_shared<ExprStmtNode>(); n->expr=left; return n;
    }

    // expressions
    NodePtr parseExpr(){ return parseOr(); }

    NodePtr parseOr(){
        auto left=parseAnd();
        while(check(TT::OR)||check(TT::KW_OR)){
            consume();
            auto r=parseAnd();
            auto n=std::make_shared<BinOpNode>(); n->op="||"; n->left=left; n->right=r; left=n;
        }
        return left;
    }
    NodePtr parseAnd(){
        auto left=parseNot();
        while(check(TT::AND)||check(TT::KW_AND)){
            consume();
            auto r=parseNot();
            auto n=std::make_shared<BinOpNode>(); n->op="&&"; n->left=left; n->right=r; left=n;
        }
        return left;
    }
    NodePtr parseNot(){
        if(check(TT::NOT)||check(TT::KW_NOT)){
            consume();
            auto n=std::make_shared<UnaryOpNode>(); n->op="!"; n->operand=parseNot(); return n;
        }
        return parseEquality();
    }
    NodePtr parseEquality(){
        auto left=parseRelational();
        while(check(TT::EQ)||check(TT::NEQ)){
            auto op=consume().val; auto r=parseRelational();
            auto n=std::make_shared<BinOpNode>(); n->op=op; n->left=left; n->right=r; left=n;
        }
        return left;
    }
    NodePtr parseRelational(){
        auto left=parseAddSub();
        while(check(TT::LT)||check(TT::GT)||check(TT::LTE)||check(TT::GTE)){
            auto op=consume().val; auto r=parseAddSub();
            auto n=std::make_shared<BinOpNode>(); n->op=op; n->left=left; n->right=r; left=n;
        }
        return left;
    }
    NodePtr parseAddSub(){
        auto left=parseMulDiv();
        while(check(TT::PLUS)||check(TT::MINUS)){
            auto op=consume().val; auto r=parseMulDiv();
            auto n=std::make_shared<BinOpNode>(); n->op=op; n->left=left; n->right=r; left=n;
        }
        return left;
    }
    NodePtr parseMulDiv(){
        auto left=parsePower();
        while(check(TT::STAR)||check(TT::SLASH)||check(TT::PERCENT)){
            auto op=consume().val; auto r=parsePower();
            auto n=std::make_shared<BinOpNode>(); n->op=op; n->left=left; n->right=r; left=n;
        }
        return left;
    }
    NodePtr parsePower(){
        auto left=parseUnary();
        if(check(TT::POWER)){
            consume();
            auto r=parsePower(); // right-associative
            auto n=std::make_shared<BinOpNode>(); n->op="**"; n->left=left; n->right=r; return n;
        }
        return left;
    }
    NodePtr parseUnary(){
        if(check(TT::MINUS)){
            consume();
            auto n=std::make_shared<UnaryOpNode>(); n->op="-"; n->operand=parseUnary(); return n;
        }
        return parsePostfix();
    }
    NodePtr parsePostfix(){
        auto node=parsePrimary();
        while(true){
            if(check(TT::LBRACKET)){
                consume();
                auto key=parseExpr(); expect(TT::RBRACKET,"]");
                auto n=std::make_shared<IndexNode>(); n->obj=node; n->key=key; node=n;
            } else if(check(TT::PLUSPLUS)||check(TT::MINUSMINUS)){
                auto op=consume().val;
                auto n=std::make_shared<PostfixNode>(); n->op=op; n->operand=node; node=n;
            } else break;
        }
        return node;
    }
    NodePtr parsePrimary(){
        if(check(TT::INT_LIT)){
            auto n=std::make_shared<IntLitNode>(); n->val=std::stoll(consume().val); return n;
        }
        if(check(TT::FLOAT_LIT)){
            auto n=std::make_shared<FloatLitNode>(); n->val=std::stod(consume().val); return n;
        }
        if(check(TT::STR_LIT)){
            auto tok=consume();
            auto n=std::make_shared<StrLitNode>();
            if(tok.val.size()>=2 && tok.val[0]=='r' && tok.val[1]=='\0'){
                n->val=tok.val.substr(2); n->interp=true;
            } else { n->val=tok.val; }
            return n;
        }
        if(check(TT::BOOL_LIT)){
            auto tok=consume();
            auto n=std::make_shared<BoolLitNode>(); n->val=(tok.val=="true"); return n;
        }
        if(check(TT::NULL_LIT)){ consume(); return std::make_shared<NullLitNode>(); }
        if(check(TT::LBRACKET)) return parseArrayOrObj();
        if(check(TT::KW_NUMBER_LOOPER)){
            consume(); expect(TT::LPAREN,"(");
            auto n=std::make_shared<NumberLooperNode>();
            n->start=parseExpr(); expect(TT::COMMA,","); n->end_=parseExpr();
            expect(TT::RPAREN,")"); return n;
        }
        if(check(TT::KW_DATA_TYPE)){
            consume(); expect(TT::LPAREN,"(");
            auto n=std::make_shared<DataTypeNode>(); n->expr=parseExpr();
            expect(TT::RPAREN,")"); return n;
        }
        if(check(TT::IDENT)){
            auto n=std::make_shared<IdentNode>(); n->name=consume().val; return n;
        }
        if(check(TT::LPAREN)){
            consume(); auto e=parseExpr(); expect(TT::RPAREN,")"); return e;
        }
        throw std::runtime_error("Line "+std::to_string(cur().line)+
            ": unexpected token '"+cur().val+"'");
    }

    NodePtr parseArrayOrObj(){
        int bracketLine=cur().line;
        expect(TT::LBRACKET,"[");
        if(check(TT::RBRACKET)){ consume(); return std::make_shared<ArrayLitNode>(); }

        // lookahead: obj if first token is a type keyword followed by key + =>
        bool isObj=false;
        if(isTypeKw()){
            size_t saved=pos;
            pos++; // skip type kw
            if(check(TT::STR_LIT)||check(TT::IDENT)) pos++;
            if(check(TT::FAT_ARROW)) isObj=true;
            pos=saved;
        }
        if(isObj) return parseObjBody(bracketLine);

        // plain array — but if we see a fat arrow it's a misuse
        auto arr=std::make_shared<ArrayLitNode>();
        while(!check(TT::RBRACKET)&&!check(TT::END_OF_FILE)){
            auto elem=parseExpr();
            if(check(TT::FAT_ARROW))
                throw std::runtime_error("Line "+std::to_string(cur().line)+
                    ": Syntax error: found '=>' inside an array. "
                    "Did you mean to declare an obj? Use 'obj' keyword.");
            arr->elems.push_back(elem);
            if(!match(TT::COMMA)) break;
        }
        expect(TT::RBRACKET,"]");
        return arr;
    }

    NodePtr parseObjBody(int startLine){
        auto obj=std::make_shared<ObjLitNode>();
        while(!check(TT::RBRACKET)&&!check(TT::END_OF_FILE)){
            if(!isTypeKw())
                throw std::runtime_error("Line "+std::to_string(cur().line)+
                    ": obj field must start with a type keyword "
                    "(int/float/str/bool/array/obj), got '"+cur().val+"'");
            std::string typeHint=consume().val;
            std::string key;
            if(check(TT::STR_LIT)) key=consume().val;
            else key=expect(TT::IDENT,"field key").val;
            expect(TT::FAT_ARROW,"=>");
            auto val=parseExpr();
            obj->pairs.push_back({key,typeHint,val});
            if(!match(TT::COMMA)) break;
        }
        expect(TT::RBRACKET,"]");
        return obj;
    }
};

// ─────────────────────────────────────────────
//  ENVIRONMENT
// ─────────────────────────────────────────────
struct Environment {
    std::map<std::string,ValuePtr> vars;
    std::shared_ptr<Environment> parent;
    Environment(std::shared_ptr<Environment> p=nullptr):parent(p){}

    void set(const std::string& name, ValuePtr val){
        for(Environment* e=this;e;e=e->parent.get())
            if(e->vars.count(name)){ e->vars[name]=val; return; }
        vars[name]=val;
    }
    ValuePtr get(const std::string& name){
        for(Environment* e=this;e;e=e->parent.get()){
            auto it=e->vars.find(name);
            if(it!=e->vars.end()) return it->second;
        }
        throw std::runtime_error("Undefined variable '"+name+"'");
    }
    void define(const std::string& name, ValuePtr val){ vars[name]=val; }
};

// ─────────────────────────────────────────────
//  INTERPRETER
// ─────────────────────────────────────────────
struct Interpreter {
    std::shared_ptr<Environment> global=std::make_shared<Environment>();

    void run(std::shared_ptr<ProgramNode> prog){
        for(auto& s:prog->stmts) execStmt(s,global);
    }

    // ── type enforcement on declaration ───────
    static void checkDeclType(const std::string& dtype,const std::string& name,ValuePtr val){
        auto got=val->typeName();
        // Null is always allowed as default / unset
        if(val->type==VType::NIL) return;
        // float accepts int (widening)
        if(dtype=="float" && val->type==VType::INT) return;
        if(dtype!=got)
            throw std::runtime_error(
                "Type error: '"+name+"' declared as "+dtype+
                " but assigned a "+got+" value ("+val->toString()+")");
    }

    void execStmt(NodePtr node, std::shared_ptr<Environment> env){
        std::string k=node->kind();
        if(k=="VarDecl"){
            auto n=std::static_pointer_cast<VarDeclNode>(node);
            auto val=evalExpr(n->init,env);
            checkDeclType(n->dtype,n->name,val);
            env->define(n->name,val);
        }
        else if(k=="Assign"){
            auto n=std::static_pointer_cast<AssignNode>(node);
            setValue(n->target,evalExpr(n->value,env),env);
        }
        else if(k=="Say"){
            auto n=std::static_pointer_cast<SayNode>(node);
            std::cout<<evalExpr(n->expr,env)->toString()<<"\n";
        }
        else if(k=="VarDumper"){
            auto n=std::static_pointer_cast<VarDumperNode>(node);
            std::cout<<evalExpr(n->expr,env)->varDump()<<"\n";
        }
        else if(k=="If"){
            auto n=std::static_pointer_cast<IfNode>(node);
            if(evalExpr(n->cond,env)->isTruthy())
                execStmt(n->then_,std::make_shared<Environment>(env));
            else if(n->else_)
                execStmt(n->else_,std::make_shared<Environment>(env));
        }
        else if(k=="Block"){
            auto n=std::static_pointer_cast<BlockNode>(node);
            for(auto& s:n->stmts) execStmt(s,env);
        }
        else if(k=="ForIn"){
            auto n=std::static_pointer_cast<ForInNode>(node);
            auto iter=evalExpr(n->iter,env);
            auto child=std::make_shared<Environment>(env);
            if(iter->type==VType::ARRAY){
                for(auto& v:iter->arr){ child->define(n->var,v); execStmt(n->body,child); }
            } else if(iter->type==VType::OBJ){
                for(auto& p:iter->obj){
                    child->define(n->var,std::make_shared<PyPPValue>(p.first));
                    execStmt(n->body,child);
                }
            } else throw std::runtime_error("for..in requires array or obj");
        }
        else if(k=="While"){
            auto n=std::static_pointer_cast<WhileNode>(node);
            auto child=std::make_shared<Environment>(env);
            while(evalExpr(n->cond,child)->isTruthy()) execStmt(n->body,child);
        }
        else if(k=="ExprStmt"){
            evalExpr(std::static_pointer_cast<ExprStmtNode>(node)->expr,env);
        }
    }

    void setValue(NodePtr target,ValuePtr val,std::shared_ptr<Environment> env){
        if(target->kind()=="Ident"){
            env->set(std::static_pointer_cast<IdentNode>(target)->name,val);
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

    // r-string interpolation at runtime
    std::string interpolate(const std::string& tmpl, std::shared_ptr<Environment> env){
        std::string result;
        for(size_t i=0;i<tmpl.size();){
            if(tmpl[i]=='\x01'){
                i++;
                std::string varName;
                while(i<tmpl.size()&&tmpl[i]!='\x02') varName+=tmpl[i++];
                if(i<tmpl.size()) i++;
                try { result+=env->get(varName)->toString(); }
                catch(...){ result+="{"+varName+"}"; }
            } else result+=tmpl[i++];
        }
        return result;
    }

    ValuePtr evalExpr(NodePtr node, std::shared_ptr<Environment> env){
        std::string k=node->kind();

        if(k=="IntLit")  return std::make_shared<PyPPValue>(std::static_pointer_cast<IntLitNode>(node)->val);
        if(k=="FloatLit")return std::make_shared<PyPPValue>(std::static_pointer_cast<FloatLitNode>(node)->val);
        if(k=="BoolLit") return std::make_shared<PyPPValue>(std::static_pointer_cast<BoolLitNode>(node)->val);
        if(k=="NullLit") return PyPPValue::makeNil();

        if(k=="StrLit"){
            auto n=std::static_pointer_cast<StrLitNode>(node);
            if(n->interp) return std::make_shared<PyPPValue>(interpolate(n->val,env));
            return std::make_shared<PyPPValue>(n->val);
        }

        if(k=="Ident") return env->get(std::static_pointer_cast<IdentNode>(node)->name);

        if(k=="ArrayLit"){
            auto n=std::static_pointer_cast<ArrayLitNode>(node);
            auto arr=PyPPValue::makeArray();
            for(auto& e:n->elems) arr->arr.push_back(evalExpr(e,env));
            return arr;
        }

        if(k=="ObjLit"){
            auto n=std::static_pointer_cast<ObjLitNode>(node);
            auto obj=PyPPValue::makeObj();
            for(auto& p:n->pairs){
                const std::string& key      = std::get<0>(p);
                const std::string& typeHint = std::get<1>(p);
                auto val=evalExpr(std::get<2>(p),env);
                // enforce declared field type
                std::string got=val->typeName();
                bool ok = (typeHint==got) || (typeHint=="float"&&got=="int");
                if(!ok && val->type!=VType::NIL)
                    throw std::runtime_error(
                        "Type error in obj field \""+key+"\": declared as "+
                        typeHint+" but got "+got+" value ("+val->toString()+").");
                obj->obj[key]=val;
            }
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
            throw std::runtime_error("Cannot index a "+obj->typeName());
        }

        if(k=="NumberLooper"){
            auto n=std::static_pointer_cast<NumberLooperNode>(node);
            auto s=evalExpr(n->start,env), e=evalExpr(n->end_,env);
            auto arr=PyPPValue::makeArray();
            for(long long x=s->ival;x<e->ival;x++)
                arr->arr.push_back(std::make_shared<PyPPValue>(x));
            return arr;
        }

        if(k=="DataType"){
            auto n=std::static_pointer_cast<DataTypeNode>(node);
            return std::make_shared<PyPPValue>(evalExpr(n->expr,env)->typeName());
        }

        if(k=="BinOp"){
            auto n=std::static_pointer_cast<BinOpNode>(node);
            // short-circuit
            if(n->op=="&&"){
                auto L=evalExpr(n->left,env);
                if(!L->isTruthy()) return std::make_shared<PyPPValue>(false);
                return std::make_shared<PyPPValue>((bool)evalExpr(n->right,env)->isTruthy());
            }
            if(n->op=="||"){
                auto L=evalExpr(n->left,env);
                if(L->isTruthy()) return std::make_shared<PyPPValue>(true);
                return std::make_shared<PyPPValue>((bool)evalExpr(n->right,env)->isTruthy());
            }
            return applyBinOp(n->op,evalExpr(n->left,env),evalExpr(n->right,env));
        }

        if(k=="UnaryOp"){
            auto n=std::static_pointer_cast<UnaryOpNode>(node);
            auto v=evalExpr(n->operand,env);
            if(n->op=="!") return std::make_shared<PyPPValue>((bool)!v->isTruthy());
            if(n->op=="-"){
                if(v->type==VType::INT)   return std::make_shared<PyPPValue>(-v->ival);
                if(v->type==VType::FLOAT) return std::make_shared<PyPPValue>(-v->fval);
            }
            return v;
        }

        if(k=="Postfix"){
            auto n=std::static_pointer_cast<PostfixNode>(node);
            auto v=evalExpr(n->operand,env);
            ValuePtr nv;
            if(v->type==VType::INT)
                nv=std::make_shared<PyPPValue>(v->ival+(n->op=="++"?1LL:-1LL));
            else if(v->type==VType::FLOAT)
                nv=std::make_shared<PyPPValue>(v->fval+(n->op=="++"?1.0:-1.0));
            else nv=v;
            setValue(n->operand,nv,env);
            return v;
        }

        throw std::runtime_error("Unknown expr node: "+k);
    }

    ValuePtr applyBinOp(const std::string& op, ValuePtr L, ValuePtr R){
        // string concat
        if(op=="+" && (L->type==VType::STR||R->type==VType::STR))
            return std::make_shared<PyPPValue>(L->toString()+R->toString());

        // power operator
        if(op=="**"){
            double base=L->type==VType::INT?(double)L->ival:L->fval;
            double exp =R->type==VType::INT?(double)R->ival:R->fval;
            double res =std::pow(base,exp);
            if(L->type==VType::INT && R->type==VType::INT && exp>=0)
                return std::make_shared<PyPPValue>((long long)std::llround(res));
            return std::make_shared<PyPPValue>(res);
        }

        bool isFloat=(L->type==VType::FLOAT||R->type==VType::FLOAT);
        double lf=L->type==VType::INT?(double)L->ival:L->fval;
        double rf=R->type==VType::INT?(double)R->ival:R->fval;
        long long li=L->type==VType::INT?L->ival:(long long)L->fval;
        long long ri=R->type==VType::INT?R->ival:(long long)R->fval;

        if(op=="+") return isFloat?std::make_shared<PyPPValue>(lf+rf):std::make_shared<PyPPValue>(li+ri);
        if(op=="-") return isFloat?std::make_shared<PyPPValue>(lf-rf):std::make_shared<PyPPValue>(li-ri);
        if(op=="*") return isFloat?std::make_shared<PyPPValue>(lf*rf):std::make_shared<PyPPValue>(li*ri);
        if(op=="/"){
            if(rf==0) throw std::runtime_error("Division by zero");
            return isFloat?std::make_shared<PyPPValue>(lf/rf):std::make_shared<PyPPValue>(li/ri);
        }
        if(op=="%") return std::make_shared<PyPPValue>(li%ri);

        // comparisons — handle Null, str, numeric
        if(op=="=="){
            if(L->type==VType::NIL&&R->type==VType::NIL) return std::make_shared<PyPPValue>(true);
            if(L->type==VType::NIL||R->type==VType::NIL) return std::make_shared<PyPPValue>(false);
            if(L->type==VType::STR||R->type==VType::STR)
                return std::make_shared<PyPPValue>((bool)(L->toString()==R->toString()));
            if(L->type==VType::BOOL||R->type==VType::BOOL)
                return std::make_shared<PyPPValue>((bool)(L->isTruthy()==R->isTruthy()));
            return std::make_shared<PyPPValue>((bool)(lf==rf));
        }
        if(op=="!="){
            if(L->type==VType::NIL&&R->type==VType::NIL) return std::make_shared<PyPPValue>(false);
            if(L->type==VType::NIL||R->type==VType::NIL) return std::make_shared<PyPPValue>(true);
            if(L->type==VType::STR||R->type==VType::STR)
                return std::make_shared<PyPPValue>((bool)(L->toString()!=R->toString()));
            return std::make_shared<PyPPValue>((bool)(lf!=rf));
        }
        if(op=="<") return std::make_shared<PyPPValue>((bool)(lf<rf));
        if(op==">") return std::make_shared<PyPPValue>((bool)(lf>rf));
        if(op=="<=")return std::make_shared<PyPPValue>((bool)(lf<=rf));
        if(op==">=")return std::make_shared<PyPPValue>((bool)(lf>=rf));

        throw std::runtime_error("Unknown operator: "+op);
    }
};

// ─────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────
int main(int argc,char* argv[]){
    if(argc<2){
        std::cout<<"PyPP Interpreter v2.0\nUsage: pypp <file.pypp>\n";
        return 0;
    }
    std::ifstream f(argv[1]);
    if(!f){ std::cerr<<"Error: cannot open '"<<argv[1]<<"'\n"; return 1; }
    std::ostringstream ss; ss<<f.rdbuf();
    try {
        auto tokens=lex(ss.str());
        Parser parser; parser.toks=tokens;
        auto ast=parser.parse();
        Interpreter interp; interp.run(ast);
    } catch(const std::exception& e){
        std::cerr<<"[PyPP Error] "<<e.what()<<"\n"; return 1;
    }
    return 0;
}