cmake_minimum_required(VERSION 3.21)
project(TableComponentTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
set(sourceRoot "${CMAKE_CURRENT_SOURCE_DIR}/../../XTrader")
add_executable(TableComponentTests TableComponentTests.cpp
 "${sourceRoot}/basic/CDatable.cpp"
 "${sourceRoot}/database/IDataBase.cpp"
 "${sourceRoot}/components/CDataTableModel.cpp"
 "${sourceRoot}/components/CDataTableModel.h"
 "${sourceRoot}/components/CUITable.cpp"
 "${sourceRoot}/components/CUITable.h")
target_include_directories(TableComponentTests PRIVATE "${sourceRoot}/components")
target_compile_options(TableComponentTests PRIVATE /utf-8 /W4)
target_link_libraries(TableComponentTests PRIVATE Qt6::Widgets Qt6::Test)
enable_testing()
add_test(NAME TableComponentTests COMMAND TableComponentTests)
set_tests_properties(TableComponentTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
