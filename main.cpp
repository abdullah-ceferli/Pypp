#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include "lexer.h"
#include "parser.h"
#include "codegen.h"

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Returns a temp directory path that works on both Windows and Linux/Mac
std::string get_temp_dir() {
#ifdef _WIN32
    const char* tmp = std::getenv("TEMP");
    if (!tmp) tmp = std::getenv("TMP");
    if (!tmp) tmp = std::getenv("USERPROFILE");
    if (!tmp) tmp = "C:\\Windows\\Temp";
    return std::string(tmp);
#else
    return "/tmp";
#endif
}

std::string get_compiler_cmd() {
#ifdef _WIN32
    // Try g++ (MinGW/MSYS2), then clang++
    return "g++";
#else
    return "g++";
#endif
}

std::string path_join(const std::string& dir, const std::string& file) {
#ifdef _WIN32
    return dir + "\\" + file;
#else
    return dir + "/" + file;
#endif
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "PyPlus Compiler v0.2\n";
        std::cerr << "Usage: pyplus <file.pypp> [--emit-cpp]\n";
        return 1;
    }

    std::string input_file = argv[1];
    bool emit_cpp = (argc >= 3 && std::string(argv[2]) == "--emit-cpp");

    try {
        // 1. Read source
        std::string source = read_file(input_file);
        if (source.empty() || source.back() != '\n') source += '\n';

        // 2. Lex
        Lexer lexer(source);
        auto tokens = lexer.tokenize();

        // 3. Parse
        Parser parser(std::move(tokens));
        auto ast = parser.parse();

        // 4. Generate C++
        CodeGen codegen;
        std::string cpp_source = codegen.generate(ast);

        if (emit_cpp) {
            std::cout << cpp_source;
            return 0;
        }

        // 5. Write temp C++ file (cross-platform path)
        std::string tmp_dir  = get_temp_dir();
        std::string cpp_file = path_join(tmp_dir, "_pyplus_out.cpp");
#ifdef _WIN32
        std::string bin_file = path_join(tmp_dir, "_pyplus_bin.exe");
#else
        std::string bin_file = path_join(tmp_dir, "_pyplus_bin");
#endif

        {
            std::ofstream out(cpp_file);
            if (!out) throw std::runtime_error("Cannot write temp file: " + cpp_file);
            out << cpp_source;
        }

        // 6. Compile with g++
        std::string compiler = get_compiler_cmd();
        std::string compile_cmd = compiler + " -O2 -std=c++17 -o \"" + bin_file + "\" \"" + cpp_file + "\"";

        int ret = std::system(compile_cmd.c_str());
        if (ret != 0) {
            std::cerr << "Compilation failed.\n";
            std::cerr << "Temp file: " << cpp_file << "\n";
            return 1;
        }

        // 7. Run the compiled binary
#ifdef _WIN32
        std::string run_cmd = "\"" + bin_file + "\"";
#else
        std::string run_cmd = "\"" + bin_file + "\"";
#endif
        return std::system(run_cmd.c_str());

    } catch (const std::exception& e) {
        std::cerr << "\033[31mError:\033[0m " << e.what() << "\n";
        return 1;
    }
}
