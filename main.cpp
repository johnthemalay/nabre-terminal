/*Scripture texts, prefaces, introductions, footnotes and cross references used in this
work are taken from the New American Bible, revised edition © 2010, 1991, 1986, 1970
Confraternity of Christian Doctrine, Inc., Washington, DC All Rights Reserved.*/

#ifndef NABRETERM_DATADIR
#define NABRETERM_DATADIR "."
#endif

#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>
#include <map>
#include <vector>
#include <readline/readline.h>
#include <readline/history.h>


using namespace std;
using json = nlohmann::json;
bool evalPostfix(const vector<string>& postfix, const string& text);

//Clear Screen
void clearScreen() {
#ifdef _WIN32
    system("cls");   // Windows
#else
    std::cout << "\033[2J\033[H"; // Linux/macOS
#endif
}

// Utility: lowercase conversion
string toLower(const string& s) {
    string result = s;
    transform(result.begin(), result.end(), result.begin(),
              [](unsigned char c){ return tolower(c); });
    return result;
}

// --- Safe stoi wrapper ---
int safeStoi(const string& s) {
    try {
        return stoi(s);
    } catch (...) {
        cerr << "Invalid number: " << s << "\n";
        return -1; // sentinel value
    }
}

int levenshtein(const string& a, const string& b) {
    int n = a.size(), m = b.size();
    vector<vector<int>> dp(n+1, vector<int>(m+1));

    for (int i = 0; i <= n; i++) dp[i][0] = i;
    for (int j = 0; j <= m; j++) dp[0][j] = j;

    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= m; j++) {
            int cost = (tolower(a[i-1]) == tolower(b[j-1])) ? 0 : 1;
            dp[i][j] = min({ dp[i-1][j] + 1,     // deletion
                             dp[i][j-1] + 1,     // insertion
                             dp[i-1][j-1] + cost }); // substitution
        }
    }
    return dp[n][m];
}

// --- Tokenizer: split into words, operators, parentheses ---
vector<string> tokenize(const string& query) {
    vector<string> tokens;
    string token;

    for (size_t i = 0; i < query.size(); i++) {
        char c = query[i];

        if (isspace(c)) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        }
        else if (c == '(' || c == ')') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            tokens.push_back(string(1, c));
        }
        else if (c == '&' && i+1 < query.size() && query[i+1] == '&') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            tokens.push_back("&&");
            i++; // skip second &
        }
        else if (c == '|' && i+1 < query.size() && query[i+1] == '|') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            tokens.push_back("||");
            i++; // skip second |
        }
        else if (c == '!') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            tokens.push_back("!");
        }
        else {
            token.push_back(c);
        }
    }

    if (!token.empty()) tokens.push_back(token);
    return tokens;
}

// --- Operator precedence helper ---
int precedence(const string& op) {
    if (op == "&&") return 2;
    if (op == "||") return 1;
    if (op == "!")  return 3;
    return 0;
}

// --- Shunting-yard: infix → postfix ---
vector<string> toPostfix(const vector<string>& tokens) {
    vector<string> output;
    stack<string> ops;
    for (auto& tok : tokens) {
        if (tok == "&&" || tok == "||" || tok == "!") {
            while (!ops.empty() && precedence(ops.top()) >= precedence(tok)) {
                output.push_back(ops.top());
                ops.pop();
            }
            ops.push(tok);
        } else if (tok == "(") {
            ops.push(tok);
        } else if (tok == ")") {
            while (!ops.empty() && ops.top() != "(") {
                output.push_back(ops.top());
                ops.pop();
            }
            if (!ops.empty()) ops.pop(); // discard "("
        } else {
            output.push_back(tok); // keyword
        }
    }
    while (!ops.empty()) {
        output.push_back(ops.top());
        ops.pop();
    }
    return output;
}


