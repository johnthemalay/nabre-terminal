cmake_minimum_required(VERSION 3.10)
project(Nabreterm)

set(CMAKE_CXX_STANDARD 17)

add_executable(nabreterm main.cpp)

# Link directly
target_link_libraries(nabreterm PRIVATE readline history)

# Copy JSON files into build dir
set(JSON_FILES nabre.json books.json)
foreach(json_file ${JSON_FILES})
    configure_file(${json_file} ${json_file} COPYONLY)
endforeach()

# Define install data directory for runtime
add_definitions(-DNABRETERM_DATADIR="${CMAKE_INSTALL_PREFIX}/share/nabreterm")

install(TARGETS nabreterm DESTINATION bin)
install(FILES ${JSON_FILES} DESTINATION share/nabreterm)
