# 🚀 PyPP Language `v1 Beta`

**PyPP** is a high-performance, lightweight scripting language built with C++17. It blends the clean, readable syntax of modern scripting languages with the structural benefits of explicit typing and C++ efficiency.

---

## 🛠 Features

* **Explicit Typing:** Native support for `int`, `float`, `str`, `array`, `bool`, and `obj`.
* **Structured Objects:** Key-value mapping using the `=>` syntax.
* **Advanced Control Flow:** Full `if / ifelse / else` chains, `while`, and `do-while` loops.
* **Modern Iteration:** Range-based loops, array iteration, and object key traversal.
* **Memory Debugging:** Built-in `var_dumper` for structured inspection of data types.
* **Standard Output:** Simple `say` command for terminal printing and data inspection.
* **Loops range:** Simple range method for For `for i in numberLooper(1, 3){}` 

---

## 📥 Getting Started

### Prerequisites
* **GCC/G++** (supporting C++17)
* **Make**

### Build & Run
To compile the interpreter and run your first script:

```bash
# Compile the project
make

# Run a PyPP script
./pypp file.pypp
```

# 🚀 PyPP Language `v1.1 Beta`

---

## 🛠 Features

* **Array methoods:** 
### `push(v)`, `pop()`                          Add/remove from end.
### `shift()`, `unshift(v)`                     Remove/add at front.
### `indexOf(v)`, `lastIndexOf(v)`              Search positions.
### `includes(v)`,                              Returns bool.
### `slice(start, end)`                         Non-destructive copy.
### `splice(start, count, …inserts)`            In-place edit, returns removed.
### `sort()`, `reverse()`                       In-place sort/flip.
### `join(sep)`, `concat(arr)`                  Combine.
### `flat(depth)`, `fill(v, s, e)`              Flatten / fill range.
### `at(i)`, ` first()`, `last()`, `isEmpty()`  Accessors.
### `length()`, `count(v)`, `clear()`           Info.
### `map`/`filter`/`forEach`/`find`/`findIndex` Higher-order.
### `every`/`some`/`reduce`                     Aggregation.

* **With open:**
### `with "file.txt", "r", encoding="utf-8" as f { f.read() }`              Reading files.
### `with "file.txt", "w", encoding="utf-8" as f { f.writeLine("hello") }`  Writing files. 
### `with "file.txt", "a", encoding="utf-8" as f { f.writeLine("append") }` Append into files.
### `with "file.txt", "w+",encoding="utf-8" as f { f.write("rw") }`         Read and Writing into files.

* **Function:**
### `function x() {}`          Writing function.
### `x()`                      calling it.
### `function x() {return hi}` return element.

* **Simple `say` command Update:** 
### `str x = "hi"`
### `say r`Salam: {x}`   \`

* **Virtual invermont:** 
### `./pypp virtual <name>  `         Create environment
### `./pypp virtual <name> activate`  Show activation help
### `./pypp virtual <name> install `  Install a module
### `./pypp virtual <name> list `     List installed modules

* **Import:**
### `import random`        import packages
### `import random as <n>` use import packages by your name


---

