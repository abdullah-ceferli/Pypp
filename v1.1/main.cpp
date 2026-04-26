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
#include <filesystem>
#include <cstdlib>
#include <chrono>
namespace fs = std::filesystem;

static const std::string BUILTIN_RANDOM =
    "// random.pypp — PyPP standard random module\n"
    "\n"
    "int __seed = 123456789\n"
    "\n"
    "function seed(n) {\n"
    "    __seed = n\n"
    "}\n"
    "\n"
    "function __next() {\n"
    "    __seed = (__seed * 1664525 + 1013904223) % 4294967296\n"
    "    return __seed\n"
    "}\n"
    "\n"
    "function random() {\n"
    "    int r = __next()\n"
    "    float f = r / 4294967296.0\n"
    "    return f\n"
    "}\n"
    "\n"
    "function randint(a, b) {\n"
    "    int r = __next()\n"
    "    int range = b - a + 1\n"
    "    int result = a + (r % range)\n"
    "    return result\n"
    "}\n"
    "\n"
    "function randrange(start, stop) {\n"
    "    int r = __next()\n"
    "    int range = stop - start\n"
    "    int result = start + (r % range)\n"
    "    return result\n"
    "}\n"
    "\n"
    "function uniform(a, b) {\n"
    "    float r = random()\n"
    "    float result = a + r * (b - a)\n"
    "    return result\n"
    "}\n"
    "\n"
    "function choice(arr) {\n"
    "    int n = arr.length()\n"
    "    int idx = __next() % n\n"
    "    return arr[idx]\n"
    "}\n"
    "\n"
    "function shuffle(arr) {\n"
    "    int n = arr.length()\n"
    "    int i = n - 1\n"
    "    while i > 0 {\n"
    "        int j = __next() % (i + 1)\n"
    "        array tmp = arr.slice(i, i + 1)\n"
    "        arr[i] = arr[j]\n"
    "        arr[j] = tmp[0]\n"
    "        i = i - 1\n"
    "    }\n"
    "    return arr\n"
    "}\n"
    "\n"
    "function sample(arr, k) {\n"
    "    array pool = arr.slice(0, arr.length())\n"
    "    int n = pool.length()\n"
    "    int i = 0\n"
    "    array result = []\n"
    "    while i < k {\n"
    "        int j = i + (__next() % (n - i))\n"
    "        array tmp = pool.slice(i, i + 1)\n"
    "        pool[i] = pool[j]\n"
    "        pool[j] = tmp[0]\n"
    "        result.push(pool[i])\n"
    "        i = i + 1\n"
    "    }\n"
    "    return result\n"
    "}\n"
    "\n"
    "function choices(arr, k) {\n"
    "    int n = arr.length()\n"
    "    array result = []\n"
    "    int i = 0\n"
    "    while i < k {\n"
    "        int idx = __next() % n\n"
    "        result.push(arr[idx])\n"
    "        i = i + 1\n"
    "    }\n"
    "    return result\n"
    "}\n"
    "\n"
    "function gauss(mu, sigma) {\n"
    "    float u1 = random()\n"
    "    float u2 = random()\n"
    "    float z = (u1 + u2 + u1 * u2 - 1.5) * 2.0\n"
    "    float result = mu + z * sigma\n"
    "    return result\n"
    "}\n"
    "\n"
    "function triangular(low, high, mode) {\n"
    "    float u = random()\n"
    "    float c = (mode - low) / (high - low)\n"
    "    float result = low + (high - low) * u\n"
    "    return result\n"
    "}\n"
    "\n"
    "function betavariate(alpha, beta_) {\n"
    "    float u = random()\n"
    "    float v = random()\n"
    "    float x = u ** (1.0 / alpha)\n"
    "    float y = v ** (1.0 / beta_)\n"
    "    float s = x + y\n"
    "    return x / s\n"
    "}\n"
    "\n"
    "function expovariate(lambd) {\n"
    "    float u = random()\n"
    "    float neg_ln = (1.0 - u) + (1.0 - u) * (1.0 - u) * 0.5\n"
    "    float result = neg_ln / lambd\n"
    "    return result\n"
    "}\n"
    "\n"
    "function getrandbits(k) {\n"
    "    int mask = (2 ** k) - 1\n"
    "    int r = __next() % (mask + 1)\n"
    "    return r\n"
    "}\n"
    "\n";

static const std::map<std::string,const std::string*> BUILTIN_MODULES = {
    {"random", &BUILTIN_RANDOM}
};


// ═══════════════════════════════════════════════
//  VALUE
// ═══════════════════════════════════════════════
enum class VType { INT, FLOAT, STR, BOOL, ARRAY, OBJ, FUNC, USERFUNC, NIL };

struct PyPPValue;
using ValuePtr = std::shared_ptr<PyPPValue>;
using ObjMap   = std::map<std::string,ValuePtr>;
using ArrVec   = std::vector<ValuePtr>;
struct ASTNode;
using NodePtr = std::shared_ptr<ASTNode>;
struct Environment;
using EnvPtr = std::shared_ptr<Environment>;
using NativeFn = std::function<ValuePtr(std::vector<ValuePtr>)>;

// forward-declare user function body so PyPPValue can hold it
struct UserFunc {
    std::vector<std::string> params;
    NodePtr body;
    EnvPtr closure;
};

struct PyPPValue {
    VType       type  = VType::NIL;
    long long   ival  = 0;
    double      fval  = 0.0;
    std::string sval;
    bool        bval  = false;
    ArrVec      arr;
    ObjMap      obj;
    NativeFn    nfn;
    std::shared_ptr<UserFunc> ufn;

    PyPPValue(){}
    explicit PyPPValue(long long v)          : type(VType::INT),  ival(v){}
    explicit PyPPValue(double    v)          : type(VType::FLOAT),fval(v){}
    explicit PyPPValue(const std::string& s) : type(VType::STR),  sval(s){}
    explicit PyPPValue(bool b)               : type(VType::BOOL), bval(b){}

    static ValuePtr makeArray() { auto v=std::make_shared<PyPPValue>(); v->type=VType::ARRAY;   return v; }
    static ValuePtr makeObj()   { auto v=std::make_shared<PyPPValue>(); v->type=VType::OBJ;     return v; }
    static ValuePtr makeNil()   { return std::make_shared<PyPPValue>(); }
    static ValuePtr makeNative(NativeFn f){
        auto v=std::make_shared<PyPPValue>(); v->type=VType::FUNC; v->nfn=f; return v;
    }
    static ValuePtr makeUser(std::shared_ptr<UserFunc> uf){
        auto v=std::make_shared<PyPPValue>(); v->type=VType::USERFUNC; v->ufn=uf; return v;
    }

    std::string typeName() const {
        switch(type){
            case VType::INT:      return "int";
            case VType::FLOAT:    return "float";
            case VType::STR:      return "str";
            case VType::BOOL:     return "bool";
            case VType::ARRAY:    return "array";
            case VType::OBJ:      return "obj";
            case VType::FUNC:     return "func";
            case VType::USERFUNC: return "func";
            case VType::NIL:      return "Null";
        }
        return "?";
    }