// Simple argument parser for flags (--book, --chapter, etc.)
map<string,string> parseArgs(int argc, char* argv[]) {
    map<string,string> args;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg.rfind("--", 0) == 0 && i+1 < argc) {
            args[arg] = argv[i+1];
            i++;
        }
    }
    return args;
}

bool isNewTestament(const string& book) {
    static vector<string> ntBooks = {
        "Matthew","Mark","Luke","John","Acts","Romans",
        "1Corinthians","2Corinthians","Galatians","Ephesians","Philippians",
        "Colossians","1Thessalonians","2Thessalonians","1Timothy","2Timothy",
        "Titus","Philemon","Hebrews","James","1Peter","2Peter",
        "1John","2John","3John","Jude","Revelation"
    };
    return find(ntBooks.begin(), ntBooks.end(), book) != ntBooks.end();
}

bool isDeuterocanonical(const string& book) {
    static vector<string> deutBooks = {
        "Tobit","Judith","Wisdom","Sirach","Baruch","1Maccabees","2Maccabees"
    };
    return find(deutBooks.begin(), deutBooks.end(), book) != deutBooks.end();
}


// Evaluate postfix expression on verse text
bool evalPostfix(const vector<string>& postfix, const string& text) {
    stack<bool> st;
    string lowerText = toLower(text);

    for (auto& token : postfix) {
        if (token == "&&" || token == "||") {
            if (st.size() < 2) return false;
            bool b = st.top(); st.pop();
            bool a = st.top(); st.pop();
            st.push(token == "&&" ? (a && b) : (a || b));
        } else if (token == "!") {
            if (st.empty()) return false;
            bool a = st.top(); st.pop();
            st.push(!a);
        } else {
            // Fuzzy match for keywords
            bool wordMatch = false;
            regex wordPattern("\\b" + toLower(token) + "\\w*\\b", regex_constants::icase);
            if (regex_search(lowerText, wordPattern)) {
                wordMatch = true;
            } else {
                istringstream iss(lowerText);
                string word;
                while (iss >> word) {
                    if (levenshtein(word, toLower(token)) <= 2) {
                        wordMatch = true;
                        break;
                    }
                }
            }
            st.push(wordMatch);
        }
    }
    return !st.empty() && st.top();
}

// -- Unified Search Engine --
void searchEngine(json& bible, const string& query, const string& scopeBook = "") {
    vector<string> toks = tokenize(query);
    bool found = false;

    // Convert to postfix for operator search
    vector<string> postfix = toPostfix(toks);

    for (auto& b : bible) {
        if (!scopeBook.empty() && toLower(b["book"]) != toLower(scopeBook)) continue;

        for (auto& ch : b["chapters"]) {
            for (auto& v : ch["verses"]) {
                string text = v["text"];
                string lowerText = toLower(text);
                bool match = false;

                // Evaluate postfix if operators are present
                if (!postfix.empty()) {
                    match = evalPostfix(postfix, text);
                } else {
                    // fallback: all tokens must appear (with fuzzy matching)
                    match = true;
                    for (auto& k : toks) {
                        if (k == "&&" || k == "||" || k == "!" || k == "(" || k == ")") continue;

                        bool wordMatch = false;
                        regex wordPattern("\\b" + toLower(k) + "\\w*\\b", regex_constants::icase);
                        if (regex_search(lowerText, wordPattern)) {
                            wordMatch = true;
                        } else {
                            // Fuzzy check against each word in the verse
                            istringstream iss(lowerText);
                            string word;
                            while (iss >> word) {
                                if (levenshtein(word, toLower(k)) <= 2) {
                                    wordMatch = true;
                                    break;
                                }
                            }
                        }

                        if (!wordMatch) { match = false; break; }
                    }
                }

                if (match) {
                    string highlighted = text;

                    // Only highlight if NOT operator is not used
                    if (query.find("!") == string::npos) {
                        for (auto& k : toks) {
                            if (k == "&&" || k == "||" || k == "!" || k == "(" || k == ")") continue;
                            regex wordPattern("\\b" + toLower(k) + "\\w*\\b", regex_constants::icase);
                            sregex_iterator it(lowerText.begin(), lowerText.end(), wordPattern);
                            sregex_iterator end;
                            size_t offset = 0;
                            for (; it != end; ++it) {
                                size_t pos = it->position() + offset;
                                string matchStr = it->str();
                                highlighted.replace(pos, matchStr.length(),
                                                    "\033[1;31m" + matchStr + "\033[0m");
                                offset += 9; // account for escape codes
                            }
                        }
                    }

                    cout << "\033[1;34m" << b["book"] << " "
                    << "\033[32m" << ch["chapter"] << ":" << v["verse"]
                    << "\033[0m → " << highlighted << "\n";
                    found = true;
                }
            }
        }
    }

    if (!found) {
        cerr << "Error: No matches found.\n";
    }
}

