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

// --- Search helper ---
void runSearch(json& bible, const string& keywordArg) {
    bool found = false;

    // Split into tokens
    istringstream iss(keywordArg);
    vector<string> tokens;
    for (string w; iss >> w;) tokens.push_back(w);

    // Try regex
    bool useRegex = false;
    regex pattern;
    try {
        pattern = regex(keywordArg, regex_constants::icase);
        useRegex = true;
    } catch (...) {
        useRegex = false;
    }

    for (auto& b : bible) {
        string book = b["book"];
        for (auto& ch : b["chapters"]) {
            int chapter = ch["chapter"];
            for (auto& v : ch["verses"]) {
                string text = v["text"];
                string lowerText = toLower(text);

                bool match = false;
                if (useRegex) {
                    match = regex_search(text, pattern);
                } else if (tokens.size() == 3 && tokens[1] == "&&") {
                    // AND logic
                    match = (lowerText.find(toLower(tokens[0])) != string::npos &&
                             lowerText.find(toLower(tokens[2])) != string::npos);
                } else if (tokens.size() == 3 && tokens[1] == "||") {
                    // OR logic
                    match = (lowerText.find(toLower(tokens[0])) != string::npos ||
                             lowerText.find(toLower(tokens[2])) != string::npos);
                } else {
                    // Default: all words must appear
                    match = true;
                    for (auto& k : tokens) {
                        if (lowerText.find(toLower(k)) == string::npos) {
                            match = false;
                            break;
                        }
                    }
                }

                if (match) {
                    string highlighted = text;
                    for (auto& k : tokens) {
                        if (k == "&&" || k == "||") continue; // skip operators
                        size_t pos = 0;
                        while ((pos = toLower(highlighted).find(toLower(k), pos)) != string::npos) {
                            highlighted.replace(pos, k.length(),
                                "\033[1;31m" + highlighted.substr(pos, k.length()) + "\033[0m");
                            pos += k.length() + 9;
                        }
                    }
                    cout << "\033[1;34m" << book << " " << "\033[32m" << chapter << ":" << v["verse"]
                    << "\033[0m" << " → " << highlighted << "\n";
                    found = true;
                }
            }
        }
    }

    if (!found) {
        cout << "The word doesn't exist.\n";
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

// --- Book-specific search helper ---
void runSearchInBook(json& bible, const string& book, const string& keywordArg) {
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
        cout << "Book not found.\n";
        return;
    }

    // Step 3: suggest if fuzzy
    if (toLower(bestBook) != toLower(book)) {
        cout << "Did you mean '" << bestBook << "'?\n";
    }

    // Step 4: search only inside bestBook
    bool found = false;
    istringstream iss(keywordArg);
    vector<string> keywords;
    for (string w; iss >> w;) keywords.push_back(w);

    for (auto& b : bible) {
        if (b["book"] == bestBook) {
            for (auto& ch : b["chapters"]) {
                int chapter = ch["chapter"];
                for (auto& v : ch["verses"]) {
                    string text = v["text"];
                    string lowerText = toLower(text);

                    bool match = true;
                    for (auto& k : keywords) {
                        if (lowerText.find(toLower(k)) == string::npos) {
                            match = false; break;
                        }
                    }

                    if (match) {
                        cout << "\033[1;34m" << bestBook << " " << "\033[32m" << chapter << ":" << v["verse"]
                        << "\033[0m" << " → " << text << "\n";
                        found = true;
                    }
                }
            }
        }
    }

    if (!found) {
        cout << "No matches found in '" << bestBook << "'.\n";
    }
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

// Evaluate postfix expression on verse text
bool evalPostfix(const vector<string>& postfix, const string& text) {
    stack<bool> st;
    string lowerText = toLower(text);

    for (auto& token : postfix) {
        if (token == "&&" || token == "||") {
            if (st.size() < 2) return false; // malformed
            bool b = st.top(); st.pop();
            bool a = st.top(); st.pop();
            st.push(token == "&&" ? (a && b) : (a || b));
        } else if (token == "!") {
            if (st.empty()) return false; // malformed
            bool a = st.top(); st.pop();
            st.push(!a);
        } else {
            st.push(lowerText.find(toLower(token)) != string::npos);
        }
    }
    return !st.empty() && st.top();
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

        // Tokenize input here
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
            << "  quit / exit              → Quit REPL\n";
            continue;
        }

        // Search branch
        if (tokens[0] == "search" && tokens.size() >= 2) {
            string query = line.substr(7); // everything after "search "
            try {
                vector<string> toks = tokenize(query);
                vector<string> postfix = toPostfix(toks);
                bool anyFound = false;
                for (auto& b : bible) {
                    for (auto& ch : b["chapters"]) {
                        for (auto& v : ch["verses"]) {
                            if (evalPostfix(postfix, v["text"])) {
                            if (evalPostfix(postfix, v["text"])) {
                                string highlighted = v["text"];
                                for (auto& tok : toks) {
                                    if (tok == "&&" || tok == "||" || tok == "!" || tok == "(" || tok == ")") continue;
                                    size_t pos = 0;
                                    while ((pos = toLower(highlighted).find(toLower(tok), pos)) != string::npos) {
                                        highlighted.replace(pos, tok.length(),
                                        "\033[1;31m" + highlighted.substr(pos, tok.length()) + "\033[0m");
                                        pos += tok.length() + 9; // move past highlight
                                    }
                                }

                                cout << "\033[1;34m" << b["book"] << " " << "\033[32m" << ch["chapter"] << ":"
                                     << v["verse"] << "\033[0m" << " → " << highlighted << "\n";
                                     anyFound = true;
                                }
                            }
                        }
                    }
                }
                if (!anyFound) cout << "No matches found.\n";
            } catch (...) {
                runSearch(bible, query); // fallback
            }
            continue;
        }

        // Book-specific search (must come before chapter/verse parsing)
        else if (tokens.size() >= 3 && tokens[1] == "search") {
            // join everything after "search" into one keyword string
            string keywordArg;
            for (size_t i = 2; i < tokens.size(); i++) {
                if (i > 2) keywordArg += " ";
                keywordArg += tokens[i];
            }
            runSearchInBook(bible, tokens[0], keywordArg);
        }

        // Book + Chapter
        else if (tokens.size() == 2) {
            int chapter = safeStoi(tokens[1]);
            if (chapter == -1) continue;
            runChapter(bible, tokens[0], chapter);
        }

        // Book + Chapter + Verse or Range
        else if (tokens.size() == 3) {
            string book = tokens[0];
            int chapter = safeStoi(tokens[1]);
            if (chapter == -1) continue;
            string verseArg = tokens[2];
            runRange(bible, book, chapter, verseArg);
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

    // --- Flag-based search (simple, regex, or operator-based) ---
    if (args.count("--search")) {
        string query = args["--search"];

        // Try operator-based parsing first
        try {
            vector<string> tokens = tokenize(query);
            vector<string> postfix = toPostfix(tokens);

            bool anyFound = false;
            for (auto& b : bible) {
                for (auto& ch : b["chapters"]) {
                    for (auto& v : ch["verses"]) {
                        string text = v["text"];
                        if (evalPostfix(postfix, text)) {
                            cout << b["book"] << " "
                                 << ch["chapter"] << ":"
                                 << v["verse"] << " → "
                                 << text << "\n";
                            anyFound = true;
                        }
                    }
                }
            }
        if (!anyFound) {
            cout << "No matches found for expression.\n";
        }
    } catch (...) {
        // Fallback: use your existing simple search
        runSearch(bible, query);
    }

    return 0;
}




    // --- Flag-based book/chapter/range ---
    if (args.count("--book") && args.count("--chapter")) {
        string book = args["--book"];
        int chapter = safeStoi(args["--chapter"]);
        if (chapter == -1) return 1;   // stop if invalid

        if (args.count("--range")) {
            runRange(bible, book, chapter, args["--range"]);
        } else {
            runChapter(bible, book, chapter);
        }
        return 0;
    }

    // --- Book-specific search: nabreterm <Book> search <Keyword>
    if (argc >= 4 && string(argv[2]) == "search") {
        string book = argv[1];
        string keywordArg = argv[3];
        runSearchInBook(bible, book, keywordArg);
        return 0;
    }

    // --- Positional arguments fallback ---
    if (argc >= 3) {
        if (string(argv[1]) == "search" && argc == 3) {
            runSearch(bible, argv[2]);
        } else {
            string book = argv[1];
            int chapter = safeStoi(argv[2]);
            if (chapter == -1) return 1;   // stop if invalid
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
