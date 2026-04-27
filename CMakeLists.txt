cmake_minimum_required(VERSION 3.15)
project(cpp-search-engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../)


add_executable(main
   DirectoryScanner.cc
   KeywordProcessor.cc
   main.cc

)

target_compile_options(main PRIVATE
   # -fno-rtti
   -frtti
    -g
)

target_include_directories(main PRIVATE
    /usr/local/include
)

target_link_directories(main PRIVATE
    /usr/local/lib
)


target_link_libraries(main PRIVATE
    spdlog
    fmt   
    
)