    std::string toString() const {
        switch(type){
            case VType::INT:      return std::to_string(ival);
            case VType::FLOAT:    { std::ostringstream o; o<<fval; return o.str(); }
            case VType::STR:      return sval;
            case VType::BOOL:     return bval?"true":"false";
            case VType::NIL:      return "Null";
            case VType::FUNC:
            case VType::USERFUNC: return "[Function]";
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
        std::string lbl=label.empty()?"":"[\""+label+"\"]";
        switch(type){
            case VType::INT:      return ind+"{int"+lbl+" => "+std::to_string(ival)+"}";
            case VType::FLOAT:    { std::ostringstream o; o<<fval; return ind+"{float"+lbl+" => "+o.str()+"}"; }
            case VType::STR:      return ind+"{str"+lbl+" => \""+sval+"\"}";
            case VType::BOOL:     return ind+"{bool"+lbl+" => "+(bval?"true":"false")+"}";
            case VType::NIL:      return ind+"{Null"+lbl+"}";
            case VType::FUNC:
            case VType::USERFUNC: return ind+"{func"+lbl+"}";
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
            case VType::INT:      return ival!=0;
            case VType::FLOAT:    return fval!=0.0;
            case VType::STR:      return !sval.empty();
            case VType::BOOL:     return bval;
            case VType::ARRAY:    return !arr.empty();
            case VType::OBJ:      return !obj.empty();
            case VType::FUNC:
            case VType::USERFUNC: return true;
            case VType::NIL:      return false;
        }
        return false;
    }
};

// ═══════════════════════════════════════════════
//  RETURN SIGNAL  (exception-based)
// ═══════════════════════════════════════════════
struct ReturnSignal {
    ValuePtr value;
    explicit ReturnSignal(ValuePtr v): value(v){}
};

// ═══════════════════════════════════════════════
//  ARRAY METHODS
// ═══════════════════════════════════════════════
ValuePtr arrayMethod(ValuePtr self,const std::string& method,const std::vector<ValuePtr>& args){
    if(self->type!=VType::ARRAY)
        throw std::runtime_error("Array method '"+method+"' called on "+self->typeName());
    ArrVec& a=self->arr;

    if(method=="length")     return std::make_shared<PyPPValue>((long long)a.size());
    if(method=="isEmpty")    return std::make_shared<PyPPValue>((bool)a.empty());
    if(method=="first")      return a.empty()?PyPPValue::makeNil():a.front();
    if(method=="last")       return a.empty()?PyPPValue::makeNil():a.back();
    if(method=="clear")      { a.clear(); return self; }
    if(method=="toString")   return std::make_shared<PyPPValue>(self->toString());

    if(method=="push")  { for(auto& v:args) a.push_back(v); return std::make_shared<PyPPValue>((long long)a.size()); }
    if(method=="pop")   { if(a.empty()) return PyPPValue::makeNil(); auto v=a.back(); a.pop_back(); return v; }
    if(method=="shift") { if(a.empty()) return PyPPValue::makeNil(); auto v=a.front(); a.erase(a.begin()); return v; }
    if(method=="unshift"){ for(int i=(int)args.size()-1;i>=0;i--) a.insert(a.begin(),args[i]); return std::make_shared<PyPPValue>((long long)a.size()); }

    if(method=="indexOf"){
        if(args.empty()) return std::make_shared<PyPPValue>(-1LL);
        std::string needle=args[0]->toString();
        long long from=args.size()>1?args[1]->ival:0;
        for(long long i=from;i<(long long)a.size();i++) if(a[i]->toString()==needle) return std::make_shared<PyPPValue>(i);
        return std::make_shared<PyPPValue>(-1LL);
    }
    if(method=="lastIndexOf"){
        if(args.empty()) return std::make_shared<PyPPValue>(-1LL);
        std::string needle=args[0]->toString();
        for(long long i=(long long)a.size()-1;i>=0;i--) if(a[i]->toString()==needle) return std::make_shared<PyPPValue>(i);
        return std::make_shared<PyPPValue>(-1LL);
    }
    if(method=="includes"){
        if(args.empty()) return std::make_shared<PyPPValue>(false);
        std::string needle=args[0]->toString();
        for(auto& v:a) if(v->toString()==needle) return std::make_shared<PyPPValue>(true);
        return std::make_shared<PyPPValue>(false);
    }
    if(method=="count"){
        std::string needle=args.empty()?"":args[0]->toString();
        long long c=0; for(auto& v:a) if(v->toString()==needle) c++;
        return std::make_shared<PyPPValue>(c);
    }
    if(method=="at"){
        long long idx=args.empty()?0:args[0]->ival; long long sz=(long long)a.size();
        if(idx<0) idx=sz+idx; if(idx<0||idx>=sz) return PyPPValue::makeNil();
        return a[idx];
    }
    if(method=="slice"){
        long long sz=(long long)a.size(), start=0, end=sz;
        if(!args.empty()){ start=args[0]->ival; if(start<0) start=std::max(0LL,sz+start); }
        if(args.size()>1){ end=args[1]->ival;   if(end<0)   end  =std::max(0LL,sz+end);   }
        start=std::max(0LL,std::min(start,sz)); end=std::max(0LL,std::min(end,sz));
        auto res=PyPPValue::makeArray(); for(long long i=start;i<end;i++) res->arr.push_back(a[i]); return res;
    }
    if(method=="splice"){
        long long sz=(long long)a.size(), start=args.empty()?0:args[0]->ival;
        if(start<0) start=std::max(0LL,sz+start); start=std::max(0LL,std::min(start,sz));
        long long dc=sz-start; if(args.size()>1) dc=std::max(0LL,std::min(args[1]->ival,sz-start));
        auto removed=PyPPValue::makeArray();
        for(long long i=0;i<dc;i++) removed->arr.push_back(a[start+i]);
        a.erase(a.begin()+start,a.begin()+start+dc);
        for(size_t i=2;i<args.size();i++) a.insert(a.begin()+start+(i-2),args[i]);
        return removed;
    }
    if(method=="reverse") { std::reverse(a.begin(),a.end()); return self; }
    if(method=="sort"){
        std::sort(a.begin(),a.end(),[](const ValuePtr& x,const ValuePtr& y){
            if((x->type==VType::INT||x->type==VType::FLOAT)&&(y->type==VType::INT||y->type==VType::FLOAT)){
                double lf=x->type==VType::INT?(double)x->ival:x->fval;
                double rf=y->type==VType::INT?(double)y->ival:y->fval;
                return lf<rf;
            }
            return x->toString()<y->toString();
        }); return self;
    }
    if(method=="join"){
        std::string sep=args.empty()?",":(args[0]->type==VType::NIL?",":args[0]->toString());
        std::string r; for(size_t i=0;i<a.size();i++){ r+=a[i]->toString(); if(i+1<a.size()) r+=sep; }
        return std::make_shared<PyPPValue>(r);
    }
    if(method=="concat"){
        auto res=PyPPValue::makeArray(); for(auto& v:a) res->arr.push_back(v);
        for(auto& arg:args){ if(arg->type==VType::ARRAY) for(auto& v:arg->arr) res->arr.push_back(v); else res->arr.push_back(arg); }
        return res;
    }
    if(method=="flat"){
        long long depth=args.empty()?1:args[0]->ival;
        std::function<void(ArrVec&,const ArrVec&,long long)> flatten;
        flatten=[&](ArrVec& out,const ArrVec& src,long long d){
            for(auto& v:src){ if(v->type==VType::ARRAY&&d>0) flatten(out,v->arr,d-1); else out.push_back(v); }
        };
        auto res=PyPPValue::makeArray(); flatten(res->arr,a,depth); return res;
    }
    if(method=="fill"){
        ValuePtr fv=args.empty()?PyPPValue::makeNil():args[0]; long long sz=(long long)a.size();
        long long start=args.size()>1?args[1]->ival:0, end=args.size()>2?args[2]->ival:sz;
        if(start<0)start=std::max(0LL,sz+start); if(end<0)end=std::max(0LL,sz+end);
        start=std::max(0LL,std::min(start,sz)); end=std::max(0LL,std::min(end,sz));
        for(long long i=start;i<end;i++) a[i]=fv; return self;
    }
    if(method=="from"){
        auto res=PyPPValue::makeArray();
        if(!args.empty()){ auto& src=args[0];
            if(src->type==VType::STR) for(char c:src->sval) res->arr.push_back(std::make_shared<PyPPValue>(std::string(1,c)));
            else if(src->type==VType::ARRAY) res->arr=src->arr;
        } return res;
    }
    // higher-order handled elsewhere
    throw std::runtime_error("Unknown array method '"+method+"'");
}

// ═══════════════════════════════════════════════
//  LEXER
// ═══════════════════════════════════════════════
enum class TT {
    INT_LIT,FLOAT_LIT,STR_LIT,BOOL_LIT,NULL_LIT,
    KW_INT,KW_FLOAT,KW_STR,KW_BOOL,KW_ARRAY,KW_OBJ,
    KW_SAY,KW_VAR_DUMPER,KW_DATA_TYPE,
    KW_IF,KW_ELSE,KW_IFELSE,
    KW_FOR,KW_IN,KW_WHILE,KW_DO,
    KW_NUMBER_LOOPER,
    KW_AND,KW_OR,KW_NOT,KW_FN,
    KW_FUNC,KW_RETURN,
    KW_WITH,KW_AS,KW_ENCODING,
    KW_TYPEOF,KW_IMPORT,
    IDENT,
    ASSIGN,FAT_ARROW,
    PLUS,MINUS,STAR,SLASH,PERCENT,POWER,
    EQ,NEQ,LT,GT,LTE,GTE,
    AND,OR,NOT,
    LPAREN,RPAREN,LBRACE,RBRACE,LBRACKET,RBRACKET,
    COMMA,SEMICOLON,DOT,
    PLUSPLUS,MINUSMINUS,
    END_OF_FILE
};

struct Token { TT type; std::string val; int line=1; };

static const std::map<std::string,TT> KEYWORDS={
    {"int",TT::KW_INT},{"float",TT::KW_FLOAT},{"str",TT::KW_STR},
    {"bool",TT::KW_BOOL},{"array",TT::KW_ARRAY},{"obj",TT::KW_OBJ},
    {"say",TT::KW_SAY},{"var_dumper",TT::KW_VAR_DUMPER},{"dataType",TT::KW_DATA_TYPE},
    {"if",TT::KW_IF},{"else",TT::KW_ELSE},{"ifelse",TT::KW_IFELSE},
    {"for",TT::KW_FOR},{"in",TT::KW_IN},{"while",TT::KW_WHILE},{"do",TT::KW_DO},
    {"numberLooper",TT::KW_NUMBER_LOOPER},
    {"and",TT::KW_AND},{"or",TT::KW_OR},{"not",TT::KW_NOT},{"fn",TT::KW_FN},
    {"function",TT::KW_FUNC},{"return",TT::KW_RETURN},
    {"with",TT::KW_WITH},{"as",TT::KW_AS},{"encoding",TT::KW_ENCODING},
    {"typeOf",TT::KW_TYPEOF},{"import",TT::KW_IMPORT},
    {"true",TT::BOOL_LIT},{"false",TT::BOOL_LIT},{"Null",TT::NULL_LIT},
};

// parse r"..." or r`...` interpolated content — markers: \x01varname\x02
static std::string lexInterp(const std::string& src, size_t& i, char closing){
    std::string s;
    while(i<src.size()&&src[i]!=closing){
        if(src[i]=='\\'&&i+1<src.size()&&src[i+1]=='{'){
            i+=2; std::string v; while(i<src.size()&&src[i]!='}') v+=src[i++];
            if(i<src.size()) i++; s+='\x01'; s+=v; s+='\x02';
        } else if(src[i]=='{'&&closing=='`'){
            // also allow plain {x} inside backticks
            i++; std::string v; while(i<src.size()&&src[i]!='}') v+=src[i++];
            if(i<src.size()) i++; s+='\x01'; s+=v; s+='\x02';
        } else if(src[i]=='\\'&&i+1<src.size()){
            i++; if(src[i]=='n') s+='\n'; else if(src[i]=='t') s+='\t'; else s+=src[i]; i++;
        } else s+=src[i++];
    }
    if(i<src.size()) i++;
    return s;
}

// interp string token: value starts with "r\0" prefix
static Token makeInterpTok(const std::string& body, int line){
    return {TT::STR_LIT, std::string("r\0",2)+body, line};
}

std::vector<Token> lex(const std::string& src){
    std::vector<Token> tokens; size_t i=0; int line=1;
    while(i<src.size()){
        if(src[i]=='\n'){line++;i++;continue;}
        if(isspace((unsigned char)src[i])){i++;continue;}
        if(i+1<src.size()&&src[i]=='/'&&src[i+1]=='/'){while(i<src.size()&&src[i]!='\n')i++;continue;}

        // r"..." or r`...` — interpolated strings
        if(src[i]=='r'&&i+1<src.size()&&(src[i+1]=='"'||src[i+1]=='`')){
            char closing=src[i+1]=='"'?'"':'`'; i+=2;
            tokens.push_back(makeInterpTok(lexInterp(src,i,closing),line)); continue;
        }
        // regular string
        if(src[i]=='"'||src[i]=='\''||src[i]=='`'){
            char q=src[i++]; std::string s;
            while(i<src.size()&&src[i]!=q){
                if(src[i]=='\\'&&i+1<src.size()){i++; if(src[i]=='n')s+='\n'; else if(src[i]=='t')s+='\t'; else s+=src[i]; i++;}
                else s+=src[i++];
            }
            if(i<src.size()) i++;
            tokens.push_back({TT::STR_LIT,s,line}); continue;
        }
        // numbers
        bool negOK=tokens.empty()||
            tokens.back().type==TT::ASSIGN||tokens.back().type==TT::FAT_ARROW||
            tokens.back().type==TT::LPAREN||tokens.back().type==TT::COMMA||
            tokens.back().type==TT::LBRACKET;
        if(isdigit((unsigned char)src[i])||(src[i]=='-'&&negOK&&i+1<src.size()&&isdigit((unsigned char)src[i+1]))){
            std::string num; if(src[i]=='-') num+=src[i++];
            while(i<src.size()&&isdigit((unsigned char)src[i])) num+=src[i++];
            if(i<src.size()&&src[i]=='.'){num+=src[i++]; while(i<src.size()&&isdigit((unsigned char)src[i])) num+=src[i++]; tokens.push_back({TT::FLOAT_LIT,num,line});}
            else tokens.push_back({TT::INT_LIT,num,line});
            continue;
        }
        // identifiers/keywords
        if(isalpha((unsigned char)src[i])||src[i]=='_'){
            std::string id; while(i<src.size()&&(isalnum((unsigned char)src[i])||src[i]=='_')) id+=src[i++];
            auto it=KEYWORDS.find(id); tokens.push_back({it!=KEYWORDS.end()?it->second:TT::IDENT,id,line}); continue;
        }
        // two-char ops
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
            case '=':tokens.push_back({TT::ASSIGN,"=",line});break;
            case '+':tokens.push_back({TT::PLUS,"+",line});break;
            case '-':tokens.push_back({TT::MINUS,"-",line});break;
            case '*':tokens.push_back({TT::STAR,"*",line});break;
            case '/':tokens.push_back({TT::SLASH,"/",line});break;
            case '%':tokens.push_back({TT::PERCENT,"%",line});break;
            case '<':tokens.push_back({TT::LT,"<",line});break;
            case '>':tokens.push_back({TT::GT,">",line});break;
            case '!':tokens.push_back({TT::NOT,"!",line});break;
            case '(':tokens.push_back({TT::LPAREN,"(",line});break;
            case ')':tokens.push_back({TT::RPAREN,")",line});break;
            case '{':tokens.push_back({TT::LBRACE,"{",line});break;
            case '}':tokens.push_back({TT::RBRACE,"}",line});break;
            case '[':tokens.push_back({TT::LBRACKET,"[",line});break;
            case ']':tokens.push_back({TT::RBRACKET,"]",line});break;
            case ',':tokens.push_back({TT::COMMA,",",line});break;
            case ';':tokens.push_back({TT::SEMICOLON,";",line});break;
            case '.':tokens.push_back({TT::DOT,".",line});break;
            default:break;
        }
        i++;
    }
    tokens.push_back({TT::END_OF_FILE,"",line});
    return tokens;
}

