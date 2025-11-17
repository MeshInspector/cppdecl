project(
    'cppdecl',
    'cpp',
    default_options: ['warning_level=3', 'cpp_std=c++20'],
)

cxx = meson.get_compiler('cpp')
if cxx.get_argument_syntax() != 'msvc'
    add_project_arguments(['-pedantic-errors', '-Wconversion', '-Wno-implicit-int-float-conversion', '-Wextra-semi', '-Wdeprecated'], language: 'cpp')
endif

install_headers(
    'include/cppdecl/misc/demangler.h',
    'include/cppdecl/misc/enum_flags.h',
    'include/cppdecl/misc/indirect_optional.h',
    'include/cppdecl/misc/overload.h',
    'include/cppdecl/misc/platform.h',
    'include/cppdecl/misc/string_helpers.h',
    install_dir: 'cppdecl/misc'
)
install_headers(
    'include/cppdecl/declarations/data.h',
    'include/cppdecl/declarations/parse_simple.h',
    'include/cppdecl/declarations/parse.h',
    'include/cppdecl/declarations/simplify.h',
    'include/cppdecl/declarations/to_string.h',
    install_dir: 'cppdecl/declarations'
)
install_headers(
    'include/cppdecl/declarations/simplify_modules/all.h',
    'include/cppdecl/declarations/simplify_modules/phmap.h',
    install_dir: 'cppdecl/declarations/simplify_modules'
)
install_headers(
    'include/cppdecl/type_name.h',
    install_dir: 'cppdecl'
)

idir = include_directories('include')

executable(
    'tests',
    'source/tests.cpp',
    include_directories: idir
)

executable(
    'cppdecl',
    'source/repl.cpp',
    install: true,
    include_directories: idir,
)

# This is experimental and as such isn't installed.
# See the comments in the source file for why.
executable(
    'cppdecl_prettify_errors',
    'source/prettify_errors.cpp',
    # install: True,
    include_directories: idir,
)
