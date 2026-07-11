cmake_minimum_required(VERSION 3.30)

### Project
project(gview
	DESCRIPTION "GGUF metadata viewer and visualizer")


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(ENABLE_CLANG_TIDY "Run clang-tidy linter" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)


if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(wxBUILD_USE_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
    set(wxBUILD_SHARED OFF CACHE BOOL "" FORCE)
endif(MSVC)

# Silence warnings in GCC that contain lots of false positives
if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -Wno-array-bounds -Wno-stringop-overflow -Wno-maybe-uninitialized")
endif()


set(wxUSE_WEBVIEW FALSE)
set(wxUSE_STC FALSE)
set(wxUSE_RIBBON FALSE)
set(wxUSE_PROPGRID FALSE)
set(wxUSE_RICHTEXT FALSE)
set(wxUSE_MEDIACTRL FALSE)
if (NOT wxBUILD_SHARED)
    set(wxUSE_HTML FALSE)
    set(wxUSE_WXHTML_HELP FALSE)
endif()

include(FetchContent)
FetchContent_Declare(
    wxwidgets
    URL https://github.com/wxWidgets/wxWidgets/releases/download/v3.3.2/wxWidgets-3.3.2.tar.bz2
    URL_HASH SHA256=50a28cb668de47b0e006cd6ebed8cf4f76c1cac6116fb3c978c44478219103f2
    FIND_PACKAGE_ARGS 3.3.2 NAMES wxWidgets
)
FetchContent_MakeAvailable(wxwidgets)



add_executable(gview
    src/main.cc
    src/gapp.cc
    src/gframe.cc
    src/gguf_reader.cc
)

target_include_directories(gview PRIVATE src)

if(ENABLE_CLANG_TIDY)
    set_target_properties(gview PROPERTIES CXX_CLANG_TIDY "clang-tidy")
endif(ENABLE_CLANG_TIDY)

if(ENABLE_ASAN)
    if(MSVC)
        target_compile_options(gview PRIVATE /fsanitize=address)
    else()
        target_compile_options(gview PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(gview PRIVATE -fsanitize=address)
    endif()
endif(ENABLE_ASAN)



if(APPLE)
    set_target_properties(gview PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "${CMAKE_PROJECT_NAME}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${CMAKE_PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2026 ibx86 . All rights reserved."
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.gguf.inspector"
        MACOSX_BUNDLE_ICON_FILE "App.icns"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/platform/osx/Info.plist"
    )
elseif(WIN32)
    target_sources(gview PRIVATE
        platform/win/resources.rc)
    set_target_properties(gview PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()


target_link_libraries(gview PRIVATE  wx::core)