// ═══════════════════════════════════════════════
//  AST NODES
// ═══════════════════════════════════════════════
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
struct MethodCallNode   : ASTNode {
    NodePtr object; std::string method; std::vector<NodePtr> args; NodePtr callback; std::string kind()override{return"MethodCall";}
};
struct FuncCallNode     : ASTNode {
    std::string name; std::vector<NodePtr> args; std::string kind()override{return"FuncCall";}
};
struct BinOpNode        : ASTNode { std::string op; NodePtr left,right; std::string kind()override{return"BinOp";} };
struct UnaryOpNode      : ASTNode { std::string op; NodePtr operand; std::string kind()override{return"UnaryOp";} };
struct PostfixNode      : ASTNode { std::string op; NodePtr operand; std::string kind()override{return"Postfix";} };
struct NumberLooperNode : ASTNode { NodePtr start,end_; std::string kind()override{return"NumberLooper";} };
struct DataTypeNode     : ASTNode { NodePtr expr; std::string kind()override{return"DataType";} };
struct TypeOfNode       : ASTNode { NodePtr expr; std::string kind()override{return"TypeOf";} };
struct ImportNode       : ASTNode { std::string module; std::string alias; std::string kind()override{return"Import";} };
// chain: "hi" . x . " bye"  →  concatenated string
struct ConcatChainNode  : ASTNode { std::vector<NodePtr> parts; std::string kind()override{return"ConcatChain";} };

// statements
struct VarDeclNode   : ASTNode { std::string dtype,name; NodePtr init; std::string kind()override{return"VarDecl";} };
struct AssignNode    : ASTNode { NodePtr target,value; std::string kind()override{return"Assign";} };
struct SayNode       : ASTNode { NodePtr expr; std::string kind()override{return"Say";} };
struct VarDumperNode : ASTNode { NodePtr expr; std::string kind()override{return"VarDumper";} };
struct BlockNode     : ASTNode { std::vector<NodePtr> stmts; std::string kind()override{return"Block";} };
struct IfNode        : ASTNode { NodePtr cond,then_,else_; std::string kind()override{return"If";} };
struct ForInNode     : ASTNode { std::string var; NodePtr iter,body; std::string kind()override{return"ForIn";} };
struct WhileNode     : ASTNode { NodePtr cond,body; std::string kind()override{return"While";} };
struct ExprStmtNode  : ASTNode { NodePtr expr; std::string kind()override{return"ExprStmt";} };
struct FuncDeclNode  : ASTNode {
    std::string name; std::vector<std::string> params; NodePtr body;
    std::string kind()override{return"FuncDecl";}
};
struct ReturnNode    : ASTNode { NodePtr expr; std::string kind()override{return"Return";} };
struct LambdaNode    : ASTNode { std::vector<std::string> params; NodePtr body; std::string kind()override{return"Lambda";} };
struct WithNode      : ASTNode {
    NodePtr filenameExpr; std::string mode,encoding,alias; NodePtr body;
    std::string kind()override{return"With";}
};

// ═══════════════════════════════════════════════
//  PARSER
// ═══════════════════════════════════════════════
struct Parser {
    std::vector<Token> toks; size_t pos=0;

    Token& cur(){ return toks[pos]; }
    Token consume(){ return toks[pos++]; }
    Token expect(TT t,const std::string& msg){
        if(cur().type!=t) throw std::runtime_error("Line "+std::to_string(cur().line)+": expected "+msg+", got '"+cur().val+"'");
        return consume();
    }
    bool check(TT t){ return cur().type==t; }
    bool match(TT t){ if(check(t)){consume();return true;} return false; }
    bool isTypeKw(){ return check(TT::KW_INT)||check(TT::KW_FLOAT)||check(TT::KW_STR)||check(TT::KW_BOOL)||check(TT::KW_ARRAY)||check(TT::KW_OBJ); }

    std::shared_ptr<ProgramNode> parse(){
        auto p=std::make_shared<ProgramNode>();
        while(!check(TT::END_OF_FILE)) p->stmts.push_back(parseStmt());
        return p;
    }

