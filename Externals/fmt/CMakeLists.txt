add_subdirectory(fmt)
dolphin_disable_warnings(fmt)

# See https://github.com/fmtlib/fmt/issues/4740
target_compile_definitions(fmt PUBLIC FMT_CONSTEVAL=)
