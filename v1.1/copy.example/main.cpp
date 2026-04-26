/*  PyPP Interpreter  v3.0
    New in v3:
      • Full JS-style array methods: push/pop/shift/unshift/splice/slice/
          indexOf/includes/find/findIndex/filter/map/forEach/reduce/
          sort/reverse/join/concat/flat/fill/every/some/length
      • array[0]  index access
      • File I/O:
            with "file.txt", "r", encoding="utf-8" as f { f.read() }
            with "file.txt", "w", encoding="utf-8" as f { f.write("...") }
            with "file.txt", "w+",encoding="utf-8" as f { f.write(...) f.read() }
            with "file.txt", "a", encoding="utf-8" as f { f.write("...") }
      • f.readLines()  →  array of lines
      • f.readLine()   →  single next line
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
#include <numeric>
#include <functional>
#include <cmath>

// ═══════════════════════════════════════════════════════
//  VALUE
// ═══════════════════════════════════════════════════════
enum class VType { INT, FLOAT, STR, BOOL, ARRAY, OBJ, FUNC, NIL };

struct PyPPValue;
using ValuePtr = std::shared_ptr<PyPPValue>;
using ObjMap   = std::map<std::string,ValuePtr>;
using ArrVec   = std::vector<ValuePtr>;

struct Environment;
struct ASTNode;
using NodePtr = std::shared_ptr<ASTNode>;

// Callable (for array method lambdas / future functions)
using NativeFn = std::function<ValuePtr(std::vector<ValuePtr>)>;

struct PyPPValue {
    VType       type = VType::NIL;
    long long   ival = 0;
    double      fval = 0.0;
    std::string sval;
    bool        bval = false;
    ArrVec      arr;
    ObjMap      obj;
    NativeFn    fn;   // for FUNC type

    PyPPValue(){}
    explicit PyPPValue(long long v)          : type(VType::INT),   ival(v){}
    explicit PyPPValue(double    v)          : type(VType::FLOAT), fval(v){}
    explicit PyPPValue(const std::string& s) : type(VType::STR),   sval(s){}
    explicit PyPPValue(bool b)               : type(VType::BOOL),  bval(b){}

    static ValuePtr makeArray(){ auto v=std::make_shared<PyPPValue>(); v->type=VType::ARRAY; return v; }
    static ValuePtr makeObj()  { auto v=std::make_shared<PyPPValue>(); v->type=VType::OBJ;   return v; }
    static ValuePtr makeNil()  { return std::make_shared<PyPPValue>(); }
    static ValuePtr makeFunc(NativeFn f){ auto v=std::make_shared<PyPPValue>(); v->type=VType::FUNC; v->fn=f; return v; }

    std::string typeName() const {
        switch(type){
            case VType::INT:   return "int";
            case VType::FLOAT: return "float";
            case VType::STR:   return "str";
            case VType::BOOL:  return "bool";
            case VType::ARRAY: return "array";
            case VType::OBJ:   return "obj";
            case VType::FUNC:  return "func";
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
            case VType::FUNC:  return "[Function]";
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
            case VType::FUNC:  return ind+"{func"+lbl+"}";
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
            case VType::FUNC:  return true;
            case VType::NIL:   return false;
        }
        return false;
    }
};

// ═══════════════════════════════════════════════════════
//  ARRAY METHODS  (JS-style)
// ═══════════════════════════════════════════════════════
// All methods take:  self (the array ValuePtr)  +  args
// Higher-order methods that need a callback use a separate path.

ValuePtr arrayMethod(ValuePtr self, const std::string& method,
                     const std::vector<ValuePtr>& args){
    if(self->type!=VType::ARRAY && method!="from")
        throw std::runtime_error("Array method '"+method+"' called on non-array ("+self->typeName()+")");

    ArrVec& a = self->arr;

    // ── length ──────────────────────────────────
    if(method=="length")
        return std::make_shared<PyPPValue>((long long)a.size());

    // ── push(v…) → newLength ────────────────────
    if(method=="push"){
        for(auto& v:args) a.push_back(v);
        return std::make_shared<PyPPValue>((long long)a.size());
    }
    // ── pop() → removed ─────────────────────────
    if(method=="pop"){
        if(a.empty()) return PyPPValue::makeNil();
        auto v=a.back(); a.pop_back(); return v;
    }
    // ── shift() → removed ───────────────────────
    if(method=="shift"){
        if(a.empty()) return PyPPValue::makeNil();
        auto v=a.front(); a.erase(a.begin()); return v;
    }
    // ── unshift(v…) → newLength ─────────────────
    if(method=="unshift"){
        for(int i=(int)args.size()-1;i>=0;i--) a.insert(a.begin(),args[i]);
        return std::make_shared<PyPPValue>((long long)a.size());
    }
    // ── indexOf(v, fromIndex=0) → int ───────────
    if(method=="indexOf"){
        if(args.empty()) return std::make_shared<PyPPValue>(-1LL);
        std::string needle=args[0]->toString();
        long long from=args.size()>1?args[1]->ival:0;
        for(long long i=from;i<(long long)a.size();i++)
            if(a[i]->toString()==needle) return std::make_shared<PyPPValue>(i);
        return std::make_shared<PyPPValue>(-1LL);
    }
    // ── lastIndexOf(v) → int ────────────────────
    if(method=="lastIndexOf"){
        if(args.empty()) return std::make_shared<PyPPValue>(-1LL);
        std::string needle=args[0]->toString();
        for(long long i=(long long)a.size()-1;i>=0;i--)
            if(a[i]->toString()==needle) return std::make_shared<PyPPValue>(i);
        return std::make_shared<PyPPValue>(-1LL);
    }
    // ── includes(v) → bool ──────────────────────
    if(method=="includes"){
        if(args.empty()) return std::make_shared<PyPPValue>(false);
        std::string needle=args[0]->toString();
        for(auto& v:a) if(v->toString()==needle) return std::make_shared<PyPPValue>(true);
        return std::make_shared<PyPPValue>(false);
    }
    // ── slice(start, end?) → array ──────────────
    if(method=="slice"){
        long long sz=(long long)a.size();
        long long start=0, end=sz;
        if(!args.empty()){ start=args[0]->ival; if(start<0) start=std::max(0LL,sz+start); }
        if(args.size()>1){ end=args[1]->ival;   if(end<0)   end  =std::max(0LL,sz+end);   }
        start=std::max(0LL,std::min(start,sz));
        end  =std::max(0LL,std::min(end,  sz));
        auto res=PyPPValue::makeArray();
        for(long long i=start;i<end;i++) res->arr.push_back(a[i]);
        return res;
    }
    // ── splice(start, deleteCount?, inserts…) → removed[] ──
    if(method=="splice"){
        long long sz=(long long)a.size();
        long long start=args.empty()?0:args[0]->ival;
        if(start<0) start=std::max(0LL,sz+start);
        start=std::max(0LL,std::min(start,sz));
        long long deleteCount=sz-start;
        if(args.size()>1) deleteCount=std::max(0LL,std::min(args[1]->ival, sz-start));
        auto removed=PyPPValue::makeArray();
        for(long long i=0;i<deleteCount;i++) removed->arr.push_back(a[start+i]);
        a.erase(a.begin()+start, a.begin()+start+deleteCount);
        for(size_t i=2;i<args.size();i++) a.insert(a.begin()+start+(i-2), args[i]);
        return removed;
    }
    // ── reverse() → self (in-place) ─────────────
    if(method=="reverse"){
        std::reverse(a.begin(),a.end()); return self;
    }
    // ── sort() → self (lexicographic) ───────────
    if(method=="sort"){
        std::sort(a.begin(),a.end(),[](const ValuePtr& x,const ValuePtr& y){
            // numeric sort if both numeric, else string sort
            if((x->type==VType::INT||x->type==VType::FLOAT)&&
               (y->type==VType::INT||y->type==VType::FLOAT)){
                double lf=x->type==VType::INT?(double)x->ival:x->fval;
                double rf=y->type==VType::INT?(double)y->ival:y->fval;
                return lf<rf;
            }
            return x->toString()<y->toString();
        });
        return self;
    }
    // ── join(sep=",") → str ─────────────────────
    if(method=="join"){
        std::string sep=args.empty()?",":(args[0]->type==VType::NIL?",":args[0]->toString());
        std::string r;
        for(size_t i=0;i<a.size();i++){ r+=a[i]->toString(); if(i+1<a.size()) r+=sep; }
        return std::make_shared<PyPPValue>(r);
    }
    // ── concat(arr…) → new array ────────────────
    if(method=="concat"){
        auto res=PyPPValue::makeArray();
        for(auto& v:a) res->arr.push_back(v);
        for(auto& arg:args){
            if(arg->type==VType::ARRAY) for(auto& v:arg->arr) res->arr.push_back(v);
            else res->arr.push_back(arg);
        }
        return res;
    }
    // ── flat(depth=1) → new array ───────────────
    if(method=="flat"){
        long long depth=args.empty()?1:args[0]->ival;
        std::function<void(ArrVec&,const ArrVec&,long long)> flatten;
        flatten=[&](ArrVec& out,const ArrVec& src,long long d){
            for(auto& v:src){
                if(v->type==VType::ARRAY && d>0) flatten(out,v->arr,d-1);
                else out.push_back(v);
            }
        };
        auto res=PyPPValue::makeArray();
        flatten(res->arr,a,depth);
        return res;
    }
    // ── fill(value, start=0, end=length) → self ─
    if(method=="fill"){
        ValuePtr fillVal=args.empty()?PyPPValue::makeNil():args[0];
        long long sz=(long long)a.size();
        long long start=args.size()>1?args[1]->ival:0;
        long long end  =args.size()>2?args[2]->ival:sz;
        if(start<0) start=std::max(0LL,sz+start);
        if(end  <0) end  =std::max(0LL,sz+end);
        start=std::max(0LL,std::min(start,sz));
        end  =std::max(0LL,std::min(end,  sz));
        for(long long i=start;i<end;i++) a[i]=fillVal;
        return self;
    }
    // ── at(index) → value ───────────────────────
    if(method=="at"){
        long long idx=args.empty()?0:args[0]->ival;
        long long sz=(long long)a.size();
        if(idx<0) idx=sz+idx;
        if(idx<0||idx>=sz) return PyPPValue::makeNil();
        return a[idx];
    }
    // ── Array.from(str) or Array.from(array) ────
    if(method=="from"){
        auto res=PyPPValue::makeArray();
        if(!args.empty()){
            auto& src=args[0];
            if(src->type==VType::STR){
                for(char c:src->sval) res->arr.push_back(std::make_shared<PyPPValue>(std::string(1,c)));
            } else if(src->type==VType::ARRAY){
                res->arr=src->arr;
            }
        }
        return res;
    }
    // ── toString() → str ────────────────────────
    if(method=="toString") return std::make_shared<PyPPValue>(self->toString());
    // ── isEmpty() → bool ────────────────────────
    if(method=="isEmpty") return std::make_shared<PyPPValue>((bool)a.empty());
    // ── first() / last() ────────────────────────
    if(method=="first") return a.empty()?PyPPValue::makeNil():a.front();
    if(method=="last")  return a.empty()?PyPPValue::makeNil():a.back();
    // ── clear() ─────────────────────────────────
    if(method=="clear"){ a.clear(); return self; }
    // ── count(val) → int ────────────────────────
    if(method=="count"){
        std::string needle=args.empty()?"":args[0]->toString();
        long long c=0;
        for(auto& v:a) if(v->toString()==needle) c++;
        return std::make_shared<PyPPValue>(c);
    }

    // higher-order methods need a callback — handled in interpreter
    // (map/filter/forEach/find/findIndex/reduce/every/some)
    throw std::runtime_error("Unknown array method: '"+method+"'");
}

// ═══════════════════════════════════════════════════════
//  LEXER
// ═══════════════════════════════════════════════════════
enum class TT {
    INT_LIT, FLOAT_LIT, STR_LIT, BOOL_LIT, NULL_LIT,
    KW_INT, KW_FLOAT, KW_STR, KW_BOOL, KW_ARRAY, KW_OBJ,
    KW_SAY, KW_VAR_DUMPER, KW_DATA_TYPE,
    KW_IF, KW_ELSE, KW_IFELSE,
    KW_FOR, KW_IN, KW_WHILE, KW_DO,
    KW_NUMBER_LOOPER,
    KW_AND, KW_OR, KW_NOT, KW_FN,
    KW_WITH, KW_AS, KW_ENCODING,
    IDENT,
    ASSIGN, FAT_ARROW,
    PLUS, MINUS, STAR, SLASH, PERCENT, POWER,
    EQ, NEQ, LT, GT, LTE, GTE,
    AND, OR, NOT,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    COMMA, SEMICOLON, DOT,
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
    {"and",TT::KW_AND},{"or",TT::KW_OR},{"not",TT::KW_NOT},{"fn",TT::KW_FN},
    {"with",TT::KW_WITH},{"as",TT::KW_AS},{"encoding",TT::KW_ENCODING},
    {"true",TT::BOOL_LIT},{"false",TT::BOOL_LIT},
    {"Null",TT::NULL_LIT},
};

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
        } else result+=src[i++];
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
        if(i+1<src.size()&&src[i]=='/'&&src[i+1]=='/'){
            while(i<src.size()&&src[i]!='\n') i++;
            continue;
        }
        if(src[i]=='r'&&i+1<src.size()&&src[i+1]=='"'){
            i+=2;
            std::string s=lexRString(src,i);
            tokens.push_back({TT::STR_LIT,std::string("r\0",2)+s,line});
            continue;
        }
        if(src[i]=='"'||src[i]=='\''){
            char q=src[i++]; std::string s;
            while(i<src.size()&&src[i]!=q){
                if(src[i]=='\\'&&i+1<src.size()){
                    i++;
                    if(src[i]=='n') s+='\n';
                    else if(src[i]=='t') s+='\t';
                    else s+=src[i]; i++;
                } else s+=src[i++];
            }
            if(i<src.size()) i++;
            tokens.push_back({TT::STR_LIT,s,line});
            continue;
        }
        bool negOK=tokens.empty()||
            tokens.back().type==TT::ASSIGN||tokens.back().type==TT::FAT_ARROW||
            tokens.back().type==TT::LPAREN||tokens.back().type==TT::COMMA||
            tokens.back().type==TT::LBRACKET;
        if(isdigit((unsigned char)src[i])||
           (src[i]=='-'&&negOK&&i+1<src.size()&&isdigit((unsigned char)src[i+1]))){
            std::string num; if(src[i]=='-') num+=src[i++];
            while(i<src.size()&&isdigit((unsigned char)src[i])) num+=src[i++];
            if(i<src.size()&&src[i]=='.'){
                num+=src[i++];
                while(i<src.size()&&isdigit((unsigned char)src[i])) num+=src[i++];
                tokens.push_back({TT::FLOAT_LIT,num,line});
            } else tokens.push_back({TT::INT_LIT,num,line});
            continue;
        }
        if(isalpha((unsigned char)src[i])||src[i]=='_'){
            std::string id;
            while(i<src.size()&&(isalnum((unsigned char)src[i])||src[i]=='_')) id+=src[i++];
            auto it=KEYWORDS.find(id);
            tokens.push_back({it!=KEYWORDS.end()?it->second:TT::IDENT,id,line});
            continue;
        }
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
        switch(src[i]){
            case '=': tokens.push_back({TT::ASSIGN,"=",line});    break;
            case '+': tokens.push_back({TT::PLUS,"+",line});      break;
            case '-': tokens.push_back({TT::MINUS,"-",line});     break;
            case '*': tokens.push_back({TT::STAR,"*",line});      break;
            case '/': tokens.push_back({TT::SLASH,"/",line});     break;
            case '%': tokens.push_back({TT::PERCENT,"%",line});   break;
            case '<': tokens.push_back({TT::LT,"<",line});        break;
            case '>': tokens.push_back({TT::GT,">",line});        break;
            case '!': tokens.push_back({TT::NOT,"!",line});       break;
            case '(': tokens.push_back({TT::LPAREN,"(",line});    break;
            case ')': tokens.push_back({TT::RPAREN,")",line});    break;
            case '{': tokens.push_back({TT::LBRACE,"{",line});    break;
            case '}': tokens.push_back({TT::RBRACE,"}",line});    break;
            case '[': tokens.push_back({TT::LBRACKET,"[",line});  break;
            case ']': tokens.push_back({TT::RBRACKET,"]",line});  break;
            case ',': tokens.push_back({TT::COMMA,",",line});     break;
            case ';': tokens.push_back({TT::SEMICOLON,";",line}); break;
            case '.': tokens.push_back({TT::DOT,".",line});       break;
            default: break;
        }
        i++;
    }
    tokens.push_back({TT::END_OF_FILE,"",line});
    return tokens;
}

// ═══════════════════════════════════════════════════════
//  AST NODES
// ═══════════════════════════════════════════════════════
struct ASTNode { virtual ~ASTNode()=default; virtual std::string kind()=0; };

struct ProgramNode      : ASTNode { std::vector<NodePtr> stmts; std::string kind()override{return"Program";} };
struct IntLitNode       : ASTNode { long long val=0; std::string kind()override{return"IntLit";} };
struct FloatLitNode     : ASTNode { double val=0;    std::string kind()override{return"FloatLit";} };
struct StrLitNode       : ASTNode { std::string val; bool interp=false; std::string kind()override{return"StrLit";} };
struct BoolLitNode      : ASTNode { bool val=false;  std::string kind()override{return"BoolLit";} };
struct NullLitNode      : ASTNode { std::string kind()override{return"NullLit";} };
struct IdentNode        : ASTNode { std::string name; std::string kind()override{return"Ident";} };
struct ArrayLitNode     : ASTNode { std::vector<NodePtr> elems; std::string kind()override{return"ArrayLit";} };
struct ObjLitNode       : ASTNode {
    std::vector<std::tuple<std::string,std::string,NodePtr>> pairs;
    std::string kind()override{return"ObjLit";}
};
struct IndexNode        : ASTNode { NodePtr obj,key; std::string kind()override{return"Index";} };
// arr.method(args)  or  file.method(args)
struct MethodCallNode   : ASTNode {
    NodePtr object;
    std::string method;
    std::vector<NodePtr> args;
    NodePtr callback; // for higher-order: map(|x| { … })
    std::string cbParam; // param name for callback
    std::string kind()override{return"MethodCall";}
};
struct BinOpNode        : ASTNode { std::string op; NodePtr left,right; std::string kind()override{return"BinOp";} };
struct UnaryOpNode      : ASTNode { std::string op; NodePtr operand; std::string kind()override{return"UnaryOp";} };
struct PostfixNode      : ASTNode { std::string op; NodePtr operand; std::string kind()override{return"Postfix";} };
struct NumberLooperNode : ASTNode { NodePtr start,end_; std::string kind()override{return"NumberLooper";} };
struct DataTypeNode     : ASTNode { NodePtr expr; std::string kind()override{return"DataType";} };
// statements
struct VarDeclNode  : ASTNode { std::string dtype,name; NodePtr init; std::string kind()override{return"VarDecl";} };
struct AssignNode   : ASTNode { NodePtr target,value; std::string kind()override{return"Assign";} };
struct SayNode      : ASTNode { NodePtr expr; std::string kind()override{return"Say";} };
struct VarDumperNode: ASTNode { NodePtr expr; std::string kind()override{return"VarDumper";} };
struct BlockNode    : ASTNode { std::vector<NodePtr> stmts; std::string kind()override{return"Block";} };
struct IfNode       : ASTNode { NodePtr cond,then_,else_; std::string kind()override{return"If";} };
struct ForInNode    : ASTNode { std::string var; NodePtr iter,body; std::string kind()override{return"ForIn";} };
struct WhileNode    : ASTNode { NodePtr cond,body; std::string kind()override{return"While";} };
struct ExprStmtNode : ASTNode { NodePtr expr; std::string kind()override{return"ExprStmt";} };
// with "file", "mode", encoding="utf-8" as f { ... }
struct WithNode : ASTNode {
    std::string filename;  // will be evaluated
    NodePtr     filenameExpr;
    std::string mode;      // "r","w","a","w+"
    std::string encoding;  // "utf-8" etc
    std::string alias;     // variable name (f)
    NodePtr     body;
    std::string kind()override{return"With";}
};
// Lambda/callback node: |param| { body }  (used in higher-order array methods)
struct LambdaNode : ASTNode {
    std::vector<std::string> params;
    NodePtr body;
    std::string kind()override{return"Lambda";}
};

// ═══════════════════════════════════════════════════════
//  PARSER
// ═══════════════════════════════════════════════════════
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
        if(isTypeKw())                return parseVarDecl();
        if(check(TT::KW_SAY))        return parseSay();
        if(check(TT::KW_VAR_DUMPER)) return parseVarDumper();
        if(check(TT::KW_IF))         return parseIf();
        if(check(TT::KW_IFELSE))     return parseIfElse();
        if(check(TT::KW_FOR))        return parseFor();
        if(check(TT::KW_WHILE)||check(TT::KW_DO)) return parseWhile();
        if(check(TT::KW_WITH))       return parseWith();
        if(check(TT::LBRACE))        return parseBlock();
        return parseAssignOrExpr();
    }

    // ── with ──────────────────────────────────────
    NodePtr parseWith(){
        consume(); // with
        auto n=std::make_shared<WithNode>();
        // filename expression
        n->filenameExpr=parseExpr();
        expect(TT::COMMA,",");
        // mode string
        if(!check(TT::STR_LIT)) throw std::runtime_error("Line "+std::to_string(cur().line)+": expected mode string");
        n->mode=consume().val;
        // optional  , encoding="utf-8"
        n->encoding="utf-8";
        if(check(TT::COMMA)){
            consume();
            if(check(TT::KW_ENCODING)||
               (check(TT::IDENT)&&cur().val=="encoding")){
                consume();
                expect(TT::ASSIGN,"=");
                if(!check(TT::STR_LIT)) throw std::runtime_error("expected encoding string");
                n->encoding=consume().val;
            }
        }
        expect(TT::KW_AS,"as");
        n->alias=expect(TT::IDENT,"file alias").val;
        n->body=parseBlock();
        return n;
    }

    NodePtr parseVarDecl(){
        int declLine=cur().line;
        std::string dtype=consume().val;
        std::string name =expect(TT::IDENT,"identifier").val;
        NodePtr init;
        if(match(TT::ASSIGN)){
            init=parseExpr();
            if(dtype=="obj"&&init->kind()=="ArrayLit")
                throw std::runtime_error("Line "+std::to_string(declLine)+
                    ": Type error: '"+name+"' declared as obj but got a plain array.\n"
                    "  Use: obj x = [ str \"key\" => value ]");
            if(dtype=="array"&&init->kind()=="ObjLit")
                throw std::runtime_error("Line "+std::to_string(declLine)+
                    ": Type error: '"+name+"' declared as array but got obj literal.");
        } else {
            if(dtype=="int")  { auto nd=std::make_shared<IntLitNode>();   nd->val=0;     init=nd; }
            else if(dtype=="float"){ auto nd=std::make_shared<FloatLitNode>(); nd->val=0.0;  init=nd; }
            else if(dtype=="str")  { auto nd=std::make_shared<StrLitNode>();               init=nd; }
            else if(dtype=="bool") { auto nd=std::make_shared<BoolLitNode>(); nd->val=false; init=nd; }
            else init=std::make_shared<NullLitNode>();
        }
        match(TT::SEMICOLON);
        auto nd=std::make_shared<VarDeclNode>(); nd->dtype=dtype; nd->name=name; nd->init=init;
        return nd;
    }

    NodePtr parseSay(){
        consume();
        auto n=std::make_shared<SayNode>(); n->expr=parseExpr();
        match(TT::SEMICOLON); return n;
    }
    NodePtr parseVarDumper(){
        consume(); expect(TT::LPAREN,"(");
        auto n=std::make_shared<VarDumperNode>(); n->expr=parseExpr();
        expect(TT::RPAREN,")"); match(TT::SEMICOLON); return n;
    }
    NodePtr parseIf(){
        consume();
        auto n=std::make_shared<IfNode>(); n->cond=parseExpr(); n->then_=parseBlock();
        if(check(TT::KW_IFELSE))      n->else_=parseIfElse();
        else if(check(TT::KW_ELSE)){ consume(); n->else_=parseBlock(); }
        return n;
    }
    NodePtr parseIfElse(){
        consume();
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

    // ── expressions ───────────────────────────────
    NodePtr parseExpr(){ return parseOr(); }

    NodePtr parseOr(){
        auto left=parseAnd();
        while(check(TT::OR)||check(TT::KW_OR)){
            consume(); auto r=parseAnd();
            auto n=std::make_shared<BinOpNode>(); n->op="||"; n->left=left; n->right=r; left=n;
        }
        return left;
    }
    NodePtr parseAnd(){
        auto left=parseNot();
        while(check(TT::AND)||check(TT::KW_AND)){
            consume(); auto r=parseNot();
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
            consume(); auto r=parsePower();
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
            // array[index]  or  obj[key]
            if(check(TT::LBRACKET)){
                consume();
                auto key=parseExpr(); expect(TT::RBRACKET,"]");
                auto n=std::make_shared<IndexNode>(); n->obj=node; n->key=key; node=n;
            }
            // expr.method(args)
            else if(check(TT::DOT)){
                consume();
                std::string method=expect(TT::IDENT,"method name").val;
                auto mc=std::make_shared<MethodCallNode>();
                mc->object=node; mc->method=method;
                if(match(TT::LPAREN)){
                    while(!check(TT::RPAREN)&&!check(TT::END_OF_FILE)){
                        // lambda argument: fn(param, param2) { body }
                        if(check(TT::KW_FN)){
                            consume(); // fn
                            expect(TT::LPAREN,"(");
                            auto lm=std::make_shared<LambdaNode>();
                            while(!check(TT::RPAREN)&&!check(TT::END_OF_FILE)){
                                lm->params.push_back(expect(TT::IDENT,"param").val);
                                match(TT::COMMA);
                            }
                            expect(TT::RPAREN,")");
                            lm->body=parseBlock();
                            mc->callback=lm;
                        } else {
                            mc->args.push_back(parseExpr());
                        }
                        if(!match(TT::COMMA)) break;
                    }
                    expect(TT::RPAREN,")");
                }
                node=mc;
            }
            else if(check(TT::PLUSPLUS)||check(TT::MINUSMINUS)){
                auto op=consume().val;
                auto n=std::make_shared<PostfixNode>(); n->op=op; n->operand=node; node=n;
            }
            else break;
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
            if(tok.val.size()>=2&&tok.val[0]=='r'&&tok.val[1]=='\0'){
                n->val=tok.val.substr(2); n->interp=true;
            } else n->val=tok.val;
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
        int bline=cur().line;
        expect(TT::LBRACKET,"[");
        if(check(TT::RBRACKET)){ consume(); return std::make_shared<ArrayLitNode>(); }
        bool isObj=false;
        if(isTypeKw()){
            size_t saved=pos;
            pos++;
            if(check(TT::STR_LIT)||check(TT::IDENT)) pos++;
            if(check(TT::FAT_ARROW)) isObj=true;
            pos=saved;
        }
        if(isObj) return parseObjBody(bline);
        auto arr=std::make_shared<ArrayLitNode>();
        while(!check(TT::RBRACKET)&&!check(TT::END_OF_FILE)){
            auto elem=parseExpr();
            if(check(TT::FAT_ARROW))
                throw std::runtime_error("Line "+std::to_string(cur().line)+
                    ": found '=>' inside array — did you mean obj? Use 'obj' keyword.");
            arr->elems.push_back(elem);
            if(!match(TT::COMMA)) break;
        }
        expect(TT::RBRACKET,"]");
        return arr;
    }

    NodePtr parseObjBody(int /*ln*/){
        auto obj=std::make_shared<ObjLitNode>();
        while(!check(TT::RBRACKET)&&!check(TT::END_OF_FILE)){
            if(!isTypeKw())
                throw std::runtime_error("Line "+std::to_string(cur().line)+
                    ": obj field must start with type keyword, got '"+cur().val+"'");
            std::string typeHint=consume().val;
            std::string key;
            if(check(TT::STR_LIT)) key=consume().val;
            else key=expect(TT::IDENT,"key").val;
            expect(TT::FAT_ARROW,"=>");
            auto val=parseExpr();
            obj->pairs.push_back({key,typeHint,val});
            if(!match(TT::COMMA)) break;
        }
        expect(TT::RBRACKET,"]");
        return obj;
    }
};