    NodePtr parseStmt(){
        if(isTypeKw())               return parseVarDecl();
        if(check(TT::KW_IMPORT))     return parseImport();
        if(check(TT::KW_FUNC))       return parseFuncDecl();
        if(check(TT::KW_RETURN))     return parseReturn();
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

    // ── import ───────────────────────────────
    NodePtr parseImport(){
        consume(); // import
        auto n=std::make_shared<ImportNode>();
        n->module=expect(TT::IDENT,"module name").val;
        // optional: import random as rnd
        if(check(TT::KW_AS)){ consume(); n->alias=expect(TT::IDENT,"alias").val; }
        else n->alias=n->module;
        match(TT::SEMICOLON);
        return n;
    }

    // ── function declaration ─────────────────
    NodePtr parseFuncDecl(){
        consume(); // function
        auto n=std::make_shared<FuncDeclNode>();
        n->name=expect(TT::IDENT,"function name").val;
        expect(TT::LPAREN,"(");
        while(!check(TT::RPAREN)&&!check(TT::END_OF_FILE)){
            n->params.push_back(expect(TT::IDENT,"param").val);
            match(TT::COMMA);
        }
        expect(TT::RPAREN,")");
        n->body=parseBlock();
        return n;
    }

    // ── return ───────────────────────────────
    NodePtr parseReturn(){
        consume(); // return
        auto n=std::make_shared<ReturnNode>();
        if(!check(TT::SEMICOLON)&&!check(TT::RBRACE)&&!check(TT::END_OF_FILE))
            n->expr=parseExpr();
        match(TT::SEMICOLON);
        return n;
    }

    // ── say  (with optional .chain) ──────────
    NodePtr parseSay(){
        consume(); // say
        auto first=parseExpr();
        // dot-chain: say "hi" . x . " bye"  =>  concat all parts
        if(check(TT::DOT)){
            auto chain=std::make_shared<ConcatChainNode>();
            chain->parts.push_back(first);
            while(match(TT::DOT)) chain->parts.push_back(parseExpr());
            match(TT::SEMICOLON);
            auto sn=std::make_shared<SayNode>(); sn->expr=chain; return sn;
        }
        match(TT::SEMICOLON);
        auto sn=std::make_shared<SayNode>(); sn->expr=first; return sn;
    }

    NodePtr parseVarDumper(){
        consume(); expect(TT::LPAREN,"(");
        auto n=std::make_shared<VarDumperNode>(); n->expr=parseExpr();
        expect(TT::RPAREN,")"); match(TT::SEMICOLON); return n;
    }
    NodePtr parseIf(){
        consume(); auto n=std::make_shared<IfNode>(); n->cond=parseExpr(); n->then_=parseBlock();
        if(check(TT::KW_IFELSE))       n->else_=parseIfElse();
        else if(check(TT::KW_ELSE)){ consume(); n->else_=parseBlock(); }
        return n;
    }
    NodePtr parseIfElse(){
        consume(); auto n=std::make_shared<IfNode>(); n->cond=parseExpr(); n->then_=parseBlock();
        if(check(TT::KW_IFELSE))       n->else_=parseIfElse();
        else if(check(TT::KW_ELSE)){ consume(); n->else_=parseBlock(); }
        return n;
    }
    NodePtr parseFor(){
        consume(); auto n=std::make_shared<ForInNode>();
        n->var=expect(TT::IDENT,"variable").val; expect(TT::KW_IN,"in");
        n->iter=parseExpr(); n->body=parseBlock(); return n;
    }
    NodePtr parseWhile(){
        bool isDo=check(TT::KW_DO); consume();
        if(isDo&&check(TT::KW_WHILE)) consume();
        auto n=std::make_shared<WhileNode>(); n->cond=parseExpr(); n->body=parseBlock(); return n;
    }
    std::shared_ptr<BlockNode> parseBlock(){
        expect(TT::LBRACE,"{");
        auto b=std::make_shared<BlockNode>();
        while(!check(TT::RBRACE)&&!check(TT::END_OF_FILE)) b->stmts.push_back(parseStmt());
        expect(TT::RBRACE,"}"); return b;
    }
    NodePtr parseVarDecl(){
        int ln=cur().line; std::string dtype=consume().val;
        std::string name=expect(TT::IDENT,"identifier").val;
        NodePtr init;
        if(match(TT::ASSIGN)){
            init=parseExpr();
            if(dtype=="obj"&&init->kind()=="ArrayLit")
                throw std::runtime_error("Line "+std::to_string(ln)+": Type error: '"+name+"' declared as obj but got plain array.\n  Use: obj x = [ str \"key\" => value ]");
            if(dtype=="array"&&init->kind()=="ObjLit")
                throw std::runtime_error("Line "+std::to_string(ln)+": Type error: '"+name+"' declared as array but got obj.");
        } else {
            if(dtype=="int")  { auto nd=std::make_shared<IntLitNode>(); nd->val=0; init=nd; }
            else if(dtype=="float"){ auto nd=std::make_shared<FloatLitNode>(); nd->val=0.0; init=nd; }
            else if(dtype=="str")  { init=std::make_shared<StrLitNode>(); }
            else if(dtype=="bool") { auto nd=std::make_shared<BoolLitNode>(); nd->val=false; init=nd; }
            else init=std::make_shared<NullLitNode>();
        }
        match(TT::SEMICOLON);
        auto nd=std::make_shared<VarDeclNode>(); nd->dtype=dtype; nd->name=name; nd->init=init; return nd;
    }
    NodePtr parseAssignOrExpr(){
        auto left=parseExpr();
        if(check(TT::ASSIGN)){ consume(); auto n=std::make_shared<AssignNode>(); n->target=left; n->value=parseExpr(); match(TT::SEMICOLON); return n; }
        match(TT::SEMICOLON); auto n=std::make_shared<ExprStmtNode>(); n->expr=left; return n;
    }
    NodePtr parseWith(){
        consume(); auto n=std::make_shared<WithNode>();
        n->filenameExpr=parseExpr(); expect(TT::COMMA,",");
        if(!check(TT::STR_LIT)) throw std::runtime_error("Line "+std::to_string(cur().line)+": expected mode string");
        n->mode=consume().val; n->encoding="utf-8";
        if(check(TT::COMMA)){ consume();
            if(check(TT::KW_ENCODING)||(check(TT::IDENT)&&cur().val=="encoding")){ consume(); expect(TT::ASSIGN,"=");
                if(!check(TT::STR_LIT)) throw std::runtime_error("expected encoding string");
                n->encoding=consume().val; }
        }
        expect(TT::KW_AS,"as"); n->alias=expect(TT::IDENT,"file alias").val; n->body=parseBlock(); return n;
    }

    // ── expressions ──────────────────────────
    NodePtr parseExpr(){ return parseOr(); }
    NodePtr parseOr(){
        auto left=parseAnd();
        while(check(TT::OR)||check(TT::KW_OR)){ consume(); auto r=parseAnd(); auto n=std::make_shared<BinOpNode>(); n->op="||"; n->left=left; n->right=r; left=n; }
        return left;
    }
    NodePtr parseAnd(){
        auto left=parseNot();
        while(check(TT::AND)||check(TT::KW_AND)){ consume(); auto r=parseNot(); auto n=std::make_shared<BinOpNode>(); n->op="&&"; n->left=left; n->right=r; left=n; }
        return left;
    }
    NodePtr parseNot(){
        if(check(TT::NOT)||check(TT::KW_NOT)){ consume(); auto n=std::make_shared<UnaryOpNode>(); n->op="!"; n->operand=parseNot(); return n; }
        return parseEquality();
    }
    NodePtr parseEquality(){
        auto left=parseRelational();
        while(check(TT::EQ)||check(TT::NEQ)){ auto op=consume().val; auto r=parseRelational(); auto n=std::make_shared<BinOpNode>(); n->op=op; n->left=left; n->right=r; left=n; }
        return left;
    }
    NodePtr parseRelational(){
        auto left=parseAddSub();
        while(check(TT::LT)||check(TT::GT)||check(TT::LTE)||check(TT::GTE)){ auto op=consume().val; auto r=parseAddSub(); auto n=std::make_shared<BinOpNode>(); n->op=op; n->left=left; n->right=r; left=n; }
        return left;
    }
    NodePtr parseAddSub(){
        auto left=parseMulDiv();
        while(check(TT::PLUS)||check(TT::MINUS)){ auto op=consume().val; auto r=parseMulDiv(); auto n=std::make_shared<BinOpNode>(); n->op=op; n->left=left; n->right=r; left=n; }
        return left;
    }
    NodePtr parseMulDiv(){
        auto left=parsePower();
        while(check(TT::STAR)||check(TT::SLASH)||check(TT::PERCENT)){ auto op=consume().val; auto r=parsePower(); auto n=std::make_shared<BinOpNode>(); n->op=op; n->left=left; n->right=r; left=n; }
        return left;
    }
    NodePtr parsePower(){
        auto left=parseUnary();
        if(check(TT::POWER)){ consume(); auto r=parsePower(); auto n=std::make_shared<BinOpNode>(); n->op="**"; n->left=left; n->right=r; return n; }
        return left;
    }
    NodePtr parseUnary(){
        if(check(TT::MINUS)){ consume(); auto n=std::make_shared<UnaryOpNode>(); n->op="-"; n->operand=parseUnary(); return n; }
        return parsePostfix();
    }
    NodePtr parsePostfix(){
        auto node=parsePrimary();
        while(true){
            if(check(TT::LBRACKET)){
                consume(); auto key=parseExpr(); expect(TT::RBRACKET,"]");
                auto n=std::make_shared<IndexNode>(); n->obj=node; n->key=key; node=n;
            } else if(check(TT::DOT)){
                consume();
                std::string method=expect(TT::IDENT,"method name").val;
                // method call: obj.method(args)
                if(check(TT::LPAREN)){
                    consume();
                    auto mc=std::make_shared<MethodCallNode>(); mc->object=node; mc->method=method;
                    while(!check(TT::RPAREN)&&!check(TT::END_OF_FILE)){
                        if(check(TT::KW_FN)){ // fn(params){body} callback
                            consume(); expect(TT::LPAREN,"(");
                            auto lm=std::make_shared<LambdaNode>();
                            while(!check(TT::RPAREN)&&!check(TT::END_OF_FILE)){ lm->params.push_back(expect(TT::IDENT,"param").val); match(TT::COMMA); }
                            expect(TT::RPAREN,")"); lm->body=parseBlock(); mc->callback=lm;
                        } else mc->args.push_back(parseExpr());
                        if(!match(TT::COMMA)) break;
                    }
                    expect(TT::RPAREN,")"); node=mc;
                } else {
                    // property access (e.g. arr.length without parens → treat as no-arg method call)
                    auto mc=std::make_shared<MethodCallNode>(); mc->object=node; mc->method=method; node=mc;
                }
            } else if(check(TT::PLUSPLUS)||check(TT::MINUSMINUS)){
                auto op=consume().val; auto n=std::make_shared<PostfixNode>(); n->op=op; n->operand=node; node=n;
            } else break;
        }
        return node;
    }
    NodePtr parsePrimary(){
        if(check(TT::INT_LIT))  { auto n=std::make_shared<IntLitNode>();   n->val=std::stoll(consume().val); return n; }
        if(check(TT::FLOAT_LIT)){ auto n=std::make_shared<FloatLitNode>(); n->val=std::stod(consume().val);  return n; }
        if(check(TT::STR_LIT)){
            auto tok=consume(); auto n=std::make_shared<StrLitNode>();
            if(tok.val.size()>=2&&tok.val[0]=='r'&&tok.val[1]=='\0'){ n->val=tok.val.substr(2); n->interp=true; }
            else n->val=tok.val;
            return n;
        }
        if(check(TT::BOOL_LIT)){ auto tok=consume(); auto n=std::make_shared<BoolLitNode>(); n->val=(tok.val=="true"); return n; }
        if(check(TT::NULL_LIT)){ consume(); return std::make_shared<NullLitNode>(); }
        if(check(TT::LBRACKET)) return parseArrayOrObj();
        if(check(TT::KW_NUMBER_LOOPER)){
            consume(); expect(TT::LPAREN,"("); auto n=std::make_shared<NumberLooperNode>();
            n->start=parseExpr(); expect(TT::COMMA,","); n->end_=parseExpr(); expect(TT::RPAREN,")"); return n;
        }
        if(check(TT::KW_DATA_TYPE)){
            consume(); expect(TT::LPAREN,"("); auto n=std::make_shared<DataTypeNode>(); n->expr=parseExpr(); expect(TT::RPAREN,")"); return n;
        }
        if(check(TT::KW_TYPEOF)){
            consume(); expect(TT::LPAREN,"("); auto n=std::make_shared<TypeOfNode>(); n->expr=parseExpr(); expect(TT::RPAREN,")"); return n;
        }
        if(check(TT::IDENT)){
            std::string name=consume().val;
            // function call: name(args)
            if(check(TT::LPAREN)){
                consume(); auto n=std::make_shared<FuncCallNode>(); n->name=name;
                while(!check(TT::RPAREN)&&!check(TT::END_OF_FILE)){ n->args.push_back(parseExpr()); if(!match(TT::COMMA)) break; }
                expect(TT::RPAREN,")"); return n;
            }
            auto n=std::make_shared<IdentNode>(); n->name=name; return n;
        }
        if(check(TT::LPAREN)){ consume(); auto e=parseExpr(); expect(TT::RPAREN,")"); return e; }
        throw std::runtime_error("Line "+std::to_string(cur().line)+": unexpected token '"+cur().val+"'");
    }
    NodePtr parseArrayOrObj(){
        int bline=cur().line; expect(TT::LBRACKET,"[");
        if(check(TT::RBRACKET)){ consume(); return std::make_shared<ArrayLitNode>(); }
        bool isObj=false;
        if(isTypeKw()){ size_t saved=pos; pos++; if(check(TT::STR_LIT)||check(TT::IDENT)) pos++; if(check(TT::FAT_ARROW)) isObj=true; pos=saved; }
        if(isObj) return parseObjBody(bline);
        auto arr=std::make_shared<ArrayLitNode>();
        while(!check(TT::RBRACKET)&&!check(TT::END_OF_FILE)){
            auto elem=parseExpr();
            if(check(TT::FAT_ARROW)) throw std::runtime_error("Line "+std::to_string(cur().line)+": '=>' inside array — did you mean obj?");
            arr->elems.push_back(elem); if(!match(TT::COMMA)) break;
        }
        expect(TT::RBRACKET,"]"); return arr;
    }
    NodePtr parseObjBody(int){
        auto obj=std::make_shared<ObjLitNode>();
        while(!check(TT::RBRACKET)&&!check(TT::END_OF_FILE)){
            if(!isTypeKw()) throw std::runtime_error("Line "+std::to_string(cur().line)+": obj field must start with type keyword, got '"+cur().val+"'");
            std::string th=consume().val; std::string key;
            if(check(TT::STR_LIT)) key=consume().val; else key=expect(TT::IDENT,"key").val;
            expect(TT::FAT_ARROW,"=>"); auto val=parseExpr();
            obj->pairs.push_back({key,th,val}); if(!match(TT::COMMA)) break;
        }
        expect(TT::RBRACKET,"]"); return obj;
    }
};

// ═══════════════════════════════════════════════
//  ENVIRONMENT
// ═══════════════════════════════════════════════
struct Environment {
    std::map<std::string,ValuePtr> vars;
    std::shared_ptr<Environment> parent;
    Environment(std::shared_ptr<Environment> p=nullptr):parent(p){}
    void set(const std::string& name,ValuePtr val){
        for(Environment* e=this;e;e=e->parent.get()) if(e->vars.count(name)){ e->vars[name]=val; return; }
        vars[name]=val;
    }
    ValuePtr get(const std::string& name){
        for(Environment* e=this;e;e=e->parent.get()){ auto it=e->vars.find(name); if(it!=e->vars.end()) return it->second; }
        throw std::runtime_error("Undefined variable '"+name+"'");
    }
    void define(const std::string& name,ValuePtr val){ vars[name]=val; }
};

// ═══════════════════════════════════════════════
//  FILE HANDLE
// ═══════════════════════════════════════════════
struct FileHandle {
    std::string filename,mode,encoding; std::fstream fs;
    void open(){
        std::ios::openmode om=std::ios::in;
        if(mode=="w") om=std::ios::out|std::ios::trunc;
        else if(mode=="a") om=std::ios::out|std::ios::app;
        else if(mode=="w+") om=std::ios::in|std::ios::out|std::ios::trunc;
        else if(mode=="r+") om=std::ios::in|std::ios::out;
        fs.open(filename,om);
        if(!fs.is_open()) throw std::runtime_error("Cannot open '"+filename+"' mode='"+mode+"'");
    }
    void close(){ fs.close(); }
    ValuePtr read(){ std::ostringstream ss; ss<<fs.rdbuf(); return std::make_shared<PyPPValue>(ss.str()); }
    ValuePtr readLines(){ auto arr=PyPPValue::makeArray(); std::string ln; while(std::getline(fs,ln)) arr->arr.push_back(std::make_shared<PyPPValue>(ln)); return arr; }
    ValuePtr readLine(){ std::string ln; if(std::getline(fs,ln)) return std::make_shared<PyPPValue>(ln); return PyPPValue::makeNil(); }
    void write(const std::string& s){ fs<<s; }
    void writeLine(const std::string& s){ fs<<s<<"\n"; }
};

// ═══════════════════════════════════════════════
//  VENV / MODULE SYSTEM
// ═══════════════════════════════════════════════
static std::string g_venv_active; // set by pypp virtual activate or by PYPP_VENV env var

static std::string findModulePath(const std::string& modName){
    // 1. Check active venv packages dir
    if(!g_venv_active.empty()){
        std::string p=g_venv_active+"/packages/"+modName+".pypp";
        if(std::ifstream(p).good()) return p;
    }
    // 2. Check PYPP_VENV environment variable
    const char* ev=std::getenv("PYPP_VENV");
    if(ev){
        std::string p=std::string(ev)+"/packages/"+modName+".pypp";
        if(std::ifstream(p).good()) return p;
    }
    // 3. Check ./packages/ relative to cwd
    {
        std::string p="packages/"+modName+".pypp";
        if(std::ifstream(p).good()) return p;
    }
    return "";
}

// ═══════════════════════════════════════════════
//  INTERPRETER
// ═══════════════════════════════════════════════
struct Interpreter {
    EnvPtr global=std::make_shared<Environment>();

