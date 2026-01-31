// nabretermui.cpp
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <random>
#include <thread>
#include <atomic>
#include <cstdlib>

#ifndef NABRETERM_DATADIR
#define NABRETERM_DATADIR "."
#endif

using namespace ftxui;
using json = nlohmann::json;

// --- Global state for async results ---
std::atomic<bool> has_new_results{false};
std::vector<std::string> new_results;


//Clipboard
static void copyToClipboard(const std::string& text) {
  FILE* pipe = nullptr;

#ifdef _WIN32
  pipe = _popen("clip", "w");
  if (pipe) {
    fwrite(text.c_str(), sizeof(char), text.size(), pipe);
    _pclose(pipe);
    return;
  }
#else
  // macOS
  pipe = popen("pbcopy", "w");
  if (pipe) {
    fwrite(text.c_str(), sizeof(char), text.size(), pipe);
    pclose(pipe);
    return;
  }

  // Linux X11: xclip
  pipe = popen("xclip -selection clipboard", "w");
  if (pipe) {
    fwrite(text.c_str(), sizeof(char), text.size(), pipe);
    pclose(pipe);
    return;
  }

  // Linux X11: xsel
  pipe = popen("xsel --clipboard --input", "w");
  if (pipe) {
    fwrite(text.c_str(), sizeof(char), text.size(), pipe);
    pclose(pipe);
    return;
  }

  // Wayland: wl-clipboard
  pipe = popen("wl-copy", "w");
  if (pipe) {
    fwrite(text.c_str(), sizeof(char), text.size(), pipe);
    pclose(pipe);
    return;
  }
#endif

  // If none worked, show feedback in results
  new_results = { "Clipboard tool not found. Install xclip, xsel, or wl-clipboard." };
  has_new_results = true;
}



// --- Utility functions ---
static std::string toLower(const std::string& s) {
  std::string result = s;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c){ return std::tolower(c); });
  return result;
}

static std::vector<std::string> searchEngine(json& bible, const std::string& query) {
  std::vector<std::string> results;
  std::string q = toLower(query);

  for (auto& b : bible) {
    for (auto& ch : b["chapters"]) {
      for (auto& v : ch["verses"]) {
        std::string text = v["text"];
        if (!q.empty() && toLower(text).find(q) != std::string::npos) {
          std::ostringstream oss;
          oss << b["book"] << " " << ch["chapter"] << ":" << v["verse"]
              << " → " << text;
          results.push_back(oss.str());
        }
      }
    }
  }
  if (results.empty()) results.push_back("No matches found.");
  return results;
}

static std::string randomVerse(json& bible) {
  static std::mt19937 rng(std::random_device{}());
  int bIndex = rng() % bible.size();
  auto& b = bible[bIndex];
  int cIndex = rng() % b["chapters"].size();
  auto& ch = b["chapters"][cIndex];
  int vIndex = rng() % ch["verses"].size();
  auto& v = ch["verses"][vIndex];
  std::ostringstream oss;
  oss << b["book"] << " " << ch["chapter"] << ":" << v["verse"]
      << " → " << v["text"];
  return oss.str();
}

static Element highlightText(const std::string& line, const std::string& query) {
  if (query.empty()) return text(line);

  std::string lower_line = toLower(line);
  std::string lower_query = toLower(query);

  size_t pos = lower_line.find(lower_query);
  if (pos == std::string::npos) return text(line);

  return hbox({
    text(line.substr(0, pos)),
    text(line.substr(pos, query.size())) | bold | color(Color::Green),
    text(line.substr(pos + query.size()))
  });
}


