cmake_minimum_required(VERSION 3.10)
project(Nabreterm)

set(CMAKE_CXX_STANDARD 17)

# --- First executable: nabreterm (CLI REPL) ---
add_executable(nabreterm main.cpp)

# Link readline (still needed for history if you keep it)
target_link_libraries(nabreterm PRIVATE readline history)

# --- Add FTXUI ---
find_package(ftxui CONFIG REQUIRED)

target_link_libraries(nabreterm PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
)

# --- Second executable: nabretermui (UI version) ---
add_executable(nabretermui nabretermui.cpp)

target_link_libraries(nabretermui PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
)

# Copy JSON files into build dir
set(JSON_FILES nabre.json books.json)
foreach(json_file ${JSON_FILES})
    configure_file(${json_file} ${json_file} COPYONLY)
endforeach()

# Define install data directory for runtime
add_definitions(-DNABRETERM_DATADIR="${CMAKE_INSTALL_PREFIX}/share/nabreterm")

# Install both executables + data
install(TARGETS nabreterm nabretermui DESTINATION bin)
install(FILES ${JSON_FILES} DESTINATION share/nabreterm)