    void run(std::shared_ptr<ProgramNode> prog){ for(auto& s:prog->stmts) execStmt(s,global); }

    static void checkDeclType(const std::string& dtype,const std::string& name,ValuePtr val){
        if(val->type==VType::NIL) return;
        if(dtype=="float"&&val->type==VType::INT) return;
        auto got=val->typeName();
        if(dtype!=got) throw std::runtime_error("Type error: '"+name+"' declared as "+dtype+" but got "+got+" ("+val->toString()+")");
    }

    void execStmt(NodePtr node,EnvPtr env){
        std::string k=node->kind();
        if(k=="FuncDecl"){
            auto n=std::static_pointer_cast<FuncDeclNode>(node);
            auto uf=std::make_shared<UserFunc>(); uf->params=n->params; uf->body=n->body; uf->closure=env;
            env->define(n->name,PyPPValue::makeUser(uf));
        }
        else if(k=="Return"){
            auto n=std::static_pointer_cast<ReturnNode>(node);
            ValuePtr val=n->expr?evalExpr(n->expr,env):PyPPValue::makeNil();
            throw ReturnSignal(val);
        }
        else if(k=="VarDecl"){
            auto n=std::static_pointer_cast<VarDeclNode>(node);
            auto val=evalExpr(n->init,env); checkDeclType(n->dtype,n->name,val); env->define(n->name,val);
        }
        else if(k=="Assign"){ auto n=std::static_pointer_cast<AssignNode>(node); setValue(n->target,evalExpr(n->value,env),env); }
        else if(k=="Say"){
            auto n=std::static_pointer_cast<SayNode>(node);
            std::cout<<evalExpr(n->expr,env)->toString()<<"\n";
        }
        else if(k=="VarDumper"){ auto n=std::static_pointer_cast<VarDumperNode>(node); std::cout<<evalExpr(n->expr,env)->varDump()<<"\n"; }
        else if(k=="If"){
            auto n=std::static_pointer_cast<IfNode>(node);
            if(evalExpr(n->cond,env)->isTruthy()) execStmt(n->then_,std::make_shared<Environment>(env));
            else if(n->else_) execStmt(n->else_,std::make_shared<Environment>(env));
        }
        else if(k=="Block"){ auto n=std::static_pointer_cast<BlockNode>(node); for(auto& s:n->stmts) execStmt(s,env); }
        else if(k=="ForIn"){
            auto n=std::static_pointer_cast<ForInNode>(node); auto iter=evalExpr(n->iter,env);
            auto child=std::make_shared<Environment>(env);
            if(iter->type==VType::ARRAY){ for(auto& v:iter->arr){ child->define(n->var,v); execStmt(n->body,child); } }
            else if(iter->type==VType::OBJ){ for(auto& p:iter->obj){ child->define(n->var,std::make_shared<PyPPValue>(p.first)); execStmt(n->body,child); } }
            else if(iter->type==VType::STR){ for(char c:iter->sval){ child->define(n->var,std::make_shared<PyPPValue>(std::string(1,c))); execStmt(n->body,child); } }
            else throw std::runtime_error("for..in requires array/obj/str");
        }
        else if(k=="While"){ auto n=std::static_pointer_cast<WhileNode>(node); auto child=std::make_shared<Environment>(env); while(evalExpr(n->cond,child)->isTruthy()) execStmt(n->body,child); }
        else if(k=="ExprStmt"){ evalExpr(std::static_pointer_cast<ExprStmtNode>(node)->expr,env); }
        else if(k=="With"){ execWith(std::static_pointer_cast<WithNode>(node),env); }
        else if(k=="Import"){ execImport(std::static_pointer_cast<ImportNode>(node),env); }
    }