// --- Search Window ---
Component SearchWindow(json& bible, ScreenInteractive& screen,
                       std::string& input_query) {
  class Impl : public ComponentBase {
  public:
    Impl(json& bible, ScreenInteractive& screen,
         std::string& input_query) {
      auto input = Input(&input_query, "Type search keyword...");

      auto btn_search = Button("Search", [&] {
        std::string query_copy = input_query;
        json* bible_ptr = &bible;
        ScreenInteractive* screen_ptr = &screen;

        std::thread([bible_ptr, query_copy, screen_ptr]() {
          new_results = searchEngine(*bible_ptr, query_copy);
          has_new_results = true;
          screen_ptr->PostEvent(Event::Custom); // signal UI
        }).detach();
      });

      auto btn_random = Button("Random Verse", [&] {
        json* bible_ptr = &bible;
        ScreenInteractive* screen_ptr = &screen;

        std::thread([bible_ptr, screen_ptr]() {
          new_results = { randomVerse(*bible_ptr) };
          has_new_results = true;
          screen_ptr->PostEvent(Event::Custom);
        }).detach();
      });

      auto btn_quit = Button("Quit", screen.ExitLoopClosure());

      Add(Container::Vertical({
        input,
        Container::Horizontal({ btn_search, btn_random, btn_quit })
      }));
    }
  };
  return Make<Impl>(bible, screen, input_query);
}

Component ResultsWindow(std::vector<std::string>& output_lines, std::string& input_query) {
  class Impl : public ComponentBase {
    float scroll_y = 0.0f;
    std::vector<std::string>& output_lines;
    std::string& input_query;

   public:
    Impl(std::vector<std::string>& output_lines, std::string& input_query)
        : output_lines(output_lines), input_query(input_query) {
      auto content = Renderer([&] {
        if (has_new_results) {
          output_lines = std::move(new_results);
          has_new_results = false;
        }
        std::vector<Element> lines;
        for (auto& line : output_lines) {
          lines.push_back(highlightText(line, input_query));
        }
        return vbox(lines);
      });

      auto scrollable_content = Renderer(content, [&, content] {
        return content->Render() | focusPositionRelative(0.0f, scroll_y) | frame | flex;
      });

      SliderOption<float> option_y;
      option_y.value = &scroll_y;
      option_y.min = 0.f;
      option_y.max = 1.f;
      option_y.increment = 0.05f;
      option_y.direction = Direction::Down;
      option_y.color_active = Color::Yellow;
      option_y.color_inactive = Color::YellowLight;
      auto scrollbar_y = Slider(option_y);

      auto btn_copy = Button("Copy First Result", [&] {
        if (!output_lines.empty()) {
          copyToClipboard(output_lines[0]);
          // feedback message
          new_results = { "Copied to clipboard!" };
          has_new_results = true;
        }
      });

Add(Container::Vertical({
  Container::Horizontal({ scrollable_content, scrollbar_y }) | flex,
  btn_copy
}));

    }

    bool OnEvent(Event event) override {
      if (event == Event::ArrowUp) {
        scroll_y = std::max(0.0f, scroll_y - 0.05f);
        return true;
      }
      if (event == Event::ArrowDown) {
        scroll_y = std::min(1.0f, scroll_y + 0.05f);
        return true;
      }
      return ComponentBase::OnEvent(event);
    }
  };

  return Make<Impl>(output_lines, input_query);
}


// --- Main ---
int main(int argc, char* argv[]) {
  std::ifstream file("nabre.json");
  if (!file.is_open()) file.open(std::string(NABRETERM_DATADIR) + "/nabre.json");
  if (!file.is_open()) {
    std::cerr << "Could not open NABRE JSON file.\n";
    return 1;
  }
  json bible; file >> bible;

  auto screen = ScreenInteractive::Fullscreen();

  std::string input_query;
  std::vector<std::string> output_lines = {"Welcome to NabretermUI"};

  auto results_child = ResultsWindow(output_lines, input_query);
  auto search_child = SearchWindow(bible, screen, input_query);

  auto search_window = Renderer(search_child, [&] {
  return window(text("Search Controls"), search_child->Render())
         | size(HEIGHT, GREATER_THAN, 3);   // never collapse below 3 lines
  });

  auto results_window = Renderer(results_child, [&] {
  return window(text("Search Results"), results_child->Render());
  });

  auto layout = Renderer(Container::Vertical({
  search_window,
  results_window
  }), [&] {
  return vbox({
    search_window->Render() | size(HEIGHT, LESS_THAN, screen.dimy() * 0.25), // cap at ~20–25%
    results_window->Render() | flex                                          // results take the rest
  });
});

screen.Loop(layout);


  return EXIT_SUCCESS;
}