// ═══════════════════════════════════════════════════════
//  ENVIRONMENT
// ═══════════════════════════════════════════════════════
struct Environment {
    std::map<std::string,ValuePtr> vars;
    std::shared_ptr<Environment> parent;
    Environment(std::shared_ptr<Environment> p=nullptr):parent(p){}
    void set(const std::string& name,ValuePtr val){
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
    void define(const std::string& name,ValuePtr val){ vars[name]=val; }
};

// ═══════════════════════════════════════════════════════
//  FILE HANDLE WRAPPER
// ═══════════════════════════════════════════════════════
struct FileHandle {
    std::string filename, mode, encoding;
    std::fstream fs;
    std::string buffer; // for read caching
    bool eof=false;

    void open(){
        std::ios::openmode om=std::ios::in;
        if(mode=="w")       om=std::ios::out|std::ios::trunc;
        else if(mode=="a")  om=std::ios::out|std::ios::app;
        else if(mode=="w+") om=std::ios::in|std::ios::out|std::ios::trunc;
        else if(mode=="r+") om=std::ios::in|std::ios::out;
        fs.open(filename,om);
        if(!fs.is_open())
            throw std::runtime_error("Cannot open file '"+filename+"' with mode '"+mode+"'");
    }
    void close(){ fs.close(); }