    void execWith(std::shared_ptr<WithNode> node,EnvPtr env){
        FileHandle fh; fh.filename=evalExpr(node->filenameExpr,env)->toString(); fh.mode=node->mode; fh.encoding=node->encoding; fh.open();
        auto child=std::make_shared<Environment>(env);
        auto fo=PyPPValue::makeObj();
        fo->obj["read"]      =PyPPValue::makeNative([&fh](std::vector<ValuePtr>)->ValuePtr{ return fh.read(); });
        fo->obj["readLines"] =PyPPValue::makeNative([&fh](std::vector<ValuePtr>)->ValuePtr{ return fh.readLines(); });
        fo->obj["readLine"]  =PyPPValue::makeNative([&fh](std::vector<ValuePtr>)->ValuePtr{ return fh.readLine(); });
        fo->obj["write"]     =PyPPValue::makeNative([&fh](std::vector<ValuePtr> a)->ValuePtr{ if(!a.empty()) fh.write(a[0]->toString()); return PyPPValue::makeNil(); });
        fo->obj["writeLine"] =PyPPValue::makeNative([&fh](std::vector<ValuePtr> a)->ValuePtr{ if(!a.empty()) fh.writeLine(a[0]->toString()); return PyPPValue::makeNil(); });
        fo->obj["close"]     =PyPPValue::makeNative([&fh](std::vector<ValuePtr>)->ValuePtr{ fh.close(); return PyPPValue::makeNil(); });
        child->define(node->alias,fo);
        execStmt(node->body,child);
        fh.close();
    }

    void execImport(std::shared_ptr<ImportNode> node,EnvPtr env){
        std::string path=findModulePath(node->module);
        if(path.empty()) throw std::runtime_error("Module '"+node->module+"' not found. Install it first with: pypp virtual install "+node->module);
        std::ifstream f(path);
        if(!f) throw std::runtime_error("Cannot open module file: "+path);
        std::ostringstream ss; ss<<f.rdbuf();
        // run module in isolated env
        auto modEnv=std::make_shared<Environment>(global);

        // Auto-seed with high-entropy time value so every run is different.
        {
            auto t1 = std::chrono::system_clock::now().time_since_epoch().count();
            auto t2 = std::chrono::steady_clock::now().time_since_epoch().count();
            static long long counter = 0;
            long long seed = t1 ^ (t2 * 2654435761LL) ^ (++counter * 6364136223846793005LL);
            if(seed < 0) seed = -seed;
            if(seed == 0) seed = 987654321LL;
            modEnv->define("__seed", std::make_shared<PyPPValue>(seed));
        }

        auto tokens2=lex(ss.str()); Parser p2; p2.toks=tokens2;
        auto ast2=p2.parse();
        for(auto& s:ast2->stmts) execStmt(s,modEnv);
        // Build module obj: each var becomes a member.
        // Functions are wrapped as native fns that call through the interpreter
        // with the module's env so that shared state (__seed etc.) is preserved.
        auto modObj=PyPPValue::makeObj();
        for(auto& kv:modEnv->vars){
            if(kv.second->type==VType::USERFUNC){
                // capture the UserFunc and the modEnv
                auto uf=kv.second;
                auto menv=modEnv;
                auto interp=this;
                modObj->obj[kv.first]=PyPPValue::makeNative([uf,menv,interp](std::vector<ValuePtr> args)->ValuePtr{
                    return interp->callFunction(uf,args,menv);
                });
            } else {
                modObj->obj[kv.first]=kv.second;
            }
        }
        env->define(node->alias,modObj);
    }

    void setValue(NodePtr target,ValuePtr val,EnvPtr env){
        if(target->kind()=="Ident") env->set(std::static_pointer_cast<IdentNode>(target)->name,val);
        else if(target->kind()=="Index"){
            auto n=std::static_pointer_cast<IndexNode>(target); auto obj=evalExpr(n->obj,env); auto key=evalExpr(n->key,env);
            if(obj->type==VType::ARRAY){ if(key->type!=VType::INT) throw std::runtime_error("Array index must be int"); size_t idx=(size_t)key->ival; while(obj->arr.size()<=idx) obj->arr.push_back(PyPPValue::makeNil()); obj->arr[idx]=val; }
            else if(obj->type==VType::OBJ) obj->obj[key->toString()]=val;
        }
    }

    std::string interpolate(const std::string& tmpl,EnvPtr env){
        std::string result;
        for(size_t i=0;i<tmpl.size();){
            if(tmpl[i]=='\x01'){ i++; std::string vn; while(i<tmpl.size()&&tmpl[i]!='\x02') vn+=tmpl[i++]; if(i<tmpl.size()) i++;
                try{ result+=env->get(vn)->toString(); } catch(...){ result+="{"+vn+"}"; }
            } else result+=tmpl[i++];
        }
        return result;
    }

    ValuePtr evalExpr(NodePtr node,EnvPtr env){
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

        if(k=="ConcatChain"){
            // say "hi" . x . " bye"  → string concat all parts
            auto n=std::static_pointer_cast<ConcatChainNode>(node);
            std::string result;
            for(auto& part:n->parts) result+=evalExpr(part,env)->toString();
            return std::make_shared<PyPPValue>(result);
        }

        // user function call: name(args)
        if(k=="FuncCall"){
            auto n=std::static_pointer_cast<FuncCallNode>(node);
            // look up function
            ValuePtr fn=env->get(n->name);
            std::vector<ValuePtr> args;
            for(auto& a:n->args) args.push_back(evalExpr(a,env));
            return callFunction(fn,args,env);
        }

        if(k=="ArrayLit"){
            auto n=std::static_pointer_cast<ArrayLitNode>(node); auto arr=PyPPValue::makeArray();
            for(auto& e:n->elems) arr->arr.push_back(evalExpr(e,env)); return arr;
        }
        if(k=="ObjLit"){
            auto n=std::static_pointer_cast<ObjLitNode>(node); auto obj=PyPPValue::makeObj();
            for(auto& p:n->pairs){
                const std::string& key=std::get<0>(p); const std::string& th=std::get<1>(p);
                auto val=evalExpr(std::get<2>(p),env); std::string got=val->typeName();
                bool ok=(th==got)||(th=="float"&&got=="int");
                if(!ok&&val->type!=VType::NIL) throw std::runtime_error("Type error in obj field \""+key+"\": declared "+th+" but got "+got+" ("+val->toString()+")");
                obj->obj[key]=val;
            }
            return obj;
        }
        if(k=="Index"){
            auto n=std::static_pointer_cast<IndexNode>(node); auto obj=evalExpr(n->obj,env); auto key=evalExpr(n->key,env);
            if(obj->type==VType::ARRAY){ if(key->type!=VType::INT) throw std::runtime_error("Array index must be int"); size_t idx=(size_t)key->ival; if(idx>=obj->arr.size()) return PyPPValue::makeNil(); return obj->arr[idx]; }
            if(obj->type==VType::OBJ){ auto it=obj->obj.find(key->toString()); if(it==obj->obj.end()) return PyPPValue::makeNil(); return it->second; }
            throw std::runtime_error("Cannot index a "+obj->typeName());
        }
        if(k=="NumberLooper"){
            auto n=std::static_pointer_cast<NumberLooperNode>(node); auto s=evalExpr(n->start,env),e=evalExpr(n->end_,env);
            auto arr=PyPPValue::makeArray(); for(long long x=s->ival;x<e->ival;x++) arr->arr.push_back(std::make_shared<PyPPValue>(x)); return arr;
        }
        if(k=="DataType") return std::make_shared<PyPPValue>(evalExpr(std::static_pointer_cast<DataTypeNode>(node)->expr,env)->typeName());
        if(k=="TypeOf")   return std::make_shared<PyPPValue>(evalExpr(std::static_pointer_cast<TypeOfNode>(node)->expr,env)->typeName());

        if(k=="MethodCall"){
            auto n=std::static_pointer_cast<MethodCallNode>(node); auto obj=evalExpr(n->object,env);
            // file/obj native method OR user function stored in obj
            if(obj->type==VType::OBJ){ auto it=obj->obj.find(n->method); if(it!=obj->obj.end()){ std::vector<ValuePtr> args; for(auto& a:n->args) args.push_back(evalExpr(a,env)); return callFunction(it->second,args,env); } }
            // array higher-order
            if(obj->type==VType::ARRAY&&n->callback){
                auto lm=std::static_pointer_cast<LambdaNode>(n->callback); return evalHigherOrder(n->method,obj,lm,env);
            }
            // array / str
            std::vector<ValuePtr> args; for(auto& a:n->args) args.push_back(evalExpr(a,env));
            if(obj->type==VType::STR) return strMethod(obj,n->method,args);
            if(obj->type==VType::ARRAY) return arrayMethod(obj,n->method,args);
            throw std::runtime_error("Cannot call method '"+n->method+"' on "+obj->typeName());
        }

        if(k=="BinOp"){
            auto n=std::static_pointer_cast<BinOpNode>(node);
            if(n->op=="&&"){ auto L=evalExpr(n->left,env); if(!L->isTruthy()) return std::make_shared<PyPPValue>(false); return std::make_shared<PyPPValue>((bool)evalExpr(n->right,env)->isTruthy()); }
            if(n->op=="||"){ auto L=evalExpr(n->left,env); if(L->isTruthy()) return std::make_shared<PyPPValue>(true);  return std::make_shared<PyPPValue>((bool)evalExpr(n->right,env)->isTruthy()); }
            return applyBinOp(n->op,evalExpr(n->left,env),evalExpr(n->right,env));
        }
        if(k=="UnaryOp"){
            auto n=std::static_pointer_cast<UnaryOpNode>(node); auto v=evalExpr(n->operand,env);
            if(n->op=="!") return std::make_shared<PyPPValue>((bool)!v->isTruthy());
            if(n->op=="-"){ if(v->type==VType::INT) return std::make_shared<PyPPValue>(-v->ival); if(v->type==VType::FLOAT) return std::make_shared<PyPPValue>(-v->fval); }
            return v;
        }
        if(k=="Postfix"){
            auto n=std::static_pointer_cast<PostfixNode>(node); auto v=evalExpr(n->operand,env);
            ValuePtr nv;
            if(v->type==VType::INT)   nv=std::make_shared<PyPPValue>(v->ival+(n->op=="++"?1LL:-1LL));
            else if(v->type==VType::FLOAT) nv=std::make_shared<PyPPValue>(v->fval+(n->op=="++"?1.0:-1.0));
            else nv=v;
            setValue(n->operand,nv,env); return v;
        }
        throw std::runtime_error("Unknown expr node: "+k);
    }