// --- Whole chapter helper ---
void runChapter(json& bible, const string& book, int chapter) {
    // Step 1: find closest book
    string bestBook;
    int bestDist = 999;
    for (auto& b : bible) {
        int dist = levenshtein(toLower(b["book"]), toLower(book));
        if (dist < bestDist) {
            bestDist = dist;
            bestBook = b["book"];
        }
    }

    // Step 2: check threshold
    if (bestDist > 2) {
        cerr << "Book not found.\n";
        return;
    }

    // Step 3: suggest if fuzzy
    if (toLower(bestBook) != toLower(book)) {
        cerr << "Did you mean '" << bestBook << "'?\n";
    }

    // Step 4: search only in bestBook
    bool found = false;
    for (auto& b : bible) {
        if (b["book"] == bestBook) {
            for (auto& ch : b["chapters"]) {
                if (ch["chapter"] == chapter) {
                    for (auto& v : ch["verses"]) {
                        cout << "\033[1;34m" << bestBook << "\033[0m" << "\033[32m" << chapter << ":" << v["verse"]
                             << "\033[0m" << " → " << v["text"] << "\n";
                        found = true;
                    }
                }
            }
        }
    }
    if (!found) cerr << "Chapter not found.\n";
}

void runRange(json& bible, const string& book, int chapter, const string& verseArg) {
    // Step 1: find closest book
    string bestBook;
    int bestDist = 999;
    for (auto& b : bible) {
        int dist = levenshtein(toLower(b["book"]), toLower(book));
        if (dist < bestDist) {
            bestDist = dist;
            bestBook = b["book"];
        }
    }

    // Step 2: check threshold
    if (bestDist > 2) {
        cerr << "Book not found.\n";
        return;
    }

    // Step 3: suggest if fuzzy
    if (toLower(bestBook) != toLower(book)) {
        cerr << "Did you mean '" << bestBook << "'?\n";
    }

    // Step 4: parse verse range
    int startVerse, endVerse;
    if (verseArg.find('-') != string::npos) {
        stringstream ss(verseArg);
        string start, end;
        getline(ss, start, '-');
        getline(ss, end, '-');
        startVerse = safeStoi(start);
        endVerse   = safeStoi(end);
    if (startVerse == -1 || endVerse == -1) return; // invalid input

    } else {
        startVerse = endVerse = safeStoi(verseArg);
        if (startVerse == -1) return; // invalid input

    }

    // Step 5: search only inside bestBook
    bool found = false;
    for (auto& b : bible) {
        if (b["book"] == bestBook) {
            for (auto& ch : b["chapters"]) {
                if (ch["chapter"] == chapter) {
                    for (auto& v : ch["verses"]) {
                        int verseNum = v["verse"];

                        if (verseNum >= startVerse && verseNum <= endVerse) {
                            cout << "\033[1;34m" << bestBook << " " << "\033[32m" << chapter << ":" << verseNum
                                 << "\033[0m" << " → " << v["text"] << "\n";
                            found = true;
                        }
                    }
                }
            }
        }
    }

    if (!found) cerr << "Verse(s) not found.\n";
}


