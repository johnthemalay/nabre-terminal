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

#ifndef NABRETERM_DATADIR
#define NABRETERM_DATADIR "."
#endif

using namespace ftxui;
using json = nlohmann::json;

// --- Global state for async results ---
std::atomic<bool> has_new_results{false};
std::vector<std::string> new_results;

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

Component ResultsWindow(std::vector<std::string>& output_lines) {
  class Impl : public ComponentBase {
   private:
    float scroll_y = 0.0f;

   public:
    Impl(std::vector<std::string>& output_lines) {
      auto content = Renderer([&] {
        if (has_new_results) {
          output_lines = std::move(new_results);
          has_new_results = false;
        }
        std::vector<Element> lines;
        for (auto& line : output_lines) {
          lines.push_back(text(line));
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

      Add(Container::Vertical({
        Container::Horizontal({ scrollable_content, scrollbar_y }) | flex
      }));
    }

    // <-- this is at class scope, not inside Impl()
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

  return Make<Impl>(output_lines);
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

  auto results_child = ResultsWindow(output_lines);
  auto search_child = SearchWindow(bible, screen, input_query);

  auto search_window = Renderer(search_child, [&] {
    return window(text("Search Controls"), search_child->Render());
  });

  auto results_window = Renderer(results_child, [&] {
    return window(text("Search Results"), results_child->Render());
  });

  auto window_container = Container::Vertical({ search_window, results_window });
  screen.Loop(window_container);

  return EXIT_SUCCESS;
}