    // ── call a function value ─────────────────
    ValuePtr callFunction(ValuePtr fn,const std::vector<ValuePtr>& args,EnvPtr callerEnv){
        (void)callerEnv;
        if(fn->type==VType::FUNC) return fn->nfn(args);
        if(fn->type==VType::USERFUNC){
            auto& uf=fn->ufn;
            auto callEnv=std::make_shared<Environment>(uf->closure);
            for(size_t i=0;i<uf->params.size();i++)
                callEnv->define(uf->params[i], i<args.size()?args[i]:PyPPValue::makeNil());
            try { execStmt(uf->body,callEnv); }
            catch(ReturnSignal& rs){ return rs.value; }
            return PyPPValue::makeNil();
        }
        throw std::runtime_error("'"+fn->typeName()+"' is not callable");
    }

    // ── higher-order array methods ────────────
    ValuePtr evalHigherOrder(const std::string& method,ValuePtr arr,std::shared_ptr<LambdaNode> lm,EnvPtr env){
        auto callCb=[&](std::vector<ValuePtr> cbArgs)->ValuePtr{
            auto cbEnv=std::make_shared<Environment>(env);
            for(size_t i=0;i<lm->params.size();i++) cbEnv->define(lm->params[i],i<cbArgs.size()?cbArgs[i]:PyPPValue::makeNil());
            auto blk=std::static_pointer_cast<BlockNode>(lm->body);
            ValuePtr last=PyPPValue::makeNil();
            for(auto& s:blk->stmts){
                if(s->kind()=="Return"){ auto rn=std::static_pointer_cast<ReturnNode>(s); return rn->expr?evalExpr(rn->expr,cbEnv):PyPPValue::makeNil(); }
                if(s->kind()=="ExprStmt") last=evalExpr(std::static_pointer_cast<ExprStmtNode>(s)->expr,cbEnv);
                else execStmt(s,cbEnv);
            }
            return last;
        };
        ArrVec& a=arr->arr;
        if(method=="forEach"){ for(size_t i=0;i<a.size();i++) callCb({a[i],std::make_shared<PyPPValue>((long long)i),arr}); return PyPPValue::makeNil(); }
        if(method=="map"){ auto res=PyPPValue::makeArray(); for(size_t i=0;i<a.size();i++) res->arr.push_back(callCb({a[i],std::make_shared<PyPPValue>((long long)i),arr})); return res; }
        if(method=="filter"){ auto res=PyPPValue::makeArray(); for(size_t i=0;i<a.size();i++) if(callCb({a[i],std::make_shared<PyPPValue>((long long)i),arr})->isTruthy()) res->arr.push_back(a[i]); return res; }
        if(method=="find"){ for(size_t i=0;i<a.size();i++) if(callCb({a[i],std::make_shared<PyPPValue>((long long)i),arr})->isTruthy()) return a[i]; return PyPPValue::makeNil(); }
        if(method=="findIndex"){ for(size_t i=0;i<a.size();i++) if(callCb({a[i],std::make_shared<PyPPValue>((long long)i),arr})->isTruthy()) return std::make_shared<PyPPValue>((long long)i); return std::make_shared<PyPPValue>(-1LL); }
        if(method=="every"){ for(size_t i=0;i<a.size();i++) if(!callCb({a[i],std::make_shared<PyPPValue>((long long)i),arr})->isTruthy()) return std::make_shared<PyPPValue>(false); return std::make_shared<PyPPValue>(true); }
        if(method=="some"){ for(size_t i=0;i<a.size();i++) if(callCb({a[i],std::make_shared<PyPPValue>((long long)i),arr})->isTruthy()) return std::make_shared<PyPPValue>(true); return std::make_shared<PyPPValue>(false); }
        if(method=="reduce"){ if(a.empty()) return PyPPValue::makeNil(); ValuePtr acc=a[0]; for(size_t i=1;i<a.size();i++) acc=callCb({acc,a[i],std::make_shared<PyPPValue>((long long)i),arr}); return acc; }
        throw std::runtime_error("Unknown higher-order method '"+method+"'");
    }