    ValuePtr read(){
        std::ostringstream ss; ss<<fs.rdbuf();
        return std::make_shared<PyPPValue>(ss.str());
    }
    ValuePtr readLines(){
        auto arr=PyPPValue::makeArray();
        std::string line;
        while(std::getline(fs,line))
            arr->arr.push_back(std::make_shared<PyPPValue>(line));
        return arr;
    }
    ValuePtr readLine(){
        std::string line;
        if(std::getline(fs,line)) return std::make_shared<PyPPValue>(line);
        return PyPPValue::makeNil();
    }
    void write(const std::string& s){ fs<<s; }
    void writeLine(const std::string& s){ fs<<s<<"\n"; }
};

// ═══════════════════════════════════════════════════════
//  INTERPRETER
// ═══════════════════════════════════════════════════════
struct Interpreter {
    std::shared_ptr<Environment> global=std::make_shared<Environment>();

    void run(std::shared_ptr<ProgramNode> prog){
        for(auto& s:prog->stmts) execStmt(s,global);
    }

    static void checkDeclType(const std::string& dtype,const std::string& name,ValuePtr val){
        if(val->type==VType::NIL) return;
        if(dtype=="float"&&val->type==VType::INT) return;
        auto got=val->typeName();
        if(dtype!=got)
            throw std::runtime_error("Type error: '"+name+"' declared as "+dtype+
                " but assigned "+got+" ("+val->toString()+")");
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
            } else if(iter->type==VType::STR){
                for(char c:iter->sval){
                    child->define(n->var,std::make_shared<PyPPValue>(std::string(1,c)));
                    execStmt(n->body,child);
                }
            } else throw std::runtime_error("for..in requires array, obj, or str");
        }
        else if(k=="While"){
            auto n=std::static_pointer_cast<WhileNode>(node);
            auto child=std::make_shared<Environment>(env);
            while(evalExpr(n->cond,child)->isTruthy()) execStmt(n->body,child);
        }
        else if(k=="ExprStmt"){
            evalExpr(std::static_pointer_cast<ExprStmtNode>(node)->expr,env);
        }
        else if(k=="With"){
            execWith(std::static_pointer_cast<WithNode>(node),env);
        }
    }

    // ── with statement ──────────────────────────
    void execWith(std::shared_ptr<WithNode> node, std::shared_ptr<Environment> env){
        auto fnameVal=evalExpr(node->filenameExpr,env);
        FileHandle fh;
        fh.filename=fnameVal->toString();
        fh.mode=node->mode;
        fh.encoding=node->encoding;
        fh.open();

        // build a child env with file-method functions
        auto child=std::make_shared<Environment>(env);
        std::string alias=node->alias;

        // Expose file handle methods as native functions on an obj
        auto fileObj=PyPPValue::makeObj();

        fileObj->obj["read"]  = PyPPValue::makeFunc([&fh](std::vector<ValuePtr>)->ValuePtr{
            return fh.read();
        });
        fileObj->obj["readLines"] = PyPPValue::makeFunc([&fh](std::vector<ValuePtr>)->ValuePtr{
            return fh.readLines();
        });
        fileObj->obj["readLine"] = PyPPValue::makeFunc([&fh](std::vector<ValuePtr>)->ValuePtr{
            return fh.readLine();
        });
        fileObj->obj["write"] = PyPPValue::makeFunc([&fh](std::vector<ValuePtr> args)->ValuePtr{
            if(!args.empty()) fh.write(args[0]->toString());
            return PyPPValue::makeNil();
        });
        fileObj->obj["writeLine"] = PyPPValue::makeFunc([&fh](std::vector<ValuePtr> args)->ValuePtr{
            if(!args.empty()) fh.writeLine(args[0]->toString());
            return PyPPValue::makeNil();
        });
        fileObj->obj["close"] = PyPPValue::makeFunc([&fh](std::vector<ValuePtr>)->ValuePtr{
            fh.close(); return PyPPValue::makeNil();
        });

        child->define(alias, fileObj);
        execStmt(node->body, child);
        fh.close();
    }

    // ── lvalue assignment ────────────────────────
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

    // ── r-string interpolation ───────────────────
    std::string interpolate(const std::string& tmpl,std::shared_ptr<Environment> env){
        std::string result;
        for(size_t i=0;i<tmpl.size();){
            if(tmpl[i]=='\x01'){
                i++;
                std::string varName;
                while(i<tmpl.size()&&tmpl[i]!='\x02') varName+=tmpl[i++];
                if(i<tmpl.size()) i++;
                try{ result+=env->get(varName)->toString(); }
                catch(...){ result+="{"+varName+"}"; }
            } else result+=tmpl[i++];
        }
        return result;
    }

    // ── expression evaluator ────────────────────
    ValuePtr evalExpr(NodePtr node,std::shared_ptr<Environment> env){
        std::string k=node->kind();

        if(k=="IntLit")   return std::make_shared<PyPPValue>(std::static_pointer_cast<IntLitNode>(node)->val);
        if(k=="FloatLit") return std::make_shared<PyPPValue>(std::static_pointer_cast<FloatLitNode>(node)->val);
        if(k=="BoolLit")  return std::make_shared<PyPPValue>(std::static_pointer_cast<BoolLitNode>(node)->val);
        if(k=="NullLit")  return PyPPValue::makeNil();
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
                const std::string& key=std::get<0>(p);
                const std::string& th =std::get<1>(p);
                auto val=evalExpr(std::get<2>(p),env);
                std::string got=val->typeName();
                bool ok=(th==got)||(th=="float"&&got=="int");
                if(!ok&&val->type!=VType::NIL)
                    throw std::runtime_error("Type error in obj field \""+key+
                        "\": declared "+th+" but got "+got+" ("+val->toString()+")");
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
            return std::make_shared<PyPPValue>(
                evalExpr(std::static_pointer_cast<DataTypeNode>(node)->expr,env)->typeName());
        }

        if(k=="MethodCall"){
            auto n=std::static_pointer_cast<MethodCallNode>(node);
            auto obj=evalExpr(n->object,env);

            // file object method call (obj with FUNC values)
            if(obj->type==VType::OBJ){
                auto it=obj->obj.find(n->method);
                if(it!=obj->obj.end()&&it->second->type==VType::FUNC){
                    std::vector<ValuePtr> args;
                    for(auto& a:n->args) args.push_back(evalExpr(a,env));
                    return it->second->fn(args);
                }
            }

            // array methods
            if(obj->type==VType::ARRAY || obj->type==VType::STR){
                // handle higher-order methods with callback
                if(n->callback){
                    auto lm=std::static_pointer_cast<LambdaNode>(n->callback);
                    return evalHigherOrder(n->method,obj,lm,env);
                }
                // simple method
                std::vector<ValuePtr> args;
                for(auto& a:n->args) args.push_back(evalExpr(a,env));
                // str methods
                if(obj->type==VType::STR) return strMethod(obj,n->method,args);
                return arrayMethod(obj,n->method,args);
            }

            throw std::runtime_error("Cannot call method '"+n->method+"' on "+obj->typeName());
        }

        if(k=="BinOp"){
            auto n=std::static_pointer_cast<BinOpNode>(node);
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

    // ── higher-order array methods ───────────────
    ValuePtr evalHigherOrder(const std::string& method, ValuePtr arr,
                             std::shared_ptr<LambdaNode> lm,
                             std::shared_ptr<Environment> env){
        auto callCb=[&](std::vector<ValuePtr> cbArgs) -> ValuePtr {
            auto cbEnv=std::make_shared<Environment>(env);
            for(size_t i=0;i<lm->params.size();i++)
                cbEnv->define(lm->params[i], i<cbArgs.size()?cbArgs[i]:PyPPValue::makeNil());
            ValuePtr last=PyPPValue::makeNil();
            auto blk=std::static_pointer_cast<BlockNode>(lm->body);
            for(auto& s:blk->stmts) execStmt(s,cbEnv);
            // grab return value from __return__ if set, else last expr
            if(cbEnv->vars.count("__return__")) return cbEnv->vars["__return__"];
            return PyPPValue::makeNil();
        };
        // We need to evaluate expressions in callbacks; re-impl to capture last expr-stmt
        auto callCbExpr=[&](std::vector<ValuePtr> cbArgs) -> ValuePtr {
            auto cbEnv=std::make_shared<Environment>(env);
            for(size_t i=0;i<lm->params.size();i++)
                cbEnv->define(lm->params[i], i<cbArgs.size()?cbArgs[i]:PyPPValue::makeNil());
            auto blk=std::static_pointer_cast<BlockNode>(lm->body);
            ValuePtr last=PyPPValue::makeNil();
            for(auto& s:blk->stmts){
                if(s->kind()=="ExprStmt")
                    last=evalExpr(std::static_pointer_cast<ExprStmtNode>(s)->expr,cbEnv);
                else execStmt(s,cbEnv);
            }
            return last;
        };

        ArrVec& a=arr->arr;

        if(method=="forEach"){
            for(size_t i=0;i<a.size();i++)
                callCbExpr({a[i],std::make_shared<PyPPValue>((long long)i),arr});
            return PyPPValue::makeNil();
        }
        if(method=="map"){
            auto res=PyPPValue::makeArray();
            for(size_t i=0;i<a.size();i++)
                res->arr.push_back(callCbExpr({a[i],std::make_shared<PyPPValue>((long long)i),arr}));
            return res;
        }
        if(method=="filter"){
            auto res=PyPPValue::makeArray();
            for(size_t i=0;i<a.size();i++)
                if(callCbExpr({a[i],std::make_shared<PyPPValue>((long long)i),arr})->isTruthy())
                    res->arr.push_back(a[i]);
            return res;
        }
        if(method=="find"){
            for(size_t i=0;i<a.size();i++)
                if(callCbExpr({a[i],std::make_shared<PyPPValue>((long long)i),arr})->isTruthy())
                    return a[i];
            return PyPPValue::makeNil();
        }
        if(method=="findIndex"){
            for(size_t i=0;i<a.size();i++)
                if(callCbExpr({a[i],std::make_shared<PyPPValue>((long long)i),arr})->isTruthy())
                    return std::make_shared<PyPPValue>((long long)i);
            return std::make_shared<PyPPValue>(-1LL);
        }
        if(method=="every"){
            for(size_t i=0;i<a.size();i++)
                if(!callCbExpr({a[i],std::make_shared<PyPPValue>((long long)i),arr})->isTruthy())
                    return std::make_shared<PyPPValue>(false);
            return std::make_shared<PyPPValue>(true);
        }
        if(method=="some"){
            for(size_t i=0;i<a.size();i++)
                if(callCbExpr({a[i],std::make_shared<PyPPValue>((long long)i),arr})->isTruthy())
                    return std::make_shared<PyPPValue>(true);
            return std::make_shared<PyPPValue>(false);
        }
        if(method=="reduce"){
            if(a.empty()) return PyPPValue::makeNil();
            ValuePtr acc=a[0];
            for(size_t i=1;i<a.size();i++)
                acc=callCbExpr({acc,a[i],std::make_shared<PyPPValue>((long long)i),arr});
            return acc;
        }
        throw std::runtime_error("Unknown higher-order array method: '"+method+"'");
    }

    // ── string methods ───────────────────────────
    ValuePtr strMethod(ValuePtr sv, const std::string& method, const std::vector<ValuePtr>& args){
        std::string& s=sv->sval;
        if(method=="length")  return std::make_shared<PyPPValue>((long long)s.size());
        if(method=="toUpperCase"){ std::string r=s; for(auto&c:r)c=toupper(c); return std::make_shared<PyPPValue>(r); }
        if(method=="toLowerCase"){ std::string r=s; for(auto&c:r)c=tolower(c); return std::make_shared<PyPPValue>(r); }
        if(method=="trim"){
            size_t a=s.find_first_not_of(" \t\r\n");
            size_t b=s.find_last_not_of(" \t\r\n");
            if(a==std::string::npos) return std::make_shared<PyPPValue>(std::string());
            return std::make_shared<PyPPValue>(s.substr(a,b-a+1));
        }
        if(method=="includes"){
            if(args.empty()) return std::make_shared<PyPPValue>(false);
            return std::make_shared<PyPPValue>((bool)(s.find(args[0]->toString())!=std::string::npos));
        }
        if(method=="startsWith"){
            if(args.empty()) return std::make_shared<PyPPValue>(false);
            std::string prefix=args[0]->toString();
            return std::make_shared<PyPPValue>((bool)(s.rfind(prefix,0)==0));
        }
        if(method=="endsWith"){
            if(args.empty()) return std::make_shared<PyPPValue>(false);
            std::string suffix=args[0]->toString();
            if(suffix.size()>s.size()) return std::make_shared<PyPPValue>(false);
            return std::make_shared<PyPPValue>((bool)(s.compare(s.size()-suffix.size(),suffix.size(),suffix)==0));
        }
        if(method=="indexOf"){
            if(args.empty()) return std::make_shared<PyPPValue>(-1LL);
            auto pos=s.find(args[0]->toString());
            return std::make_shared<PyPPValue>(pos==std::string::npos?-1LL:(long long)pos);
        }
        if(method=="slice"){
            long long sz=(long long)s.size();
            long long start=args.empty()?0:args[0]->ival;
            long long end=sz;
            if(args.size()>1) end=args[1]->ival;
            if(start<0) start=std::max(0LL,sz+start);
            if(end<0)   end  =std::max(0LL,sz+end);
            start=std::max(0LL,std::min(start,sz));
            end  =std::max(0LL,std::min(end,sz));
            return std::make_shared<PyPPValue>(s.substr(start,end-start));
        }
        if(method=="split"){
            std::string sep=args.empty()?"":(args[0]->type==VType::NIL?"":args[0]->toString());
            auto res=PyPPValue::makeArray();
            if(sep.empty()){
                for(char c:s) res->arr.push_back(std::make_shared<PyPPValue>(std::string(1,c)));
            } else {
                size_t pos=0,found;
                while((found=s.find(sep,pos))!=std::string::npos){
                    res->arr.push_back(std::make_shared<PyPPValue>(s.substr(pos,found-pos)));
                    pos=found+sep.size();
                }
                res->arr.push_back(std::make_shared<PyPPValue>(s.substr(pos)));
            }
            return res;
        }
        if(method=="replace"){
            if(args.size()<2) return sv;
            std::string from=args[0]->toString(), to=args[1]->toString();
            std::string r=s;
            auto pos=r.find(from);
            if(pos!=std::string::npos) r.replace(pos,from.size(),to);
            return std::make_shared<PyPPValue>(r);
        }
        if(method=="replaceAll"){
            if(args.size()<2) return sv;
            std::string from=args[0]->toString(), to=args[1]->toString();
            std::string r=s;
            size_t pos=0;
            while((pos=r.find(from,pos))!=std::string::npos){ r.replace(pos,from.size(),to); pos+=to.size(); }
            return std::make_shared<PyPPValue>(r);
        }
        if(method=="repeat"){
            long long n=args.empty()?0:args[0]->ival;
            std::string r; for(long long i=0;i<n;i++) r+=s;
            return std::make_shared<PyPPValue>(r);
        }
        if(method=="charAt"){
            long long idx=args.empty()?0:args[0]->ival;
            if(idx<0||idx>=(long long)s.size()) return std::make_shared<PyPPValue>(std::string());
            return std::make_shared<PyPPValue>(std::string(1,s[idx]));
        }
        if(method=="charCodeAt"){
            long long idx=args.empty()?0:args[0]->ival;
            if(idx<0||idx>=(long long)s.size()) return PyPPValue::makeNil();
            return std::make_shared<PyPPValue>((long long)(unsigned char)s[idx]);
        }
        if(method=="padStart"){
            long long tlen=args.empty()?0:args[0]->ival;
            std::string pad=args.size()>1?args[1]->toString():" ";
            std::string r=s;
            while((long long)r.size()<tlen) r=pad+r;
            return std::make_shared<PyPPValue>(r.substr(r.size()-(size_t)tlen));
        }
        if(method=="padEnd"){
            long long tlen=args.empty()?0:args[0]->ival;
            std::string pad=args.size()>1?args[1]->toString():" ";
            std::string r=s;
            while((long long)r.size()<tlen) r+=pad;
            return std::make_shared<PyPPValue>(r.substr(0,(size_t)tlen));
        }
        if(method=="toString") return sv;
        throw std::runtime_error("Unknown string method: '"+method+"'");
    }

    ValuePtr applyBinOp(const std::string& op,ValuePtr L,ValuePtr R){
        if(op=="+"&&(L->type==VType::STR||R->type==VType::STR))
            return std::make_shared<PyPPValue>(L->toString()+R->toString());
        if(op=="**"){
            double base=L->type==VType::INT?(double)L->ival:L->fval;
            double exp =R->type==VType::INT?(double)R->ival:R->fval;
            double res =std::pow(base,exp);
            if(L->type==VType::INT&&R->type==VType::INT&&exp>=0)
                return std::make_shared<PyPPValue>((long long)std::llround(res));
            return std::make_shared<PyPPValue>(res);
        }
        bool isF=(L->type==VType::FLOAT||R->type==VType::FLOAT);
        double lf=L->type==VType::INT?(double)L->ival:L->fval;
        double rf=R->type==VType::INT?(double)R->ival:R->fval;
        long long li=L->type==VType::INT?L->ival:(long long)L->fval;
        long long ri=R->type==VType::INT?R->ival:(long long)R->fval;
        if(op=="+") return isF?std::make_shared<PyPPValue>(lf+rf):std::make_shared<PyPPValue>(li+ri);
        if(op=="-") return isF?std::make_shared<PyPPValue>(lf-rf):std::make_shared<PyPPValue>(li-ri);
        if(op=="*") return isF?std::make_shared<PyPPValue>(lf*rf):std::make_shared<PyPPValue>(li*ri);
        if(op=="/"){
            if(rf==0) throw std::runtime_error("Division by zero");
            return isF?std::make_shared<PyPPValue>(lf/rf):std::make_shared<PyPPValue>(li/ri);
        }
        if(op=="%") return std::make_shared<PyPPValue>(li%ri);
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

// ═══════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════
int main(int argc,char* argv[]){
    if(argc<2){
        std::cout<<"PyPP Interpreter v3.0\nUsage: pypp <file.pypp>\n";
        return 0;
    }
    std::ifstream f(argv[1]);
    if(!f){ std::cerr<<"Error: cannot open '"<<argv[1]<<"'\n"; return 1; }
    std::ostringstream ss; ss<<f.rdbuf();
    try{
        auto tokens=lex(ss.str());
        Parser parser; parser.toks=tokens;
        auto ast=parser.parse();
        Interpreter interp; interp.run(ast);
    } catch(const std::exception& e){
        std::cerr<<"[PyPP Error] "<<e.what()<<"\n"; return 1;
    }
    return 0;
}
