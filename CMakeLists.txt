cmake_minimum_required(VERSION 3.15)
project(cpp-search-engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../)


add_executable(server
   DirectoryScanner.cc
   KeywordProcessor.cc
   server.cc
   KeywordRecommender.cc
   LengthHeaderCodec.cc
   PageProcessor.cc
   WebSearcher.cc
   main.cc
)

target_compile_options(server PRIVATE
   # -fno-rtti
   -frtti
    -g
)

target_include_directories(server PRIVATE
    /usr/local/include
)

target_link_directories(server PRIVATE
    /usr/local/lib
)


target_link_libraries(server PRIVATE
    muduo_net
    muduo_base
    spdlog
    fmt
    tinyxml2
    pthread
)

