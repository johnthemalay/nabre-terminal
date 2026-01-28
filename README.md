Here’s a **full documentation draft** you can include in your repo as `README.md` (or a separate `DOCUMENTATION.md`). It covers everything from **CMake build** to **install** and **uninstall**, in a professional open‑source style:

---

# Nabreterm

Nabreterm is a lightweight terminal application for exploring the NABRE Bible in JSON format.  
It supports both **CLI mode** (one‑shot commands) and **REPL mode** (interactive session).

---

## 🚀 Build with CMake

### Requirements
- GNU GCC / G++ (tested with GCC 11+)
- GNU Readline library (`libreadline-dev` on Debian/Ubuntu)
- [nlohmann/json](https://github.com/nlohmann/json) (header‑only library)

### Steps
```bash
# 1. Create build directory
mkdir build
cd build

# 2. Configure project
cmake ..

# 3. Compile
make

# 4. Run
./Nabreterm
```

After building, the JSON files (`nabre.json`, `books.json`) will be copied into the build directory alongside the binary.

---

## 📦 Install

To install Nabreterm system‑wide:

```bash
sudo make install
```

This will place:
- Binary → `/usr/local/bin/Nabreterm`
- JSON files → `/usr/local/share/nabreterm`

Now you can run it anywhere:
```bash
Nabreterm
```

---

## 🗑️ Uninstall

If you added the uninstall target in `CMakeLists.txt`, you can remove Nabreterm cleanly:

```bash
cd build
sudo make uninstall
```

If uninstall target isn’t available, remove manually:
```bash
sudo rm /usr/local/bin/Nabreterm
sudo rm -rf /usr/local/share/nabreterm
```

---

## 📖 Usage

### REPL Mode
Start with:
```bash
./Nabreterm
```

Commands:
- `John 3 16` → show verse  
- `John 3 16-18` → show range  
- `John 3` → show chapter  
- `search love` → global search  
- `Matthew search kingdom` → search within a book  
- `list` → list all books  
- `help` → show command list  
- `quit` / `exit` → leave REPL  

### CLI Mode
Run directly with arguments:
- `./Nabreterm --search love` → global search  
- `./Nabreterm John search light` → search within a book  
- `./Nabreterm John 3` → show chapter  
- `./Nabreterm John 3 16` → show verse  
- `./Nabreterm John 3 16-18` → show range  
- `./Nabreterm --list` → list all books  

---

## ✨ Features
- **Fuzzy matching** for book names (handles typos like `Matthw` → `Matthew`).  
- **Regex search** supported in `search`.  
- **Persistent history** stored in `~/.nabreterm_history`, recalled with ↑ / ↓ arrows.  
- **Color highlighting** for book names and search matches.  

---

## 📂 Project Structure
- `main.cpp` → core application  
- `nabre.json` → NABRE Bible data  
- `books.json` → list of book names  
- `CMakeLists.txt` → build configuration  
- `cmake_uninstall.cmake.in` → uninstall script (optional)  

---

## 📜 License
MIT License. See `LICENSE` file for details.

---
