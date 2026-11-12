cmake_minimum_required(VERSION 3.21)
project(StrategyManagementTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../../XTrader/components")
find_package(Qt6 REQUIRED COMPONENTS Widgets)
set(taskRoot "${CMAKE_CURRENT_SOURCE_DIR}/../../XTrader")
add_executable(StrategyManagementTests StrategyManagementTests.cpp
 "${taskRoot}/components/CKeyboardSprite.cpp"
 "${taskRoot}/components/CKeyboardSprite.h"
 "${taskRoot}/components/CViewTradeAccounts.cpp"
 "${taskRoot}/components/CTradeAccountLoginDialog.cpp"
 "${taskRoot}/components/CUIStyle.cpp"
 "${taskRoot}/components/CUITable.cpp"
 "${taskRoot}/components/CUITable.h"
 "${taskRoot}/components/CDataTableModel.cpp"
 "${taskRoot}/components/CDataTableModel.h"
 "${taskRoot}/components/CStrategyEditDialog.cpp"
 "${taskRoot}/components/CStrategyEditDialog.h"
 "${taskRoot}/components/CViewStrategySettings.cpp"
 "${taskRoot}/components/CViewStrategySettings.h"
 "${taskRoot}/service/strategy/CStrategyService.cpp"
 "${taskRoot}/request/request.cpp"
 "${taskRoot}/request/request.pb.cc"
 "${taskRoot}/request/v1/market.pb.cc"
 "${taskRoot}/request/RequestCenter.cpp"
 "${taskRoot}/request/MarketTypes.cpp"
 "${taskRoot}/basic/CDatable.cpp")
target_compile_definitions(StrategyManagementTests PRIVATE PROTOBUF_USE_DLLS NOMINMAX XTRADER_COMPONENTS_PATH="${taskRoot}/components")
target_compile_options(StrategyManagementTests PRIVATE /utf-8 /w)
target_include_directories(StrategyManagementTests PRIVATE "${taskRoot}" "${taskRoot}/components" "${taskRoot}/request" "${taskRoot}/include/debug/protobuf" "${taskRoot}/include/debug/libevent" "${taskRoot}/include/debug/rapidjson")
target_link_directories(StrategyManagementTests PRIVATE "${taskRoot}/libs/debug")
target_link_libraries(StrategyManagementTests PRIVATE Qt6::Widgets libprotobufd abseil_dll ws2_32)
enable_testing()
add_test(NAME StrategyManagementTests COMMAND StrategyManagementTests)
set_tests_properties(StrategyManagementTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
