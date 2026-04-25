# 🚀 PyPP Language `v1 Beta`

**PyPP** is a high-performance, lightweight scripting language built with C++17. It blends the clean, readable syntax of modern scripting languages with the structural benefits of explicit typing and C++ efficiency.

---

## 🛠 Features

* **Explicit Typing:** Native support for `int`, `float`, `str`, `array`, and `obj`.
* **Structured Objects:** Key-value mapping using the `=>` syntax.
* **Advanced Control Flow:** Full `if / ifelse / else` chains, `while`, and `do-while` loops.
* **Modern Iteration:** Range-based loops, array iteration, and object key traversal.
* **Memory Debugging:** Built-in `var_dumper` for structured inspection of data types.

---

## 📥 Getting Started

### Prerequisites
* **GCC/G++** (supporting C++17)
* **Make**

### Build & Run
To compile the interpreter and run your first script, use the following:

```bash
# Compile the project
make

# Run a PyPP script
./pypp file.pypp