// --- List all books from JSON ---
void runListBooksColumn(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Could not open books JSON file.\n";
        return;
    }
    json books;
    file >> books;

    int cols = 4; // number of columns
    int width = 20; // column width for alignment
    int count = books.size();

    cout << "NABRE BOOKS\n";
    cout << string(cols * width, '-') << "\n"; // underline

    for (int i = 0; i < count; i++) {
        cout << left << setw(width) << books[i].get<string>();
        if ((i+1) % cols == 0) cout << "\n";
    }
    if (count % cols != 0) cout << "\n"; // final newline
}

void replLoop(json& bible) {
    string histFile = string(getenv("HOME")) + "/.nabreterm_history";

    // Load history at startup (safe if file missing)
    read_history(histFile.c_str());

    while (true) {
        char* input = readline("\033[1;37mNabreterm> \033[0m");
        if (!input) break; // Ctrl+D
        string line(input);
        free(input);

        if (line.empty()) continue;
        add_history(line.c_str());

        if (line == "quit" || line == "exit") break;
        if (line == "list") {
            runListBooksColumn("books.json");
            continue;
        }

        // Tokenize input
        istringstream iss(line);
        vector<string> tokens;
        for (string w; iss >> w;) tokens.push_back(w);

        if (tokens.empty()) continue;

        if (line == "help") {
            cout << "Commands:\n"
                 << "  Book Chapter Verse       → Show verse\n"
                 << "  Book Chapter Verse-Range → Show range\n"
                 << "  Book Chapter             → Show chapter\n"
                 << "  search <keyword>         → Search globally\n"
                 << "  <Book> search <keyword>  → Search within a book\n"
                 << "  search faith && hope     → Operator search (AND)\n"
                 << "  search faith || love     → Operator search (OR)\n"
                 << "  search !(sin)            → Operator search (NOT)\n"
                 << "  list                     → List all books\n"
                 << "  random                   → random Bible Verse\n"
                 << "  random [scope,e.g Psalms]→ random Bible Verse but its in the picked scope\n"
                 << "  random [N] [scope]       → your custom number verses in random Bible Verse\n"
                 << "                           → but its in the picked scope\n"
                 << "  quit / exit              → Quit REPL\n";
            continue;
        }

        // Global search
        if (tokens[0] == "search" && tokens.size() >= 2) {
            string query = line.substr(7); // everything after "search "
            searchEngine(bible, query);
            continue;
        }

        // Book-specific search
        else if (tokens.size() >= 3 && tokens[1] == "search") {
            string keywordArg;
            for (size_t i = 2; i < tokens.size(); i++) {
                if (i > 2) keywordArg += " ";
                keywordArg += tokens[i];
            }
            searchEngine(bible, keywordArg, tokens[0]);
            continue;
        }

        //Clear Screen
        if (line == "clear") {
            clearScreen();
            continue;
        }

        // Random verse(s) with optional count and scope
        if (tokens[0] == "random") {
            int verseCount = 1; // default
            string scopeArg;

            // detect if second token is a number
            if (tokens.size() >= 2) {
                if (isdigit(tokens[1][0])) {
                    verseCount = safeStoi(tokens[1]);
                    if (tokens.size() >= 3) scopeArg = toLower(tokens[2]);
                } else {
                    scopeArg = toLower(tokens[1]);
                }
            }

            if (verseCount <= 0) {
                cerr << "Invalid verse count.\n";
                continue;
            }

            vector<json> scope;
            if (!scopeArg.empty()) {
                if (scopeArg == "ot") {
                    for (auto& b : bible) if (!isNewTestament(b["book"])) scope.push_back(b);
                } else if (scopeArg == "nt") {
                    for (auto& b : bible) if (isNewTestament(b["book"])) scope.push_back(b);
                } else if (scopeArg == "deut") {
                    for (auto& b : bible) if (isDeuterocanonical(b["book"])) scope.push_back(b);
                } else {
                    // fuzzy match for specific book
                    string bestBook;
                    int bestDist = 999;
                    for (auto& b : bible) {
                        int dist = levenshtein(toLower(b["book"]), scopeArg);
                        if (dist < bestDist) { bestDist = dist; bestBook = b["book"]; }
                    }
                    if (bestDist <= 2) {
                        for (auto& b : bible) if (b["book"] == bestBook) { scope.push_back(b); break; }
                    }
                }
            } else {
                scope = bible; // default whole Bible
            }

            if (scope.empty()) {
                cerr << "Scope not found.\n";
                continue;
            }

            // pick random book + chapter
            int bIndex = rand() % scope.size();
            auto& b = scope[bIndex];
            int cIndex = rand() % b["chapters"].size();
            auto& ch = b["chapters"][cIndex];

            if (ch["verses"].size() < static_cast<size_t>(verseCount)) {
                cerr << "Not enough verses in this chapter.\n";
                continue;
            }

            // pick distinct verses
            vector<int> chosen;
            while (chosen.size() < static_cast<size_t>(verseCount)) {
                int vIndex = rand() % ch["verses"].size();
                if (find(chosen.begin(), chosen.end(), vIndex) == chosen.end()) {
                    chosen.push_back(vIndex);
                }
            }

            for (int idx : chosen) {
                auto& v = ch["verses"][idx];
                cout << "\033[1;34m" << b["book"] << " "
                << "\033[32m" << ch["chapter"] << ":" << v["verse"]
                << "\033[0m → " << v["text"] << "\n";
            }
            continue;
        }


        // Book + Chapter + Verse or Range
        else if (tokens.size() == 3) {
            int chapter = safeStoi(tokens[1]);
            if (chapter == -1) continue;
            runRange(bible, tokens[0], chapter, tokens[2]);
            continue;
        }

        else {
            cout << "Unknown command. Try again.\n";
        }
    }

    // Save history on exit (creates file if missing)
    write_history(histFile.c_str());
}

