if (NOT MSVC)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(SRCS
  imgui.cpp
  imgui_demo.cpp
  imgui_draw.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
)

add_library(imgui STATIC ${SRCS})
dolphin_disable_warnings(imgui)
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# Tell ImGui to both declare and emit the math operators in THIS target:
target_compile_definitions(imgui
  PUBLIC
    IMGUI_DEFINE_MATH_OPERATORS
)

# Disable gcc's interprocedural scalar replacement to make template instantiations globally visible
# This is necessary because we rely on imgui's private api... yikes.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(imgui PRIVATE -fno-ipa-sra)
endif()

target_link_libraries(imgui
PRIVATE
  fmt::fmt
)
