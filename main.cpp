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
        startVerse = stoi(start);
        endVerse = stoi(end);
    } else {
        startVerse = endVerse = stoi(verseArg);
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
                        cout << bestBook << " " << chapter << ":" << v["verse"]
                             << " → " << text << "\n";
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
         << "  Book Chapter Verse     → Show verse\n"
         << "  Book Chapter Verse-Range → Show range\n"
         << "  Book Chapter           → Show chapter\n"
         << "  search <keyword>       → Search globally\n"
         << "  <Book> search <keyword> → Search within a book\n"
         << "  list                   → List all books\n"
         << "  quit / exit            → Quit REPL\n";
    continue;
    }

    // Now you can safely use tokens
    if (tokens[0] == "search" && tokens.size() >= 2) {
        runSearch(bible, tokens[1]);
    } else if (tokens.size() == 2) {
        runChapter(bible, tokens[0], stoi(tokens[1]));
    } else if (tokens.size() == 3) {
        string book = tokens[0];
        int chapter = stoi(tokens[1]);
        string verseArg = tokens[2];

        // Debug check
        //cout << "DEBUG: book=" << book << " chapter=" << chapter << " verseArg=" << verseArg << "\n";

        runRange(bible, book, chapter, verseArg);
    } else if (tokens.size() >= 3 && tokens[1] == "search") {
        runSearchInBook(bible, tokens[0], tokens[2]);
    } else {
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
        runSearch(bible, args["--search"]);
        return 0;
    }

    // --- Flag-based book/chapter/range ---
    if (args.count("--book") && args.count("--chapter")) {
        string book = args["--book"];
        int chapter = stoi(args["--chapter"]);

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
            int chapter = stoi(argv[2]);   // safe now, because "search" case was handled above
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