int main(int argc, char* argv[]) {
    auto args = parseArgs(argc, argv);

    ifstream file("nabre.json");
    if (!file.is_open()) {
    file.open(std::string(NABRETERM_DATADIR) + "/nabre.json");
    }
    if (!file.is_open()) {
    std::cerr << "Could not open NABRE JSON file.\n";
    return 1;
    }

    json bible;
    file >> bible;

    // --- Flag-based search ---
    if (args.count("--search")) {
        string query = args["--search"];
        searchEngine(bible, query);
        return 0;
    }

    // --- Book-specific search: nabreterm <Book> search <Keyword>
    if (argc >= 4 && string(argv[2]) == "search") {
        string book = argv[1];
        string keywordArg;
        for (int i = 3; i < argc; i++) {
            if (i > 3) keywordArg += " ";
            keywordArg += argv[i];
        }
        searchEngine(bible, keywordArg, book);
        return 0;
    }

    // --- Positional arguments fallback ---
    if (argc >= 3) {
        if (string(argv[1]) == "search" && argc == 3) {
            searchEngine(bible, argv[2]);
        } else {
            string book = argv[1];
            int chapter = safeStoi(argv[2]);
            if (chapter == -1) return 1;
            if (argc == 3) {
                runChapter(bible, book, chapter);
            } else {
                runRange(bible, book, chapter, argv[3]);
            }
        }
    }

    // --- List all books ---
    if (args.count("--list") || (argc == 2 && string(argv[1]) == "list")) {
        runListBooksColumn("books.json");
        return 0;
    }

// --- Interactive REPL mode ---
    if (argc == 1) {
        replLoop(bible);
    }

    return 0;
}