    // ── string methods ────────────────────────
    ValuePtr strMethod(ValuePtr sv,const std::string& method,const std::vector<ValuePtr>& args){
        std::string& s=sv->sval;
        if(method=="length")      return std::make_shared<PyPPValue>((long long)s.size());
        if(method=="toUpperCase"){ std::string r=s; for(auto& c:r) c=toupper((unsigned char)c); return std::make_shared<PyPPValue>(r); }
        if(method=="toLowerCase"){ std::string r=s; for(auto& c:r) c=tolower((unsigned char)c); return std::make_shared<PyPPValue>(r); }
        if(method=="trim"){ size_t a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n"); if(a==std::string::npos) return std::make_shared<PyPPValue>(std::string()); return std::make_shared<PyPPValue>(s.substr(a,b-a+1)); }
        if(method=="includes"){ if(args.empty()) return std::make_shared<PyPPValue>(false); return std::make_shared<PyPPValue>((bool)(s.find(args[0]->toString())!=std::string::npos)); }
        if(method=="startsWith"){ if(args.empty()) return std::make_shared<PyPPValue>(false); std::string p=args[0]->toString(); return std::make_shared<PyPPValue>((bool)(s.rfind(p,0)==0)); }
        if(method=="endsWith"){ if(args.empty()) return std::make_shared<PyPPValue>(false); std::string p=args[0]->toString(); if(p.size()>s.size()) return std::make_shared<PyPPValue>(false); return std::make_shared<PyPPValue>((bool)(s.compare(s.size()-p.size(),p.size(),p)==0)); }
        if(method=="indexOf"){ if(args.empty()) return std::make_shared<PyPPValue>(-1LL); auto pos=s.find(args[0]->toString()); return std::make_shared<PyPPValue>(pos==std::string::npos?-1LL:(long long)pos); }
        if(method=="slice"){ long long sz=(long long)s.size(),start=args.empty()?0:args[0]->ival,end=sz; if(args.size()>1) end=args[1]->ival; if(start<0)start=std::max(0LL,sz+start); if(end<0)end=std::max(0LL,sz+end); start=std::max(0LL,std::min(start,sz)); end=std::max(0LL,std::min(end,sz)); return std::make_shared<PyPPValue>(s.substr(start,end-start)); }
        if(method=="split"){ std::string sep=args.empty()?"":(args[0]->type==VType::NIL?"":args[0]->toString()); auto res=PyPPValue::makeArray(); if(sep.empty()){ for(char c:s) res->arr.push_back(std::make_shared<PyPPValue>(std::string(1,c))); } else { size_t pos=0,found; while((found=s.find(sep,pos))!=std::string::npos){ res->arr.push_back(std::make_shared<PyPPValue>(s.substr(pos,found-pos))); pos=found+sep.size(); } res->arr.push_back(std::make_shared<PyPPValue>(s.substr(pos))); } return res; }
        if(method=="replace"){ if(args.size()<2) return sv; std::string from=args[0]->toString(),to=args[1]->toString(),r=s; auto pos=r.find(from); if(pos!=std::string::npos) r.replace(pos,from.size(),to); return std::make_shared<PyPPValue>(r); }
        if(method=="replaceAll"){ if(args.size()<2) return sv; std::string from=args[0]->toString(),to=args[1]->toString(),r=s; size_t pos=0; while((pos=r.find(from,pos))!=std::string::npos){ r.replace(pos,from.size(),to); pos+=to.size(); } return std::make_shared<PyPPValue>(r); }
        if(method=="repeat"){ long long n=args.empty()?0:args[0]->ival; std::string r; for(long long i=0;i<n;i++) r+=s; return std::make_shared<PyPPValue>(r); }
        if(method=="charAt"){ long long idx=args.empty()?0:args[0]->ival; if(idx<0||idx>=(long long)s.size()) return std::make_shared<PyPPValue>(std::string()); return std::make_shared<PyPPValue>(std::string(1,s[idx])); }
        if(method=="toString") return sv;
        throw std::runtime_error("Unknown string method '"+method+"'");
    }

    ValuePtr applyBinOp(const std::string& op,ValuePtr L,ValuePtr R){
        if(op=="+"&&(L->type==VType::STR||R->type==VType::STR)) return std::make_shared<PyPPValue>(L->toString()+R->toString());
        if(op=="**"){ double base=L->type==VType::INT?(double)L->ival:L->fval,exp=R->type==VType::INT?(double)R->ival:R->fval,res=std::pow(base,exp); if(L->type==VType::INT&&R->type==VType::INT&&exp>=0) return std::make_shared<PyPPValue>((long long)std::llround(res)); return std::make_shared<PyPPValue>(res); }
        bool isF=(L->type==VType::FLOAT||R->type==VType::FLOAT);
        double lf=L->type==VType::INT?(double)L->ival:L->fval, rf=R->type==VType::INT?(double)R->ival:R->fval;
        long long li=L->type==VType::INT?L->ival:(long long)L->fval, ri=R->type==VType::INT?R->ival:(long long)R->fval;
        if(op=="+") return isF?std::make_shared<PyPPValue>(lf+rf):std::make_shared<PyPPValue>(li+ri);
        if(op=="-") return isF?std::make_shared<PyPPValue>(lf-rf):std::make_shared<PyPPValue>(li-ri);
        if(op=="*") return isF?std::make_shared<PyPPValue>(lf*rf):std::make_shared<PyPPValue>(li*ri);
        if(op=="/"){ if(rf==0) throw std::runtime_error("Division by zero"); return isF?std::make_shared<PyPPValue>(lf/rf):std::make_shared<PyPPValue>(li/ri); }
        if(op=="%") return std::make_shared<PyPPValue>(li%ri);
        if(op=="=="){ if(L->type==VType::NIL&&R->type==VType::NIL) return std::make_shared<PyPPValue>(true); if(L->type==VType::NIL||R->type==VType::NIL) return std::make_shared<PyPPValue>(false); if(L->type==VType::STR||R->type==VType::STR) return std::make_shared<PyPPValue>((bool)(L->toString()==R->toString())); if(L->type==VType::BOOL||R->type==VType::BOOL) return std::make_shared<PyPPValue>((bool)(L->isTruthy()==R->isTruthy())); return std::make_shared<PyPPValue>((bool)(lf==rf)); }
        if(op=="!="){ if(L->type==VType::NIL&&R->type==VType::NIL) return std::make_shared<PyPPValue>(false); if(L->type==VType::NIL||R->type==VType::NIL) return std::make_shared<PyPPValue>(true); if(L->type==VType::STR||R->type==VType::STR) return std::make_shared<PyPPValue>((bool)(L->toString()!=R->toString())); return std::make_shared<PyPPValue>((bool)(lf!=rf)); }
        if(op=="<") return std::make_shared<PyPPValue>((bool)(lf<rf));
        if(op==">") return std::make_shared<PyPPValue>((bool)(lf>rf));
        if(op=="<=")return std::make_shared<PyPPValue>((bool)(lf<=rf));
        if(op==">=")return std::make_shared<PyPPValue>((bool)(lf>=rf));
        throw std::runtime_error("Unknown operator: "+op);
    }
};

// ═══════════════════════════════════════════════
//  CROSS-PLATFORM HELPERS
// ═══════════════════════════════════════════════

static std::string absPath(const std::string& rel){
    try{ return fs::absolute(rel).string(); }
    catch(...){ return rel; }
}
static bool makeDirs(const std::string& path){
    try{ fs::create_directories(path); return true; }
    catch(...){ return false; }
}
static bool copyFile(const std::string& src,const std::string& dst){
    try{ fs::copy_file(src,dst,fs::copy_options::overwrite_existing); return true; }
    catch(...){ return false; }
}

// ═══════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════
int main(int argc,char* argv[]){

    if(argc>=3 && std::string(argv[1])=="virtual"){
        std::string vname   = argv[2];
        std::string venvDir = ".pypp_envs/" + vname;
        std::string pkgDir  = venvDir + "/packages";
        std::string subcmd  = (argc>=4) ? argv[3] : "";

        // ── CREATE ──────────────────────────────────────────────────
        if(subcmd.empty()){
            if(!makeDirs(pkgDir)){
                std::cerr<<"[PyPP] Error: could not create '"<<pkgDir<<"'.\n";
                std::cerr<<"       Make sure you have write permission here.\n";
                return 1;
            }
            std::string absVenv = absPath(venvDir);

            // PowerShell
            { std::ofstream ps(venvDir+"/activate.ps1");
              ps<<"# PyPP virtual environment activation (PowerShell)\n";
              ps<<"$env:PYPP_VENV = \""<<absVenv<<"\"\n";
              ps<<"$env:PYPP_VENV_NAME = \""<<vname<<"\"\n";
              ps<<"# Patch the prompt to show (PYPP"<<vname<<")\n";
              ps<<"function global:prompt {\n";
              ps<<"    Write-Host \"(PYPP"<<vname<<")\" -NoNewline -ForegroundColor Cyan\n";
              ps<<"    Write-Host \" PS $($executionContext.SessionState.Path.CurrentLocation)\" -NoNewline\n";
              ps<<"    return \"> \"\n";
              ps<<"}\n";
              ps<<"Write-Host \"[PyPP] Virtual environment '"<<vname<<"' activated.\" -ForegroundColor Green\n";
              ps<<"Write-Host \"       PYPP_VENV = $env:PYPP_VENV\"\n";
              ps<<"Write-Host \"       Prompt  : (PYPP"<<vname<<")\"\n"; }
            // CMD
            { std::ofstream bat(venvDir+"/activate.bat");
              bat<<"@echo off\n";
              bat<<"set PYPP_VENV="<<absVenv<<"\n";
              bat<<"set PYPP_VENV_NAME="<<vname<<"\n";
              bat<<"set PROMPT=(PYPP"<<vname<<") $P$G\n";
              bat<<"echo [PyPP] Virtual environment '"<<vname<<"' activated.\n";
              bat<<"echo        PYPP_VENV=%PYPP_VENV%\n"; }
            // Bash
            { std::ofstream sh(venvDir+"/activate.sh");
              sh<<"#!/usr/bin/env bash\n";
              sh<<"export PYPP_VENV=\""<<absVenv<<"\"\n";
              sh<<"export PYPP_VENV_NAME=\""<<vname<<"\"\n";
              sh<<"export PS1=\"(PYPP"<<vname<<") $PS1\"\n";
              sh<<"echo \"[PyPP] Virtual environment '"<<vname<<"' activated.\"\n";
              sh<<"echo \"       PYPP_VENV=$PYPP_VENV\"\n"; }

            std::cout<<"[PyPP] Virtual environment '"<<vname<<"' created.\n\n";
            std::cout<<"  Location : "<<absVenv<<"\n\n";
            std::cout<<"  Activate (PowerShell) : .  "<<venvDir<<"\\activate.ps1\n";
            std::cout<<"  Activate (CMD)        : "<<venvDir<<"\\activate.bat\n";
            std::cout<<"  Activate (bash/zsh)   : source "<<venvDir<<"/activate.sh\n\n";
            std::cout<<"  After activation your prompt becomes: (PYPP"<<vname<<") PS ...>\n\n";
            std::cout<<"  Install a package     : .\\pypp virtual "<<vname<<" install <module>\n";
            return 0;
        }

        // ── ACTIVATE ────────────────────────────────────────────────
        if(subcmd=="activate"){
            if(!fs::exists(venvDir)){
                std::cerr<<"[PyPP] Venv '"<<vname<<"' does not exist. Create it first:\n";
                std::cerr<<"       .\\pypp virtual "<<vname<<"\n"; return 1;
            }
            std::string absVenv=absPath(venvDir);
            std::cout<<"[PyPP] To activate '"<<vname<<"', run ONE of:\n\n";
            std::cout<<"  PowerShell : .  "<<venvDir<<"\\activate.ps1\n";
            std::cout<<"  CMD        : "<<venvDir<<"\\activate.bat\n";
            std::cout<<"  bash/zsh   : source "<<venvDir<<"/activate.sh\n\n";
            std::cout<<"  (Sets PYPP_VENV="<<absVenv<<" and prompt to (PYPP"<<vname<<"))\n";
            return 0;
        }

        // ── INSTALL ─────────────────────────────────────────────────
        if(subcmd=="install"){
            if(argc<5){ std::cerr<<"[PyPP] Usage: .\\pypp virtual "<<vname<<" install <module>\n"; return 1; }
            std::string modName=argv[4];
            if(!fs::exists(venvDir)){
                std::cerr<<"[PyPP] Venv '"<<vname<<"' does not exist. Create it first:\n";
                std::cerr<<"       .\\pypp virtual "<<vname<<"\n"; return 1;
            }
            makeDirs(pkgDir);
            std::string dest=pkgDir+"/"+modName+".pypp";
            if(fs::exists(dest)){
                std::cout<<"[PyPP] '"<<modName<<"' is already installed in '"<<vname<<"'.\n"; return 0;
            }
            // Check embedded built-in modules (no external files needed)
            auto bit = BUILTIN_MODULES.find(modName);
            if(bit != BUILTIN_MODULES.end()){
                std::ofstream out(dest);
                if(!out){ std::cerr<<"[PyPP] Error writing module to venv.\n"; return 1; }
                out << *(bit->second);
                std::cout<<"[PyPP] Installed '"<<modName<<"' into venv '"<<vname<<"'.\n";
                std::cout<<"       You can now use:  import "<<modName<<"\n";
                return 0;
            }
            std::cerr<<"[PyPP] Unknown module '"<<modName<<"'.\n";
            std::cerr<<"       Available built-in modules:";
            for(auto& bm: BUILTIN_MODULES) std::cerr<<" "<<bm.first;
            std::cerr<<"\n";
            return 1;
        }

        // ── LIST ────────────────────────────────────────────────────
        if(subcmd=="list"){
            if(!fs::exists(pkgDir)){ std::cerr<<"[PyPP] Venv '"<<vname<<"' does not exist.\n"; return 1; }
            std::cout<<"[PyPP] Packages in '"<<vname<<"':\n";
            bool any=false;
            for(auto& e:fs::directory_iterator(pkgDir)){
                if(e.path().extension()==".pypp"){ std::cout<<"  - "<<e.path().stem().string()<<"\n"; any=true; }
            }
            if(!any) std::cout<<"  (none installed)\n";
            return 0;
        }

        std::cerr<<"[PyPP] Unknown sub-command '"<<subcmd<<"'.\n\n";
        std::cerr<<"Usage:\n";
        std::cerr<<"  .\\pypp virtual <n>                 Create a venv\n";
        std::cerr<<"  .\\pypp virtual <n> activate        Show activation help\n";
        std::cerr<<"  .\\pypp virtual <n> install <mod>   Install a module\n";
        std::cerr<<"  .\\pypp virtual <n> list            List installed modules\n";
        return 1;
    }

    // ── run a script ────────────────────────────────────────────────
    if(argc<2){
        std::cout<<"PyPP Interpreter v2.0\n\n";
        std::cout<<"Usage:\n";
        std::cout<<"  .\\pypp <file.pypp>                       Run a script\n";
        std::cout<<"  .\\pypp virtual <n>                    Create a virtual environment\n";
        std::cout<<"  .\\pypp virtual <n> activate           Show activation instructions\n";
        std::cout<<"  .\\pypp virtual <n> install <module>   Install a module into venv\n";
        std::cout<<"  .\\pypp virtual <n> list               List installed modules\n";
        return 0;
    }

    std::ifstream f(argv[1]);
    if(!f){ std::cerr<<"[PyPP] Error: cannot open '"<<argv[1]<<"'.\n"; return 1; }
    std::ostringstream ss; ss<<f.rdbuf();

    const char* ev=std::getenv("PYPP_VENV");
    if(ev) g_venv_active=ev;

    try{
        auto tokens=lex(ss.str()); Parser parser; parser.toks=tokens;
        auto ast=parser.parse(); Interpreter interp; interp.run(ast);
    } catch(const std::exception& e){ std::cerr<<"[PyPP Error] "<<e.what()<<"\n"; return 1; }
    return 0;
}