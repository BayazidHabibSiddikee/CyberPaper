cmake_minimum_required(VERSION 3.20)
project(swordfilemanager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

add_executable(swordfm
    # app — entry point, main window shell, shared theme constants
    src/app/main.cpp
    src/app/mainwindow.cpp

    # model — data layer: filesystem model, filter proxy, recursive search
    src/model/filemodel.cpp
    src/model/filefilter.cpp
    src/model/searchmodel.cpp
    src/model/filescanner.cpp

    # view — the file listing itself (details / icon / search pages)
    src/view/fileview.cpp

    # panel — chrome around the listing
    src/panel/sidebar.cpp
    src/panel/toolbar.cpp
    src/panel/statusbar.cpp
    src/panel/previewpanel.cpp

    # ops — actions performed on files
    src/ops/fileops.cpp
    src/ops/archiveops.cpp
    src/ops/convertops.cpp
    src/ops/shareops.cpp
    src/ops/contextmenu.cpp
    src/ops/openwith.cpp
    src/ops/termutil.cpp

    resources/resources.qrc
)

target_include_directories(swordfm PRIVATE src)
target_link_libraries(swordfm PRIVATE Qt6::Widgets Qt6::Core)

install(TARGETS swordfm RUNTIME DESTINATION bin)
