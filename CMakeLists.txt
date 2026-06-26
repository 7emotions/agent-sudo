cmake_minimum_required(VERSION 3.22)
project(agent-sudo-gui VERSION 2.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_compile_options(-O2)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Svg Multimedia)
find_package(Eigen3 REQUIRED)

# ══════════════════════════════════════════════════════════════════════════
#  creeper-qt (vendored source)
# ══════════════════════════════════════════════════════════════════════════

set(CREEPER_QT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/creeper-qt")

set(CREEPER_QT_SOURCES
    ${CREEPER_QT_DIR}/layout/flow.cc
    ${CREEPER_QT_DIR}/layout/mixer.cc
    ${CREEPER_QT_DIR}/layout/scroll.cc
    ${CREEPER_QT_DIR}/utility/animation/animatable.cc
    ${CREEPER_QT_DIR}/utility/painter/helper.cc
    ${CREEPER_QT_DIR}/utility/solution/round-angle.cc
    ${CREEPER_QT_DIR}/utility/theme/theme.cc
    ${CREEPER_QT_DIR}/widget/buttons/filled-button.cc
    ${CREEPER_QT_DIR}/widget/buttons/icon-button.cc
    ${CREEPER_QT_DIR}/widget/dropdown-menu.cc
    ${CREEPER_QT_DIR}/widget/image.cc
    ${CREEPER_QT_DIR}/widget/main-window.cc
    ${CREEPER_QT_DIR}/widget/sliders.cc
    ${CREEPER_QT_DIR}/widget/sliders.hh
    ${CREEPER_QT_DIR}/widget/snackbar.cc
    ${CREEPER_QT_DIR}/widget/switch.cc
    ${CREEPER_QT_DIR}/widget/text-fields.cc
    ${CREEPER_QT_DIR}/widget/widget.cc
)

add_library(creeper-qt-widgets STATIC ${CREEPER_QT_SOURCES})
target_include_directories(creeper-qt-widgets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
)
target_link_libraries(creeper-qt-widgets
    PUBLIC
        Qt6::Widgets
    PRIVATE
        Qt6::Network
        Eigen3::Eigen
)

# ══════════════════════════════════════════════════════════════════════════
#  agent-sudo-flush
# ══════════════════════════════════════════════════════════════════════════

add_executable(agent-sudo-flush
    resources.qrc
    src/main.cc
    src/gui/command_card.cc
    src/gui/icon.cc
    src/gui/ring_countdown.cc
    src/gui/sound.cc
    src/gui/theme_transition.cc
    src/gui/queue_io.cc
    src/gui/executor.cc
)

target_include_directories(agent-sudo-flush
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(agent-sudo-flush
    PRIVATE
        creeper-qt-widgets
        Qt6::Widgets
        Qt6::Network
        Qt6::Svg
        Qt6::Multimedia
)

# Install
install(TARGETS agent-sudo-flush RUNTIME DESTINATION bin)
