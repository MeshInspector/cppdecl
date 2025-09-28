#pragma once

#include "cppdecl/declarations/data.h"
#include "cppdecl/misc/enum_flags.h"
#include "cppdecl/misc/mixins.h"
#include "cppdecl/misc/overload.h"
#include "cppdecl/misc/platform.h"

// Unfortunate to add those two. We use them to roundtrip numeric literals: [
#include "cppdecl/declarations/to_string.h"
#include "cppdecl/declarations/parse.h"
// ]

#include <cassert>
#include <iterator>
#include <type_traits>
#include <version>

// There are several helper functions here, but the primary thing you should use is `Simplify()`.
// Note that it does nothing if the flags are empty.

namespace cppdecl
{
    enum class SimplifyFlags
    {
        // Fixes for compiler/stdlib-specific quirks:
        // Those should do nothing if applied to other platforms.

        // Remove MSVC's `__ptr32` and `__ptr64` cv-qualifiers on pointers.
        bit_msvc_remove_ptr32_ptr64 = 1 << 0,

        // Replace `std::__cxx11` with `std`.
        bit_libstdcxx_remove_cxx11_namespace_in_std = 1 << 1,
        // Replace `std::__1` with `std`.
        bit_libcpp_remove_version_namespace_in_std = 1 << 2,

        bits_remove_std_version_namespace =
            bit_libstdcxx_remove_cxx11_namespace_in_std |
            bit_libcpp_remove_version_namespace_in_std,


        // Rewrite iterators from their internal names.
        // Note that this is best effort. Some containers reuse iterators from each other, and you will not get entirely consistent names across compilers. There's a table in `tests.cpp` that shows the differences.
        // It's recommended to combine this with `bits_common_remove_defargs` and `bits_remove_std_version_namespace`, because we don't go out of our way to reproduce those things in our output.

        // While the other two `..._normalize_iterators` bits don't touch the stuff from other standard libraries, libstdc++ has some name collisions with MSVC STL.
        // So if this bit is set, and the MSVC STL bit isn't set, and a name from MSVC STL is given, you might get an incorrect result.
        bit_libstdcxx_normalize_iterators = 1 << 3,
        bit_libcpp_normalize_iterators = 1 << 4,
        bit_msvcstl_normalize_iterators = 1 << 5,
        bit_common_normalize_iterators = 1 << 6, // At the moment we don't use this directly, this is solely for mixins.

        bits_normalize_iterators =
            bit_libstdcxx_normalize_iterators |
            bit_libcpp_normalize_iterators |
            bit_msvcstl_normalize_iterators |
            bit_common_normalize_iterators,


        // Fixes for common C++ stuff:

        // Convert numeric literals to some normalized form.
        // This is equivalent to `ToCode(..., weakly_canonical_language_agnostic)`. We don't expose the individual numeric literal knobs because there's too many of them.
        bit_common_normalize_numbers = 1 << 7,

        // Remove elaborated type specifiers, `typename`, etc.
        bit_common_remove_type_prefix = 1 << 8,

        // Remove `signed`, except from `signed char`.
        // Will also not touch `signed` unless you also specify `bit_common_add_implied_int`, which turns it into `signed int`,
        //   and then this flag turns it into just `int`.
        bit_common_remove_redundant_signed = 1 << 9,

        // Remove `int` from `short int`, `long int`, `long long int`.
        bit_common_remove_redundant_int = 1 << 10,

        // Remove the "implied int" flag, forcing `int` to be spelled out.
        // This is used only for `unsigned [int]` and `signed [int]`, and has no overload with `bit_common_remove_redundant_int`.
        bit_common_add_implied_int = 1 << 11,

        // Remove allocators from template parameters.
        bit_common_remove_defarg_allocator = 1 << 12,
        // Remove `std::char_traits<T>` from template parameters of `std::basic_string`.
        // This typically requires `bit_common_remove_defarg_allocator` as well, since we can't remove non-last template arguments,
        //   and having the allocator after this one will prevent it from being removed.
        bit_common_remove_defarg_char_traits = 1 << 13,
        // Remove `std::less<T>` and `std::equal_to<T>` from ordered and unordered containers respectively.
        // This typically requires `bit_common_remove_defarg_allocator` as well, since we can't remove non-last template arguments,
        //   and having the allocator after this one will prevent it from being removed.
        bit_common_remove_defarg_comparator = 1 << 14,
        // Remove `std::hash<T>` from unordered containers.
        // This typically requires `bit_common_remove_defarg_allocator` and `bit_common_remove_defarg_comparator` as well, since we can't remove non-last template arguments,
        //   and having the allocator and comparator after this one will prevent it from being removed.
        bit_common_remove_defarg_hash_functor = 1 << 15,
        // Remove `std::default_delete<T>` from `std::unique_ptr`.
        bit_common_remove_defarg_default_delete = 1 << 16,
        // Unused by default. Custom mixins might use this.
        bit_common_remove_defargs_other = 1 << 17,

        // Remove various default arguments from templates.
        bits_common_remove_defargs =
            bit_common_remove_defarg_allocator |
            bit_common_remove_defarg_char_traits |
            bit_common_remove_defarg_comparator |
            bit_common_remove_defarg_hash_functor |
            bit_common_remove_defarg_default_delete |
            bit_common_remove_defargs_other,

        // Rewrite `std::basic_string<char>` to `std::string` and such.
        // This typically requires `bit_common_remove_defarg_hash_functor`, `bit_common_remove_defarg_allocator`, and `bit_common_remove_defarg_comparator` as well,
        //   since this expects the default template arguments to be already stripped.
        bit_common_rewrite_template_specializations_as_typedefs = 1 << 18,

        // Rewrite `std::array<T, 42ul>` and such to just `std::array<T, 42>`.
        // We can't act on ALL numeric literals, because this might lose information if the template parameter is `auto`.
        // So we can only act on known classes such as `std::array`.
        bit_common_remove_numeric_literal_suffixes_from_known_good_template_params = 1 << 19,

        // Various mostly compiler-independent bits.
        // Note that `bits_common_remove_defargs` isn't needed when you get the types from `__PRETTY_FUNCTION__` or equivalent on Clang.
        common =
            bit_common_normalize_iterators |
            bit_common_normalize_numbers |
            bit_common_remove_type_prefix |
            bit_common_remove_redundant_signed |
            bit_common_remove_redundant_int |
            bit_common_add_implied_int |
            bits_common_remove_defargs |
            bit_common_rewrite_template_specializations_as_typedefs |
            bit_common_remove_numeric_literal_suffixes_from_known_good_template_params,


        // Fixes for C stuff:

        // Rewrite `_Bool` as `bool`.
        bit_c_normalize_bool = 1 << 20,

        c =
            bit_c_normalize_bool,


        // Presents for different compilers:
        // Those should do nothing if applied to the names from the wrong compiler.

        // MSVC and Clang in MSVC-compatible mode.
        compiler_msvc_like = bit_msvc_remove_ptr32_ptr64,

        compiler_all = compiler_msvc_like,
        compiler_current = 0
            #ifdef _MSC_VER
            | compiler_msvc_like
            #endif
            ,

        // Presets for different standard libraries:
        stdlib_libstdcxx =
            bit_libstdcxx_remove_cxx11_namespace_in_std |
            bit_libstdcxx_normalize_iterators,
        stdlib_libcpp =
            bit_libcpp_remove_version_namespace_in_std |
            bit_libcpp_normalize_iterators,
        stdlib_msvcstl =
            bit_msvcstl_normalize_iterators,

        stdlib_all = stdlib_libstdcxx | stdlib_libcpp | stdlib_msvcstl,
        stdlib_current = 0
            #ifdef _GLIBCXX_RELEASE
            | stdlib_libstdcxx
            #endif
            #ifdef _LIBCPP_VERSION
            | stdlib_libcpp
            #endif
            #ifdef _MSVC_STL_VERSION
            | stdlib_msvcstl
            #endif
            ,


        // High-level flags:

        // Absolutely every rule we know. Good if the names come from outside of the program.
        all = common | c | compiler_all | stdlib_all,
        // Only the rules that are relevant on the current compiler and standard library.
        native =
            (common | compiler_current | stdlib_current) // Intentionally excluding `c` here, since this is a C++ library.
            #ifndef _MSC_VER
            // Only MSVC-like compilers print elaborated type specifiers.
            & ~bit_common_remove_type_prefix
            #endif
            ,
        // Only the rules that are relevant on the current compiler and standard library, with the assumption that the type names come from a method based on `__PRETTY_FUNCTION__`, `__FUNCSIG__`, `std::source_location::function_name()`.
        native_func_name_based_only = native
            #ifdef __GNUC__
            // GCC and Clang seem to remove default template arguments automatically.
            & ~bits_common_remove_defargs
            #endif
            ,


        // Extra flags:
        // Those can be lossy, and produce invalid C++ types.

        // Rewrite both `std::expected` and `tl::expected` as just `expected`.
        bit_extra_merge_std_tl_expected = 1 << 21,
    };
    CPPDECL_FLAG_OPERATORS(SimplifyFlags)

    // A CRTP base. Do not use directly, use `SimplifyTraits`.
    template <typename Derived>
    struct BasicSimplifyTraits
    {
        // This class should call all member functions of itself though those, to allow customization in derived classes.
        [[nodiscard]] CPPDECL_CONSTEXPR       Derived &GetDerived()       {return static_cast<      Derived &>(*this);}
        [[nodiscard]] CPPDECL_CONSTEXPR const Derived &GetDerived() const {return static_cast<const Derived &>(*this);}

        // If this returns false, the `Simplify()` call does nothing.
        [[nodiscard]] CPPDECL_CONSTEXPR bool ShouldAct(SimplifyFlags flags) const
        {
            return flags != SimplifyFlags{};
        }

        // Those are only used in this class:

        [[nodiscard]] CPPDECL_CONSTEXPR bool IsLibcppVersionNamespace(std::string_view word)
        {
            return
                word == "__1" ||
                word == "__2" || // Seen on Emscripten.
                word == "__ndk1"; // Seen in LLVM's CMakeLists.txt, supposedly used on Android.
        }

        // If `name` is of the form `std[::{__cxx11,__1}]::A[::...]`, returns A. Otherwise returns an empty string.
        // Only permits the trailing `::...` if `index` isn't null.
        // On success, if `index` isn't null, writes the index of `A` into `*index`. That's typically 1, or 2 if we had to skip the version namespace.
        // On failure intentionally leaves `index` untouched.
        // Handling the version namespaces here is a bit redundant, but it allows us to do stuff independently from removing those namespaces, which is nice.
        [[nodiscard]] CPPDECL_CONSTEXPR std::string_view AsStdName(const QualifiedName &name, std::size_t *index = nullptr)
        {
            if (name.parts.size() < 2)
                return "";
            if (name.parts.front().AsSingleWord() != "std")
                return "";
            std::size_t part_index = 1;
            if (name.parts.at(1).AsSingleWord() == "__cxx11" || GetDerived().IsLibcppVersionNamespace(name.parts.at(1).AsSingleWord()))
                part_index++;
            if (index ? name.parts.size() < part_index + 1 : name.parts.size() != part_index + 1)
                return "";
            if (index)
                *index = part_index;

            return name.parts.at(part_index).AsSingleWord(SingleWordFlags::ignore_template_args);
        }
        // Same but for types.
        [[nodiscard]] CPPDECL_CONSTEXPR std::string_view AsStdName(const Type &type, std::size_t *index = nullptr, std::size_t num_ignored_modifiers = 0, bool require_const = false)
        {
            return
                !type.IsEmpty() &&
                type.modifiers.size() == num_ignored_modifiers &&
                type.simple_type.IsOnlyQualifiedName(SingleWordFlags::ignore_type_prefixes | require_const * SingleWordFlags::require_const)
                ? GetDerived().AsStdName(type.simple_type.name, index)
                : "";
        }


        // Those can be overridden:

        // Note that all those must leave `index` untouched if they return false.

        // `A<T, std::char_traits<T>, std::allocator<T>>`
        // Checks only the name `A`, ignoring the trailing template arguments and any extra unqualified parts after `A`.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsStringLike(const QualifiedName &name, std::size_t *index)
        {
            // This has to be a separate variable because we don't want to write to `*index` if the string comparison is false,
            //   but `AsStdName()` has still succeeded.
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                std_name == "basic_string";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // `A<T, std::allocator<T>>`
        // Checks only the name `A`, ignoring the trailing template arguments and any extra unqualified parts after `A`.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsVectorLike(const QualifiedName &name, std::size_t *index)
        {
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                std_name == "vector" ||
                std_name == "list" ||
                std_name == "forward_list" ||
                std_name == "deque" ||
                std_name == "hive";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // `A<T, std::less<T>, std::allocator<T>>`
        // Checks only the name `A`, ignoring the trailing template arguments and any extra unqualified parts after `A`.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsOrderedSetLike(const QualifiedName &name, std::size_t *index)
        {
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                std_name == "set" ||
                std_name == "multiset";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // `A<T, U, std::less<T>, std::allocator<std::pair<const T, U>>>`
        // Checks only the name `A`, ignoring the trailing template arguments and any extra unqualified parts after `A`.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsOrderedMapLike(const QualifiedName &name, std::size_t *index)
        {
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                std_name == "map" ||
                std_name == "multimap";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // `A<T, std::hash<T>, std::equal_to<T>, std::allocator<T>>`
        // Checks only the name `A`, ignoring the trailing template arguments and any extra unqualified parts after `A`.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsUnorderedSetLike(const QualifiedName &name, std::size_t *index)
        {
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                std_name == "unordered_set" ||
                std_name == "unordered_multiset";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // `A<T, U, std::hash<T>, std::equal_to<T>, std::allocator<std::pair<const T, U>>>`
        // Checks only the name `A`, ignoring the trailing template arguments and any extra unqualified parts after `A`.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsUnorderedMapLike(const QualifiedName &name, std::size_t *index)
        {
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                std_name == "unordered_map" ||
                std_name == "unordered_multimap";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // One of the various type that have char traits as the second template argument.
        // I got this list by searching for `preferred_name` (case-independent) in libc++ sources.
        [[nodiscard]] CPPDECL_CONSTEXPR bool HasCharTraits(const QualifiedName &name, std::size_t *index)
        {
            // This has to be a separate variable because we don't want to write to `*index` if the string comparison is false,
            //   but `AsStdName()` has still succeeded.
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                // We rely on all those having typedefs without `basic_` for specific character types.
                // This assumption is made in `SpecializationsHaveTypedefsForCharTypes`. If that's not the case for your types, the logic will have to be changed somehow.
                std_name == "basic_string" ||
                std_name == "basic_string_view" ||
                std_name == "basic_ios" ||
                std_name == "basic_filebuf" ||
                std_name == "basic_streambuf" ||
                std_name == "basic_stringbuf" ||
                std_name == "basic_istream" ||
                std_name == "basic_ostream" ||
                std_name == "basic_iostream" ||
                std_name == "basic_stringstream" ||
                std_name == "basic_istringstream" ||
                std_name == "basic_ostringstream" ||
                std_name == "basic_ostringstream" ||
                std_name == "basic_fstream" ||
                std_name == "basic_ifstream" ||
                std_name == "basic_ofstream";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // `A<T, std::default_delete<T>>`
        // Checks only the name `A`, ignoring the trailing template arguments and any extra unqualified parts after `A`.
        [[nodiscard]] CPPDECL_CONSTEXPR bool HasDefaultDelete(const QualifiedName &name, std::size_t *index)
        {
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                std_name == "unique_ptr";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // Checks if `deleter_type` is `std::default_delete<T>`, for `T` equal to `target_type`.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsDefaultDelete(const Type &deleter_type, const Type &target_type)
        {
            if (
                GetDerived().AsStdName(deleter_type) == "default_delete" &&
                deleter_type.simple_type.name.parts.back().template_args &&
                deleter_type.simple_type.name.parts.back().template_args->args.size() == 1
            )
            {
                auto deleter_targ = std::get_if<Type>(&deleter_type.simple_type.name.parts.back().template_args->args.front().var);
                return  deleter_targ && *deleter_targ == target_type;
            }

            return false;
        }

        // Whether this name can be shortened by removing the single template argument of a char type and replacing the `basic_` prefix (or some other?) with the abbreviation of the char type.
        // `resulting_string_base` on success receives the part of the name after `basic_`.
        // `allow_all_char_types` on success receives true if this type understands `char{8,16,32}_t` too, not just `char` and `wchar_t`.
        // The prefix to prepend to `resulting_string_base` is assumed to be always fixed.
        [[nodiscard]] CPPDECL_CONSTEXPR bool SpecializationsHaveTypedefsForCharTypes(const QualifiedName &name, std::size_t *index, std::string_view *resulting_string_base, bool *allow_all_char_types)
        {
            // This has to be a separate variable because we don't want to write to `*index` if the string comparison is false,
            //   but `AsStdName()` has still succeeded.
            std::size_t std_index = 0;

            // Intentionally no `GetDerived().` here.
            if (HasCharTraits(name, &std_index))
            {
                if (index)
                    *index = std_index;

                std::string_view name_word = name.parts.at(std_index).AsSingleWord(SingleWordFlags::ignore_template_args);
                assert(name_word.starts_with("basic_")); // We assume that the name starts with `basic_`.
                if (resulting_string_base)
                    *resulting_string_base = name_word.substr(6); // Remove `basic_`.
                if (allow_all_char_types)
                    *allow_all_char_types = name_word == "basic_string" || name_word == "basic_string_view";

                return true;
            }

            return false;
        }


        // Is this a character traits type that we should remove from `IsStringLike()` types?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsCharTraits(const Type &type)
        {
            return GetDerived().AsStdName(type) == "char_traits";
        }
        // Is this an allocator type that we can remove?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsAllocatorName(const Type &type)
        {
            return GetDerived().AsStdName(type) == "allocator";
        }
        // Is this a pair type? (That will appear e.g. as the allocator parameter in maps.)
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsPair(const Type &type, std::size_t num_ignored_modifiers = 0, bool require_const = false)
        {
            return GetDerived().AsStdName(type, nullptr, num_ignored_modifiers, require_const) == "pair";
        }
        // Is this an allocator type for `elem_type`, that we can remove?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsAllocatorFor(const Type &allocator_type, const Type &elem_type)
        {
            if (!GetDerived().IsAllocatorName(allocator_type))
                return false;

            if (
                allocator_type.simple_type.name.parts.back().template_args &&
                allocator_type.simple_type.name.parts.back().template_args->args.size() == 1
            )
            {
                if (auto allocator_targ = std::get_if<Type>(&allocator_type.simple_type.name.parts.back().template_args->args.front().var))
                {
                    return elem_type == *allocator_targ;
                }
            }

            return false;
        }
        // Is this a `std::pair<const elem_type_first, elem_type_second>` (pair-ness is checked with `IsPair()`).
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsConstNonconstPair(const Type &pair_type, const Type &elem_type_first, const Type &elem_type_second, std::size_t num_modifiers_to_skip_in_pair_type = 0, bool require_const = false)
        {
            if (
                GetDerived().IsPair(pair_type, num_modifiers_to_skip_in_pair_type, require_const) &&
                pair_type.simple_type.name.parts.back().template_args &&
                pair_type.simple_type.name.parts.back().template_args->args.size() == 2
            )
            {
                auto allocator_targ0 = std::get_if<Type>(&pair_type.simple_type.name.parts.back().template_args->args.at(0).var);
                auto allocator_targ1 = std::get_if<Type>(&pair_type.simple_type.name.parts.back().template_args->args.at(1).var);
                if (
                    allocator_targ0 && allocator_targ1 &&
                    allocator_targ0->IsConst() &&
                    allocator_targ0->Equals(elem_type_first, Type::EqualsFlags::as_if_target_is_const) && // 0th one differs in constness.
                    *allocator_targ1 == elem_type_second // Here constness matches exactly.
                )
                {
                    return true;
                }
            }

            return false;
        }
        // Is this an allocator type for `std::pair<const elem_type_first, elem_type_second>` (pair-ness is checked with `IsPair()`), that we can remove?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsAllocatorForPair(const Type &allocator_type, const Type &elem_type_first, const Type &elem_type_second)
        {
            if (!GetDerived().IsAllocatorName(allocator_type))
                return false;

            if (
                allocator_type.simple_type.name.parts.back().template_args &&
                allocator_type.simple_type.name.parts.back().template_args->args.size() == 1
            )
            {
                if (auto allocator_targ = std::get_if<Type>(&allocator_type.simple_type.name.parts.back().template_args->args.front().var))
                {
                    return GetDerived().IsConstNonconstPair(*allocator_targ, elem_type_first, elem_type_second);
                }
            }

            return false;
        }
        // Is this a less comparator that should be removed from ordered containers?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsLessComparator(const Type &type)
        {
            return GetDerived().AsStdName(type) == "less";
        }
        // Is this an equality comparator that should be removed from unordered containers?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsEqualToComparator(const Type &type)
        {
            return GetDerived().AsStdName(type) == "equal_to";
        }
        // Is this a hash functor that should be removed from unordered containers?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsHashFunctor(const Type &type)
        {
            return GetDerived().AsStdName(type) == "hash";
        }


        // --- Simplification functions:

        // Simplify a name with the assumption that it's a type name.
        // This is a low-level function, prefer `Simplify()`.
        CPPDECL_CONSTEXPR void SimplifyQualifiedNameNonRecursively(SimplifyFlags flags, QualifiedName &name)
        {
            // Rewrite `_Bool` as `bool`.
            if (bool(flags & SimplifyFlags::bit_c_normalize_bool) && name.AsSingleWord() == "_Bool")
            {
                name.parts.at(0).var = "bool";
                return; // Surely we don't need to check anything else.
            }

            // Rewrite `std::expected` and `tl::expected` as just `expected`.
            if (bool(flags & SimplifyFlags::bit_extra_merge_std_tl_expected))
            {
                bool ok = false;

                // std::expected
                std::size_t name_index = std::size_t(-1);
                if (!ok && GetDerived().AsStdName(name, &name_index) == "expected")
                {
                    ok = true;
                    name.parts.erase(name.parts.begin(), name.parts.begin() + std::ptrdiff_t(name_index));
                }

                // tl::expected
                if (!ok && name.parts.size() >= 2 && name.parts.at(0).AsSingleWord() == "tl" && name.parts.at(1).AsSingleWord(SingleWordFlags::ignore_template_args) == "expected")
                {
                    ok = true;
                    name.parts.erase(name.parts.begin());
                }

                if (ok)
                    return; // Surely we don't need to check anything else.
            }

            // Remove the version namespace from std.
            if (bool(flags & (SimplifyFlags::bit_libstdcxx_remove_cxx11_namespace_in_std | SimplifyFlags::bit_libcpp_remove_version_namespace_in_std)))
            {
                // The first part of `name` is `std`, and there are at least two parts.
                bool is_in_std = name.parts.size() >= 2 && name.parts.front().AsSingleWord() == "std";

                bool removed_std_version_namespace = false;
                if (!removed_std_version_namespace && bool(flags & SimplifyFlags::bit_libstdcxx_remove_cxx11_namespace_in_std))
                {
                    if (is_in_std && name.parts.at(1).AsSingleWord() == "__cxx11")
                    {
                        removed_std_version_namespace = true;
                        name.parts.erase(name.parts.begin() + 1);
                    }
                }
                if (!removed_std_version_namespace && bool(flags & SimplifyFlags::bit_libcpp_remove_version_namespace_in_std))
                {
                    if (is_in_std && GetDerived().IsLibcppVersionNamespace(name.parts.at(1).AsSingleWord()))
                    {
                        removed_std_version_namespace = true;
                        name.parts.erase(name.parts.begin() + 1);
                    }
                }
            }

            { // Rewrite iterator names.
                bool already_normalized_iter = false;

                // MSVC STL
                // This has to run before libstdc++ iterator rewrites, because there is some name overlaps.
                if (!already_normalized_iter && bool(flags & SimplifyFlags::bit_msvcstl_normalize_iterators))
                {
                    // Not using `GetDerived().AsStdName()` here because MSVC STL doesn't use version namespaces.

                    if (
                        name.parts.size() >= 2 &&
                        name.parts.at(0).AsSingleWord() == "std" &&
                        name.parts.at(1).IsSingleWord(SingleWordFlags::ignore_template_args) &&
                        name.parts.at(1).template_args
                    )
                    {
                        const std::string_view word1 = name.parts.at(1).AsSingleWord(SingleWordFlags::ignore_template_args);

                        // std::array
                        if (!already_normalized_iter)
                        {
                            const bool is_mut = word1 == "_Array_iterator";
                            const bool is_const = !is_mut && word1 == "_Array_const_iterator";

                            if (
                                (is_mut || is_const) &&
                                name.parts.at(1).template_args->args.size() == 2
                            )
                            {
                                // Success!
                                already_normalized_iter = true;

                                name.parts.at(1).var = "array";
                                name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                            }
                        }

                        // The rest of the containers.
                        if (
                            !already_normalized_iter &&
                            name.parts.at(1).template_args->args.size() == 1
                        )
                        {
                            if (
                                auto targ = std::get_if<Type>(&name.parts.at(1).template_args->args.front().var);
                                targ &&
                                targ->IsOnlyQualifiedName(SingleWordFlags::ignore_type_prefixes) &&
                                targ->simple_type.name.parts.size() == 2 &&
                                targ->simple_type.name.parts.at(0).AsSingleWord() == "std" &&
                                targ->simple_type.name.parts.at(1).IsSingleWord(SingleWordFlags::ignore_template_args) &&
                                targ->simple_type.name.parts.at(1).template_args &&
                                targ->simple_type.name.parts.at(1).template_args->args.size() == 1
                            )
                            {
                                const std::string_view word2 = targ->simple_type.name.parts.at(1).AsSingleWord(SingleWordFlags::ignore_template_args);

                                if (
                                    auto sub_targ = std::get_if<Type>(&targ->simple_type.name.parts.at(1).template_args->args.front().var);
                                    sub_targ &&
                                    sub_targ->IsOnlyQualifiedName(SingleWordFlags::ignore_type_prefixes) &&
                                    sub_targ->simple_type.name.parts.size() == 2 &&
                                    sub_targ->simple_type.name.parts.at(0).AsSingleWord() == "std" &&
                                    sub_targ->simple_type.name.parts.at(1).IsSingleWord(SingleWordFlags::ignore_template_args) &&
                                    sub_targ->simple_type.name.parts.at(1).template_args &&
                                    sub_targ->simple_type.name.parts.at(1).template_args->args.size() == 1
                                )
                                {
                                    const std::string_view word3 = sub_targ->simple_type.name.parts.at(1).AsSingleWord(SingleWordFlags::ignore_template_args);

                                    if (auto elem_type_targ = std::get_if<Type>(&sub_targ->simple_type.name.parts.at(1).template_args->args.front().var))
                                    {
                                        // std::vector
                                        if (!already_normalized_iter && word2 == "_Vector_val" && word3 == "_Simple_types")
                                        {
                                            const bool is_mut = word1 == "_Vector_iterator";
                                            const bool is_const = !is_mut && word1 == "_Vector_const_iterator";
                                            if (is_mut || is_const)
                                            {
                                                // Success!
                                                already_normalized_iter = true;

                                                name.parts.at(1).var = "vector";

                                                // Using a temporary variable because moving directly to the parent sounds unsafe.
                                                Type elem_type = std::move(*elem_type_targ);
                                                name.parts.at(1).template_args->args.front().var = std::move(elem_type);

                                                name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                                            }
                                        }

                                        // std::deque
                                        if (!already_normalized_iter && word2 == "_Deque_val" && word3 == "_Deque_simple_types")
                                        {
                                            const bool is_mut = word1 == "_Deque_iterator";
                                            const bool is_const = !is_mut && word1 == "_Deque_const_iterator";
                                            if (is_mut || is_const)
                                            {
                                                // Success!
                                                already_normalized_iter = true;

                                                name.parts.at(1).var = "deque";

                                                // Using a temporary variable because moving directly to the parent sounds unsafe.
                                                Type elem_type = std::move(*elem_type_targ);
                                                name.parts.at(1).template_args->args.front().var = std::move(elem_type);

                                                name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                                            }
                                        }

                                        // std::forward_list
                                        if (!already_normalized_iter && word2 == "_Flist_val" && word3 == "_Flist_simple_types")
                                        {
                                            const bool is_mut = word1 == "_Flist_iterator";
                                            const bool is_const = !is_mut && word1 == "_Flist_const_iterator";
                                            if (is_mut || is_const)
                                            {
                                                // Success!
                                                already_normalized_iter = true;

                                                name.parts.at(1).var = "forward_list";

                                                // Using a temporary variable because moving directly to the parent sounds unsafe.
                                                Type elem_type = std::move(*elem_type_targ);
                                                name.parts.at(1).template_args->args.front().var = std::move(elem_type);

                                                name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                                            }
                                        }

                                        // Is the element type a `std::pair` with the `const` first argument?
                                        const bool elem_type_is_map_like =
                                            !already_normalized_iter && // Not only because of perfomance, but also because `*elem_type_targ` can be moved-from in that case.
                                            elem_type_targ->IsOnlyQualifiedName(SingleWordFlags::ignore_type_prefixes) &&
                                            elem_type_targ->simple_type.name.parts.size() == 2 &&
                                            // Don't need `GetDerived().AsStdName()` here, because MSVC STL doesn't use version namespaces.
                                            elem_type_targ->simple_type.name.parts.at(0).AsSingleWord() == "std" &&
                                            elem_type_targ->simple_type.name.parts.at(1).AsSingleWord(SingleWordFlags::ignore_template_args) == "pair" &&
                                            elem_type_targ->simple_type.name.parts.at(1).template_args &&
                                            elem_type_targ->simple_type.name.parts.at(1).template_args->args.size() == 2 &&
                                            std::holds_alternative<Type>(elem_type_targ->simple_type.name.parts.at(1).template_args->args.at(0).var) &&
                                            std::holds_alternative<Type>(elem_type_targ->simple_type.name.parts.at(1).template_args->args.at(1).var) &&
                                            std::get<Type>(elem_type_targ->simple_type.name.parts.at(1).template_args->args.at(0).var).IsConst();

                                        // std::unordered_map (must be before `std::list`)
                                        // Uses the same iterator type as `std::list` with `std::pair` element type.
                                        if (!already_normalized_iter && elem_type_is_map_like && word2 == "_List_val" && word3 == "_List_simple_types")
                                        {
                                            const bool is_mut = word1 == "_List_iterator";
                                            const bool is_const = !is_mut && word1 == "_List_const_iterator";
                                            if (is_mut || is_const)
                                            {
                                                // Success!
                                                already_normalized_iter = true;

                                                name.parts.at(1).var = "unordered_map";

                                                // Using a temporary variable because moving directly to the parent sounds unsafe.
                                                TemplateArgumentList elem_types = std::move(elem_type_targ->simple_type.name.parts.back().template_args.value());
                                                std::get<Type>(elem_types.args.at(0).var).RemoveQualifiers(CvQualifiers::const_); // We have already confirmed that that template argument is a type.
                                                name.parts.at(1).template_args = std::move(elem_types);

                                                name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                                            }
                                        }

                                        // std::list
                                        if (!already_normalized_iter && word2 == "_List_val" && word3 == "_List_simple_types")
                                        {
                                            const bool is_mut = word1 == "_List_iterator";
                                            const bool is_const = !is_mut && word1 == "_List_const_iterator";
                                            if (is_mut || is_const)
                                            {
                                                // Success!
                                                already_normalized_iter = true;

                                                name.parts.at(1).var = "list";

                                                // Using a temporary variable because moving directly to the parent sounds unsafe.
                                                Type elem_type = std::move(*elem_type_targ);
                                                name.parts.at(1).template_args->args.front().var = std::move(elem_type);

                                                name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                                            }
                                        }

                                        // std::map (must be before `std::set`)
                                        if (!already_normalized_iter && elem_type_is_map_like && word2 == "_Tree_val" && word3 == "_Tree_simple_types")
                                        {
                                            const bool is_mut = word1 == "_Tree_iterator";
                                            const bool is_const = !is_mut && word1 == "_Tree_const_iterator";
                                            if (is_mut || is_const)
                                            {
                                                // Success!
                                                already_normalized_iter = true;

                                                name.parts.at(1).var = "map";

                                                // Using a temporary variable because moving directly to the parent sounds unsafe.
                                                TemplateArgumentList elem_types = std::move(elem_type_targ->simple_type.name.parts.back().template_args.value());
                                                std::get<Type>(elem_types.args.at(0).var).RemoveQualifiers(CvQualifiers::const_); // We have already confirmed that that template argument is a type.
                                                name.parts.at(1).template_args = std::move(elem_types);

                                                name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                                            }
                                        }

                                        // std::set
                                        if (!already_normalized_iter && word1 == "_Tree_const_iterator" && word2 == "_Tree_val" && word3 == "_Tree_simple_types")
                                        {
                                            // Success!
                                            already_normalized_iter = true;

                                            name.parts.at(1).var = "set";

                                            // Using a temporary variable because moving directly to the parent sounds unsafe.
                                            Type elem_type = std::move(*elem_type_targ);
                                            name.parts.at(1).template_args->args.front().var = std::move(elem_type);

                                            name.parts.emplace(name.parts.begin() + 2, "const_iterator");
                                        }

                                        // No `std::unordered_set` here, because it used the exact same iterators as `std::list::const_iterator` on MSVC STL.
                                    }
                                }
                            }
                        }
                    }
                }

                // libstdc++
                if (!already_normalized_iter && bool(flags & SimplifyFlags::bit_libstdcxx_normalize_iterators))
                {
                    if (name.parts.size() >= 2)
                    {
                        const std::string_view word1 = name.parts.at(1).AsSingleWord(SingleWordFlags::ignore_template_args);

                        // std::vector
                        if (
                            !already_normalized_iter &&
                            name.parts.at(0).AsSingleWord() == "__gnu_cxx" &&
                            word1 == "__normal_iterator" &&
                            name.parts.at(1).template_args &&
                            name.parts.at(1).template_args->args.size() == 2
                        )
                        {
                            auto targ0 = std::get_if<Type>(&name.parts.at(1).template_args->args.at(0).var);
                            auto targ1 = std::get_if<Type>(&name.parts.at(1).template_args->args.at(1).var);
                            if (
                                targ0 &&
                                targ1 &&
                                targ0->Is<Pointer>() &&
                                GetDerived().AsStdName(*targ1) == "vector" &&
                                targ1->simple_type.name.parts.back().template_args &&
                                targ1->simple_type.name.parts.back().template_args->args.size() >= 1
                            )
                            {

                                if (auto vector_targ0 = std::get_if<Type>(&targ1->simple_type.name.parts.back().template_args->args.at(0).var))
                                {
                                    auto targ0_without_ptr = *targ0;
                                    targ0_without_ptr.RemoveModifier().RemoveQualifiers(CvQualifiers::const_);
                                    if (*vector_targ0 == targ0_without_ptr)
                                    {
                                        // Success!
                                        already_normalized_iter = true;
                                        bool is_const = targ0->IsConst(1);
                                        QualifiedName container_name = std::move(targ1->simple_type.name);
                                        name.parts.erase(name.parts.begin(), name.parts.begin() + 2);
                                        name.parts.insert(name.parts.begin(), std::make_move_iterator(container_name.parts.begin()), std::make_move_iterator(container_name.parts.end()));
                                        name.parts.emplace(name.parts.begin() + std::ptrdiff_t(container_name.parts.size()), is_const ? "const_iterator" : "iterator");
                                    }
                                }
                            }
                        }

                        // The rest of the containers.
                        if (
                            !already_normalized_iter &&
                            name.parts.at(0).AsSingleWord() == "std" // No version namespace here, so it's easier to check manually, without using `GetDerived().AsStdName()`.
                        )
                        {
                            // std::deque
                            if (
                                !already_normalized_iter &&
                                word1 == "_Deque_iterator" &&
                                name.parts.at(1).template_args &&
                                name.parts.at(1).template_args->args.size() == 3
                            )
                            {
                                auto targ0 = std::get_if<Type>(&name.parts.at(1).template_args->args.at(0).var);
                                auto targ1 = std::get_if<Type>(&name.parts.at(1).template_args->args.at(1).var);
                                auto targ2 = std::get_if<Type>(&name.parts.at(1).template_args->args.at(2).var);
                                if (
                                    targ0 &&
                                    targ1 &&
                                    targ2 &&
                                    targ1->Is<Reference>() &&
                                    targ2->Is<Pointer>()
                                )
                                {
                                    // Somehow checking `targ2` (a pointer) feels a bit more reliable than checking `targ1` (a reference).
                                    bool is_const = targ2->IsConst(1);

                                    // Since the element type of `std::deque` can't be const, testing this way is fine.

                                    auto targ1_without_const_ref = *targ1;
                                    targ1_without_const_ref.RemoveModifier().RemoveQualifiers(CvQualifiers::const_);

                                    auto targ2_without_const_ptr = *targ2;
                                    targ2_without_const_ptr.RemoveModifier().RemoveQualifiers(CvQualifiers::const_);

                                    if (*targ0 == targ1_without_const_ref && *targ0 == targ2_without_const_ptr)
                                    {
                                        // Success!
                                        already_normalized_iter = true;
                                        name.parts.at(1).var = "deque";
                                        name.parts.at(1).template_args->args.resize(1);
                                        name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                                    }
                                }
                            }

                            // std::forward_list
                            if (!already_normalized_iter)
                            {
                                bool is_mut = word1 == "_Fwd_list_iterator";
                                bool is_const = !is_mut && word1 == "_Fwd_list_const_iterator";

                                if (
                                    (is_mut || is_const) &&
                                    name.parts.at(1).template_args &&
                                    name.parts.at(1).template_args->args.size() == 1
                                )
                                {
                                    // Success!
                                    already_normalized_iter = true;
                                    name.parts.at(1).var = "forward_list";
                                    name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                                }
                            }

                            // std::list
                            if (!already_normalized_iter)
                            {
                                bool is_mut = word1 == "_List_iterator";
                                bool is_const = !is_mut && word1 == "_List_const_iterator";

                                if (
                                    (is_mut || is_const) &&
                                    name.parts.at(1).template_args &&
                                    name.parts.at(1).template_args->args.size() == 1
                                )
                                {
                                    // Success!
                                    already_normalized_iter = true;
                                    name.parts.at(1).var = "list";
                                    name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                                }
                            }

                            // std::set, std::multiset, std::map, std::multimap
                            if (!already_normalized_iter)
                            {
                                bool is_mut = word1 == "_Rb_tree_iterator";
                                bool is_const = !is_mut && word1 == "_Rb_tree_const_iterator";

                                if (
                                    (is_mut || is_const) &&
                                    name.parts.at(1).template_args &&
                                    name.parts.at(1).template_args->args.size() == 1
                                )
                                {
                                    if (auto targ = std::get_if<Type>(&name.parts.at(1).template_args->args.at(0).var))
                                    {
                                        // This gets set if this is a map as opposed to a set, and receives the list of its arguments.
                                        TemplateArgumentList *map_targs_ptr = nullptr;
                                        if (
                                            targ->IsOnlyQualifiedName() &&
                                            targ->simple_type.name.parts.size() == 2 &&
                                            targ->simple_type.name.parts.at(0).AsSingleWord() == "std" && // No version namespace here, so it's easier to check manually, without using `GetDerived().AsStdName()`.
                                            targ->simple_type.name.parts.at(1).AsSingleWord(SingleWordFlags::ignore_template_args) == "pair" &&
                                            targ->simple_type.name.parts.at(1).template_args &&
                                            targ->simple_type.name.parts.at(1).template_args->args.size() == 2
                                        )
                                        {
                                            auto targ0 = std::get_if<Type>(&targ->simple_type.name.parts.at(1).template_args->args.at(0).var);
                                            auto targ1 = std::get_if<Type>(&targ->simple_type.name.parts.at(1).template_args->args.at(1).var);
                                            if (
                                                targ0 &&
                                                targ1 &&
                                                targ0->IsConst()
                                                // Not checking `!targ1->IsConst()` because apparently maps can have non-const values just fine. Tested on libstdc++, libc++, and MSVC STL.
                                            )
                                            {
                                                // Can't adjust `map_targs_ptr` here yet, because there are more conditions to check below.
                                                map_targs_ptr = &*targ->simple_type.name.parts.at(1).template_args;
                                            }
                                        }

                                        // Reject if this is a non-const iterator and at the same time not a map iterator.
                                        // This could never happen in my tests (because the set iterators are always const).
                                        if (is_const || map_targs_ptr)
                                        {
                                            // Success!
                                            already_normalized_iter = true;

                                            name.parts.at(1).var = map_targs_ptr ? "map" : "set";
                                            if (map_targs_ptr)
                                            {
                                                // We introduce a temporary variable to avoid directly moving the targs into their parent, which sounds unsafe.
                                                TemplateArgumentList map_targs = std::move(*map_targs_ptr);
                                                std::get<Type>(map_targs.args.at(0).var).RemoveQualifiers(CvQualifiers::const_);
                                                name.parts.at(1).template_args = std::move(map_targs);
                                            }
                                            name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                                        }
                                    }
                                }
                            }

                            // std::unordered_set, std::unordered_multiset, std::unordered_map, std::unordered_multimap
                            if (
                                !already_normalized_iter &&
                                name.parts.size() >= 3 &&
                                name.parts.at(1).AsSingleWord() == "__detail" // No version namespace here, so it's easier to check manually, without using `GetDerived().AsStdName()`.
                            )
                            {
                                const std::string_view word2 = name.parts.at(2).AsSingleWord(SingleWordFlags::ignore_template_args);

                                const bool is_mut = word2 == "_Node_iterator";
                                const bool is_const = !is_mut && word2 == "_Node_const_iterator";

                                if (
                                    (is_mut || is_const) &&
                                    name.parts.at(2).template_args &&
                                    name.parts.at(2).template_args->args.size() == 3
                                )
                                {
                                    auto targ0 = std::get_if<Type>(&name.parts.at(2).template_args->args.at(0).var);
                                    auto targ1 = std::get_if<PseudoExpr>(&name.parts.at(2).template_args->args.at(1).var);
                                    auto targ2 = std::get_if<PseudoExpr>(&name.parts.at(2).template_args->args.at(2).var);

                                    if (
                                        targ0 &&
                                        targ1 &&
                                        targ2 &&
                                        targ1->tokens.size() == 1 &&
                                        targ2->tokens.size() == 1
                                    )
                                    {
                                        auto targ1_token = std::get_if<SimpleType>(&targ1->tokens.front());
                                        auto targ2_token = std::get_if<SimpleType>(&targ2->tokens.front());

                                        if (
                                            targ1_token &&
                                            targ2_token &&
                                            targ2_token->AsSingleWord() == "false"
                                        )
                                        {
                                            bool is_set = targ1_token->AsSingleWord() == "true";
                                            bool is_map = !is_set && targ1_token->AsSingleWord() == "false";

                                            if (is_set || is_map)
                                            {
                                                TemplateArgumentList *map_targs_ptr = nullptr;
                                                if (
                                                    is_map &&
                                                    targ0->IsOnlyQualifiedName() &&
                                                    targ0->simple_type.name.parts.size() == 2 &&
                                                    targ0->simple_type.name.parts.at(0).AsSingleWord() == "std" && // No version namespace here, so it's easier to check manually, without using `GetDerived().AsStdName()`.
                                                    targ0->simple_type.name.parts.at(1).AsSingleWord(SingleWordFlags::ignore_template_args) == "pair" &&
                                                    targ0->simple_type.name.parts.at(1).template_args &&
                                                    targ0->simple_type.name.parts.at(1).template_args->args.size() == 2
                                                )
                                                {
                                                    auto pair_targ0 = std::get_if<Type>(&targ0->simple_type.name.parts.at(1).template_args->args.at(0).var);
                                                    auto pair_targ1 = std::get_if<Type>(&targ0->simple_type.name.parts.at(1).template_args->args.at(1).var);
                                                    if (
                                                        pair_targ0 &&
                                                        pair_targ1 &&
                                                        pair_targ0->IsConst()
                                                        // Not checking `!targ1->IsConst()` because apparently maps can have non-const values just fine. Tested on libstdc++, libc++, and MSVC STL.
                                                    )
                                                    {
                                                        // Can't adjust `map_targs_ptr` here yet, because there are more conditions to check below.
                                                        map_targs_ptr = &*targ0->simple_type.name.parts.at(1).template_args;
                                                    }
                                                }

                                                if (is_map == bool(map_targs_ptr))
                                                {
                                                    // Success!
                                                    already_normalized_iter = true;

                                                    name.parts.at(1).var = is_map ? "unordered_map" : "unordered_set";
                                                    if (is_map)
                                                    {
                                                        name.parts.at(1).template_args = std::move(*map_targs_ptr);
                                                        std::get<Type>(name.parts.at(1).template_args->args.at(0).var).RemoveQualifiers(CvQualifiers::const_);
                                                        name.parts.at(2).template_args.reset();
                                                    }
                                                    else
                                                    {
                                                        name.parts.at(1).template_args = std::move(name.parts.at(2).template_args);
                                                        name.parts.at(1).template_args->args.resize(1);
                                                        name.parts.at(2).template_args.reset();
                                                    }
                                                    name.parts.at(2).var = is_const ? "const_iterator" : "iterator";
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // libc++
                if (!already_normalized_iter && bool(flags & SimplifyFlags::bit_libcpp_normalize_iterators))
                {
                    // Here we don't use `GetDerived().AsStdName()` because we only need to support one specific spelling of the version namespace.

                    if (name.parts.size() >= 2 && name.parts.at(0).AsSingleWord() == "std")
                    {
                        const std::size_t part_index = GetDerived().IsLibcppVersionNamespace(name.parts.at(1).AsSingleWord()) ? 2 : 1;

                        if (
                            part_index < name.parts.size() &&
                            name.parts.at(part_index).IsSingleWord(SingleWordFlags::ignore_template_args) &&
                            name.parts.at(part_index).template_args
                        )
                        {
                            UnqualifiedName &part = name.parts.at(part_index);
                            const std::string_view word = part.AsSingleWord(SingleWordFlags::ignore_template_args);

                            auto CountsAsPtrdiffType = [](const Type &type) -> bool
                            {
                                std::string_view word = type.AsSingleWord();
                                // Could later make those checks platform-specific if needed.
                                return word == "long" || word == "long long";
                            };
                            auto CountsAsPtrdiffConstant = [](const PseudoExpr &expr, std::uint64_t value) -> bool
                            {
                                if (expr.tokens.size() != 1)
                                    return false;
                                auto number = std::get_if<NumericLiteral>(&expr.tokens.front());
                                if (!number)
                                    return false;
                                return number->ToInteger<decltype(value)>() == value;
                            };
                            auto IsLibcppStdNameIgnoringTemplateArgs = [&](const QualifiedName &name, std::string_view target) -> bool
                            {
                                // Not using `GetDerived().AsStdName()` because this only needs libc++ namespaces.
                                // We're also not using this lambda for the first check above, because here we don't allow more unqualified names after the `target`.
                                if (name.parts.size() < 2)
                                    return false;
                                if (name.parts.at(0).AsSingleWord() != "std")
                                    return false;
                                bool has_version_namespace = GetDerived().IsLibcppVersionNamespace(name.parts.at(1).AsSingleWord());
                                if (name.parts.size() != (has_version_namespace ? 3 : 2))
                                    return false;
                                return name.parts.back().AsSingleWord(SingleWordFlags::ignore_template_args) == target;
                            };

                            // std::vector
                            if (
                                !already_normalized_iter &&
                                word == "__wrap_iter" &&
                                part.template_args->args.size() == 1
                            )
                            {
                                if (
                                    auto type = std::get_if<Type>(&part.template_args->args.at(0).var);
                                    type &&
                                    type->Is<Pointer>()
                                )
                                {
                                    // Success!
                                    already_normalized_iter = true;

                                    const bool is_const = type->IsConst(1);
                                    part.var = "vector";
                                    name.parts.emplace(name.parts.begin() + std::ptrdiff_t(part_index) + 1, is_const ? "const_iterator" : "iterator");
                                    type->RemoveModifier().RemoveQualifiers(CvQualifiers::const_);
                                }
                            }

                            // std::deque
                            if (
                                !already_normalized_iter &&
                                word == "__deque_iterator" &&
                                part.template_args->args.size() == 6
                            )
                            {
                                auto targ0 = std::get_if<Type>(&part.template_args->args.at(0).var);
                                auto targ1 = std::get_if<Type>(&part.template_args->args.at(1).var);
                                auto targ2 = std::get_if<Type>(&part.template_args->args.at(2).var);
                                auto targ3 = std::get_if<Type>(&part.template_args->args.at(3).var);
                                auto targ4 = std::get_if<Type>(&part.template_args->args.at(4).var);
                                auto targ5 = std::get_if<PseudoExpr>(&part.template_args->args.at(5).var);
                                if (
                                    targ0 &&
                                    targ1 && targ1->Is<Pointer>() &&
                                    targ2 && targ2->Is<Reference>() &&
                                    targ3 && targ3->Is<Pointer>() && targ3->Is<Pointer>(1) && // `Is()` correctly handles the index being out of bounds by returning false.
                                    targ4 && CountsAsPtrdiffType(*targ4) &&
                                    targ5 && CountsAsPtrdiffConstant(*targ5, 1024)
                                )
                                {
                                    const bool is_const = targ1->IsConst(1);
                                    if (
                                        targ2->IsConst(1) == is_const &&
                                        targ3->IsConst(1) == is_const && targ3->IsConst(2) == is_const // Both pointer levels have the same constness.
                                    )
                                    {
                                        auto targ1_without_const_ptr = *targ1;
                                        auto targ2_without_const_ref = *targ2;
                                        auto targ3_without_double_const_ref = *targ3;
                                        targ1_without_const_ptr.RemoveModifier().RemoveQualifiers(CvQualifiers::const_);
                                        targ2_without_const_ref.RemoveModifier().RemoveQualifiers(CvQualifiers::const_);
                                        targ3_without_double_const_ref.RemoveModifier().RemoveModifier().RemoveQualifiers(CvQualifiers::const_);

                                        if (
                                            *targ0 == targ1_without_const_ptr &&
                                            *targ0 == targ2_without_const_ref &&
                                            *targ0 == targ3_without_double_const_ref
                                        )
                                        {
                                            // Success!
                                            already_normalized_iter = true;

                                            part.var = "deque";
                                            part.template_args->args.resize(1);
                                            name.parts.emplace(name.parts.begin() + std::ptrdiff_t(part_index) + 1, is_const ? "const_iterator" : "iterator");
                                        }
                                    }
                                }
                            }

                            // std::forward_list
                            if (
                                !already_normalized_iter &&
                                part.template_args->args.size() == 1
                            )
                            {
                                const bool is_mut = word == "__forward_list_iterator";
                                const bool is_const = !is_mut && word == "__forward_list_const_iterator";
                                if (is_mut || is_const)
                                {
                                    if (
                                        auto targ = std::get_if<Type>(&part.template_args->args.at(0).var);
                                        targ &&
                                        targ->Is<Pointer>() &&
                                        targ->GetQualifiers() == CvQualifiers{} &&
                                        targ->modifiers.size() == 1 &&
                                        targ->simple_type.IsOnlyQualifiedName() &&
                                        IsLibcppStdNameIgnoringTemplateArgs(targ->simple_type.name, "__forward_list_node") &&
                                        targ->simple_type.name.parts.back().template_args &&
                                        targ->simple_type.name.parts.back().template_args->args.size() == 2
                                    )
                                    {
                                        auto targ0 = std::get_if<Type>(&targ->simple_type.name.parts.back().template_args->args.at(0).var);
                                        auto targ1 = std::get_if<Type>(&targ->simple_type.name.parts.back().template_args->args.at(1).var);
                                        if (
                                            targ0 &&
                                            targ1 &&
                                            *targ1 == Type::FromSingleWord("void").AddModifier(Pointer{})
                                        )
                                        {
                                            // Success!
                                            already_normalized_iter = true;

                                            part.var = "forward_list";

                                            // Move into a temporary variable first. Moving directly to parent seems unsafe.
                                            Type elem_type = std::move(*targ0);
                                            part.template_args->args.resize(1);
                                            part.template_args->args.front().var = std::move(elem_type);

                                            name.parts.emplace(name.parts.begin() + std::ptrdiff_t(part_index) + 1, is_const ? "const_iterator" : "iterator");
                                        }
                                    }
                                }
                            }

                            // std::list
                            if (
                                !already_normalized_iter &&
                                part.template_args->args.size() == 2
                            )
                            {
                                const bool is_mut = word == "__list_iterator";
                                const bool is_const = !is_mut && word == "__list_const_iterator";
                                if (is_mut || is_const)
                                {
                                    auto targ0 = std::get_if<Type>(&part.template_args->args.at(0).var);
                                    auto targ1 = std::get_if<Type>(&part.template_args->args.at(1).var);
                                    if (
                                        targ0 &&
                                        targ1 &&
                                        *targ1 == Type::FromSingleWord("void").AddModifier(Pointer{})
                                    )
                                    {
                                        // Success!
                                        already_normalized_iter = true;

                                        part.var = "list";
                                        part.template_args->args.resize(1);
                                        name.parts.emplace(name.parts.begin() + std::ptrdiff_t(part_index) + 1, is_const ? "const_iterator" : "iterator");
                                    }
                                }
                            }

                            // Returns true if `part`if those template arguments look like those if a `std::set` iterator.
                            // This is in a lambda because we need to reuse it for `std::map` iterators.
                            auto IsSetIteratorTargs = [&CountsAsPtrdiffType, &IsLibcppStdNameIgnoringTemplateArgs](const std::optional<TemplateArgumentList> &targs) -> bool
                            {
                                if (
                                    targs &&
                                    targs->args.size() == 3
                                )
                                {
                                    auto targ0 = std::get_if<Type>(&targs->args.at(0).var);
                                    auto targ1 = std::get_if<Type>(&targs->args.at(1).var);
                                    auto targ2 = std::get_if<Type>(&targs->args.at(2).var);
                                    if (
                                        targ0 &&
                                        targ1 &&
                                        targ1->Is<Pointer>() &&
                                        targ1->GetQualifiers() == CvQualifiers{} &&
                                        targ1->modifiers.size() == 1 &&
                                        targ1->simple_type.IsOnlyQualifiedName() &&
                                        IsLibcppStdNameIgnoringTemplateArgs(targ1->simple_type.name, "__tree_node") &&
                                        targ1->simple_type.name.parts.back().template_args &&
                                        targ1->simple_type.name.parts.back().template_args->args.size() == 2 &&
                                        targ2 &&
                                        CountsAsPtrdiffType(*targ2)
                                    )
                                    {
                                        auto sub_targ0 = std::get_if<Type>(&targ1->simple_type.name.parts.back().template_args->args.at(0).var);
                                        auto sub_targ1 = std::get_if<Type>(&targ1->simple_type.name.parts.back().template_args->args.at(1).var);
                                        if (
                                            sub_targ0 &&
                                            sub_targ1 &&
                                            *sub_targ1 == Type::FromSingleWord("void").AddModifier(Pointer{}) &&
                                            // Check this last, because this is potentially more expensive for big types.
                                            *sub_targ0 == *targ0
                                        )
                                        {
                                            return true;
                                        }
                                    }
                                }

                                return false;
                            };

                            // std::set
                            if (
                                !already_normalized_iter &&
                                word == "__tree_const_iterator" &&
                                IsSetIteratorTargs(part.template_args)
                            )
                            {
                                // We already checked that that template argument is a type (in IsSetIteratorTargs()`), so we can `std::get()` directly here.
                                const Type &type = std::get<Type>(part.template_args->args.front().var);
                                // Make sure we exclude `std::map` iterator internals.
                                if (!(type.IsOnlyQualifiedName() && IsLibcppStdNameIgnoringTemplateArgs(type.simple_type.name, "__value_type")))
                                {
                                    // Success!
                                    already_normalized_iter = true;

                                    part.var = "set";
                                    part.template_args->args.resize(1);
                                    name.parts.emplace(name.parts.begin() + std::ptrdiff_t(part_index) + 1, "const_iterator");
                                }
                            }

                            // std::map
                            if (
                                !already_normalized_iter &&
                                part.template_args->args.size() == 1
                            )
                            {
                                const bool is_mut = word == "__map_iterator";
                                const bool is_const = !is_mut && word == "__map_const_iterator";
                                if (is_mut || is_const)
                                {
                                    if (
                                        auto targ = std::get_if<Type>(&part.template_args->args.front().var);
                                        targ &&
                                        targ->IsOnlyQualifiedName() &&
                                        IsLibcppStdNameIgnoringTemplateArgs(targ->simple_type.name, is_const ? "__tree_const_iterator" : "__tree_iterator") &&
                                        IsSetIteratorTargs(targ->simple_type.name.parts.back().template_args)
                                    )
                                    {
                                        if (
                                            auto sub_targ = std::get_if<Type>(&targ->simple_type.name.parts.back().template_args->args.front().var);
                                            sub_targ &&
                                            sub_targ->IsOnlyQualifiedName() &&
                                            IsLibcppStdNameIgnoringTemplateArgs(sub_targ->simple_type.name, "__value_type") &&
                                            sub_targ->simple_type.name.parts.back().template_args &&
                                            sub_targ->simple_type.name.parts.back().template_args->args.size() == 2 &&
                                            std::holds_alternative<Type>(sub_targ->simple_type.name.parts.back().template_args->args.at(0).var) &&
                                            std::holds_alternative<Type>(sub_targ->simple_type.name.parts.back().template_args->args.at(1).var)
                                        )
                                        {
                                            // Success!
                                            already_normalized_iter = true;

                                            part.var = "map";

                                            // Move into a temporary variable, because moving directly to the parent looks unsafe.
                                            TemplateArgumentList elem_types = std::move(*sub_targ->simple_type.name.parts.back().template_args);
                                            part.template_args = std::move(elem_types);

                                            name.parts.emplace(name.parts.begin() + std::ptrdiff_t(part_index) + 1, is_const ? "const_iterator" : "iterator");
                                        }
                                    }
                                }
                            }

                            // Returns true if `part`if those template arguments look like those if a `std::unordered_set` iterator.
                            // This is in a lambda because we need to reuse it for `std::unordered_map` iterators.
                            auto IsUnorderedSetIteratorTargs = [&IsLibcppStdNameIgnoringTemplateArgs](const std::optional<TemplateArgumentList> &targs) -> bool
                            {
                                if (
                                    targs &&
                                    targs->args.size() == 1
                                )
                                {
                                    auto targ0 = std::get_if<Type>(&targs->args.at(0).var);
                                    if (
                                        targ0 &&
                                        targ0->Is<Pointer>() &&
                                        targ0->GetQualifiers() == CvQualifiers{} &&
                                        targ0->modifiers.size() == 1 &&
                                        targ0->simple_type.IsOnlyQualifiedName() &&
                                        IsLibcppStdNameIgnoringTemplateArgs(targ0->simple_type.name, "__hash_node") &&
                                        targ0->simple_type.name.parts.back().template_args &&
                                        targ0->simple_type.name.parts.back().template_args->args.size() == 2
                                    )
                                    {
                                        auto sub_targ0 = std::get_if<Type>(&targ0->simple_type.name.parts.back().template_args->args.at(0).var);
                                        auto sub_targ1 = std::get_if<Type>(&targ0->simple_type.name.parts.back().template_args->args.at(1).var);
                                        if (
                                            sub_targ0 &&
                                            sub_targ1 &&
                                            *sub_targ1 == Type::FromSingleWord("void").AddModifier(Pointer{})
                                        )
                                        {
                                            return true;
                                        }
                                    }
                                }

                                return false;
                            };

                            // std::unordered_set
                            if (
                                !already_normalized_iter &&
                                word == "__hash_const_iterator" &&
                                IsUnorderedSetIteratorTargs(part.template_args)
                            )
                            {
                                // We already checked the preconditions for those two `std::get()` above in `IsUnorderedSetIteratorTargs()`, so calling `std::get()` here directly should be fine.
                                Type &type = std::get<Type>(std::get<Type>(part.template_args->args.front().var).simple_type.name.parts.back().template_args->args.front().var);
                                // Make sure we exclude `std::unordered_map` iterator internals.
                                if (!(type.IsOnlyQualifiedName() && IsLibcppStdNameIgnoringTemplateArgs(type.simple_type.name, "__hash_value_type")))
                                {
                                    // Success!
                                    already_normalized_iter = true;

                                    part.var = "unordered_set";

                                    // Move into a temporary variable, because moving directly into its parent seems unsafe.
                                    Type elem_type = std::move(type);
                                    part.template_args->args.at(0).var = std::move(elem_type);
                                    name.parts.emplace(name.parts.begin() + std::ptrdiff_t(part_index) + 1, "const_iterator");
                                }
                            }

                            // std::map
                            if (
                                !already_normalized_iter &&
                                part.template_args->args.size() == 1
                            )
                            {
                                const bool is_mut = word == "__hash_map_iterator";
                                const bool is_const = !is_mut && word == "__hash_map_const_iterator";
                                if (is_mut || is_const)
                                {
                                    if (
                                        auto targ = std::get_if<Type>(&part.template_args->args.front().var);
                                        targ &&
                                        targ->IsOnlyQualifiedName() &&
                                        IsLibcppStdNameIgnoringTemplateArgs(targ->simple_type.name, is_const ? "__hash_const_iterator" : "__hash_iterator") &&
                                        IsUnorderedSetIteratorTargs(targ->simple_type.name.parts.back().template_args)
                                    )
                                    {
                                        if (
                                            // Here we go TWO template arguments deep.
                                            // Without too many checks, since `IsUnorderedSetIteratorTargs()` should have already validated the structure here.
                                            auto sub_targ = std::get_if<Type>(&std::get<Type>(targ->simple_type.name.parts.back().template_args->args.front().var).simple_type.name.parts.back().template_args->args.front().var);
                                            sub_targ &&
                                            sub_targ->IsOnlyQualifiedName() &&
                                            IsLibcppStdNameIgnoringTemplateArgs(sub_targ->simple_type.name, "__hash_value_type") &&
                                            sub_targ->simple_type.name.parts.back().template_args &&
                                            sub_targ->simple_type.name.parts.back().template_args->args.size() == 2 &&
                                            std::holds_alternative<Type>(sub_targ->simple_type.name.parts.back().template_args->args.at(0).var) &&
                                            std::holds_alternative<Type>(sub_targ->simple_type.name.parts.back().template_args->args.at(1).var)
                                        )
                                        {
                                            // Success!
                                            already_normalized_iter = true;

                                            part.var = "unordered_map";

                                            // Move into a temporary variable, because moving directly to the parent looks unsafe.
                                            TemplateArgumentList elem_types = std::move(*sub_targ->simple_type.name.parts.back().template_args);
                                            part.template_args = std::move(elem_types);

                                            name.parts.emplace(name.parts.begin() + std::ptrdiff_t(part_index) + 1, is_const ? "const_iterator" : "iterator");
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Those need to be in a specific order, since we can only remove the last template argument at the every step:

            // Remove the allocator.
            if (bool(flags & SimplifyFlags::bit_common_remove_defarg_allocator))
            {
                std::size_t name_index = std::size_t(-1);

                const bool is_string_like        = GetDerived().IsStringLike(name, &name_index);
                const bool is_vector_like        = GetDerived().IsVectorLike(name, &name_index);
                const bool is_ordered_set_like   = GetDerived().IsOrderedSetLike(name, &name_index);
                const bool is_ordered_map_like   = GetDerived().IsOrderedMapLike(name, &name_index);
                const bool is_unordered_set_like = GetDerived().IsUnorderedSetLike(name, &name_index);
                const bool is_unordered_map_like = GetDerived().IsUnorderedMapLike(name, &name_index);

                if (
                    is_string_like ||
                    is_vector_like ||
                    is_ordered_set_like ||
                    is_ordered_map_like ||
                    is_unordered_set_like ||
                    is_unordered_map_like
                )
                {
                    std::size_t allocator_targ_pos = std::size_t(-2); // `-2` is used to not enter the `if` below if none of the branches is taken and the assert is disabled.
                    if (is_string_like)
                        allocator_targ_pos = 2; // After `std::char_traits`.
                    else if (is_vector_like)
                        allocator_targ_pos = 1; // As usual.
                    else if (is_ordered_set_like)
                        allocator_targ_pos = 2; // After comparator.
                    else if (is_ordered_map_like)
                        allocator_targ_pos = 3; // After mapped type and comparator.
                    else if (is_unordered_set_like)
                        allocator_targ_pos = 3; // After hash and equality.
                    else if (is_unordered_map_like)
                        allocator_targ_pos = 4; // After mapped type, hash and equality.
                    else
                        assert(false && "This should be unreachable.");

                    const bool allocator_is_map_like = is_ordered_map_like || is_unordered_map_like;

                    UnqualifiedName &name_part = name.parts.at(name_index);

                    // The allocator must be the last argument, otherwise removing it will mess up the order.
                    if (name_part.template_args && name_part.template_args->args.size() == allocator_targ_pos + 1)
                    {
                        if (auto allocator_type = std::get_if<Type>(&name_part.template_args->args.back().var))
                        {
                            if (allocator_is_map_like)
                            {
                                auto targ0 = std::get_if<Type>(&name_part.template_args->args.at(0).var);
                                auto targ1 = std::get_if<Type>(&name_part.template_args->args.at(1).var);
                                if (targ0 && targ1 && GetDerived().IsAllocatorForPair(*allocator_type, *targ0, *targ1))
                                    name_part.template_args->args.pop_back();
                            }
                            else
                            {
                                if (auto targ = std::get_if<Type>(&name_part.template_args->args.at(0).var); targ && GetDerived().IsAllocatorFor(*allocator_type, *targ))
                                    name_part.template_args->args.pop_back();
                            }
                        }
                    }
                }
            }

            // Remove char GetDerived(). Must be after removing the allocator.
            if (bool(flags & SimplifyFlags::bit_common_remove_defarg_char_traits))
            {
                std::size_t name_index = std::size_t(-1);

                if (GetDerived().HasCharTraits(name, &name_index))
                {
                    UnqualifiedName &name_part = name.parts.at(name_index);

                    if (name_part.template_args && name_part.template_args->args.size() == 2)
                    {
                        auto targ0 = std::get_if<Type>(&name_part.template_args->args.at(0).var);
                        auto targ1 = std::get_if<Type>(&name_part.template_args->args.at(1).var);
                        if (
                            targ0 && targ1 &&
                            GetDerived().IsCharTraits(*targ1) &&
                            targ1->simple_type.name.parts.back().template_args &&
                            targ1->simple_type.name.parts.back().template_args->args.size() == 1
                        )
                        {
                            if (auto allocator_targ = std::get_if<Type>(&targ1->simple_type.name.parts.back().template_args->args.front().var))
                            {
                                if (*allocator_targ == *targ0)
                                {
                                    // Success!
                                    name_part.template_args->args.pop_back();
                                }
                            }
                        }
                    }
                }
            }

            // Remove `std::less` and `std::equal_to`. Must be after removing the allocator.
            if (bool(flags & SimplifyFlags::bit_common_remove_defarg_comparator))
            {
                std::size_t name_index = std::size_t(-1);

                const bool is_ordered_set_like   = GetDerived().IsOrderedSetLike(name, &name_index);
                const bool is_ordered_map_like   = GetDerived().IsOrderedMapLike(name, &name_index);
                const bool is_unordered_set_like = GetDerived().IsUnorderedSetLike(name, &name_index);
                const bool is_unordered_map_like = GetDerived().IsUnorderedMapLike(name, &name_index);

                if (
                    is_ordered_set_like ||
                    is_ordered_map_like ||
                    is_unordered_set_like ||
                    is_unordered_map_like
                )
                {
                    std::size_t comparator_targ_pos = std::size_t(-2); // `-2` is used to not enter the `if` below if none of the branches is taken and the assert is disabled.
                    if (is_ordered_set_like)
                        comparator_targ_pos = 1; // Right after the element type.
                    else if (is_ordered_map_like)
                        comparator_targ_pos = 2; // After the mapped type.
                    else if (is_unordered_set_like)
                        comparator_targ_pos = 2; // After the hash functor.
                    else if (is_unordered_map_like)
                        comparator_targ_pos = 3; // After the mapped type and the hash.
                    else
                        assert(false && "This should be unreachable.");

                    const bool container_is_unordered = is_unordered_set_like || is_unordered_map_like;

                    UnqualifiedName &name_part = name.parts.at(name_index);

                    // The comparator must be the last argument, otherwise removing it will mess up the order.
                    if (name_part.template_args && name_part.template_args->args.size() == comparator_targ_pos + 1)
                    {
                        if (auto comparator_type = std::get_if<Type>(&name_part.template_args->args.back().var))
                        {
                            if (
                                (container_is_unordered ? GetDerived().IsEqualToComparator(*comparator_type) : GetDerived().IsLessComparator(*comparator_type)) &&
                                comparator_type->simple_type.name.parts.back().template_args &&
                                comparator_type->simple_type.name.parts.back().template_args->args.size() == 1
                            )
                            {
                                if (auto comparator_targ = std::get_if<Type>(&comparator_type->simple_type.name.parts.back().template_args->args.front().var))
                                {
                                    if (auto our_targ = std::get_if<Type>(&name_part.template_args->args.at(0).var))
                                    {
                                        if (*our_targ == *comparator_targ)
                                            name_part.template_args->args.pop_back();
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Remove `std::hash`. Must be after removing the comparator (and the allocator).
            if (bool(flags & SimplifyFlags::bit_common_remove_defarg_hash_functor))
            {
                std::size_t name_index = std::size_t(-1);

                const bool is_unordered_set_like = GetDerived().IsUnorderedSetLike(name, &name_index);
                const bool is_unordered_map_like = GetDerived().IsUnorderedMapLike(name, &name_index);

                if (
                    is_unordered_set_like ||
                    is_unordered_map_like
                )
                {
                    std::size_t hash_targ_pos = std::size_t(-2); // `-2` is used to not enter the `if` below if none of the branches is taken and the assert is disabled.
                    if (is_unordered_set_like)
                        hash_targ_pos = 1; // After the element type.
                    else if (is_unordered_map_like)
                        hash_targ_pos = 2; // After the mapped type..
                    else
                        assert(false && "This should be unreachable.");

                    UnqualifiedName &name_part = name.parts.at(name_index);

                    // The hash must be the last argument, otherwise removing it will mess up the order.
                    if (name_part.template_args && name_part.template_args->args.size() == hash_targ_pos + 1)
                    {
                        if (auto hash_type = std::get_if<Type>(&name_part.template_args->args.back().var))
                        {
                            if (
                                GetDerived().IsHashFunctor(*hash_type) &&
                                hash_type->simple_type.name.parts.back().template_args &&
                                hash_type->simple_type.name.parts.back().template_args->args.size() == 1
                            )
                            {
                                if (auto hash_targ = std::get_if<Type>(&hash_type->simple_type.name.parts.back().template_args->args.front().var))
                                {
                                    if (auto our_targ = std::get_if<Type>(&name_part.template_args->args.at(0).var))
                                    {
                                        if (*our_targ == *hash_targ)
                                            name_part.template_args->args.pop_back();
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Remove `std::default_delete`.
            if (bool(flags & SimplifyFlags::bit_common_remove_defarg_default_delete))
            {
                std::size_t name_index = std::size_t(-1);

                if (GetDerived().HasDefaultDelete(name, &name_index))
                {
                    UnqualifiedName &name_part = name.parts.at(name_index);

                    // The deleter must be the last argument, otherwise removing it will mess up the order.
                    if (name_part.template_args && name_part.template_args->args.size() == 2)
                    {
                        auto targ0 = std::get_if<Type>(&name_part.template_args->args.at(0).var);
                        auto targ1 = std::get_if<Type>(&name_part.template_args->args.at(1).var);

                        if (
                            targ0 &&
                            targ1 &&
                            GetDerived().IsDefaultDelete(*targ1, *targ0)
                        )
                        {
                            // Success!
                            name_part.template_args->args.pop_back();
                        }
                    }
                }
            }

            // Rewrite template specializations as typedefs.
            if (bool(flags & SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs))
            {
                std::size_t name_index = std::size_t(-1);
                std::string_view new_name_base_view;
                bool allow_all_char_types = false;

                if (GetDerived().SpecializationsHaveTypedefsForCharTypes(name, &name_index, &new_name_base_view, &allow_all_char_types))
                {
                    UnqualifiedName &part = name.parts.at(name_index);

                    // Need to copy this to avoid copying a part of string to itself, which trips ASAN and actually causes bugs on MSVC.
                    std::string new_name_base(new_name_base_view);

                    // A separate condition to avoid going into the `else` below if this fails.
                    if (part.template_args && part.template_args->args.size() == 1)
                    {
                        if (auto type = std::get_if<Type>(&part.template_args->args.front().var))
                        {
                            const std::string_view type_word = type->AsSingleWord();
                            if (type_word == "char")
                            {
                                part.template_args.reset();
                                part.var = std::move(new_name_base);
                            }
                            else if (type_word == "wchar_t")
                            {
                                part.template_args.reset();
                                part.var.emplace<std::string>("w") += new_name_base;
                            }
                            else if (allow_all_char_types)
                            {
                                if (type_word == "char8_t")
                                {
                                    part.template_args.reset();
                                    part.var.emplace<std::string>("u8") += new_name_base;
                                }
                                else if (type_word == "char16_t")
                                {
                                    part.template_args.reset();
                                    part.var.emplace<std::string>("u16") += new_name_base;
                                }
                                else if (type_word == "char32_t")
                                {
                                    part.template_args.reset();
                                    part.var.emplace<std::string>("u32") += new_name_base;
                                }
                            }
                        }
                    }
                }
            }

            // Remove numeric literal suffixes in certain known good cases.
            if (bool(flags & SimplifyFlags::bit_common_remove_numeric_literal_suffixes_from_known_good_template_params))
            {
                // `std::array` size.
                std::size_t index = std::size_t(-1);
                if (GetDerived().AsStdName(name, &index) == "array")
                {
                    UnqualifiedName &part = name.parts.at(index);
                    if (
                        part.template_args &&
                        part.template_args->args.size() == 2
                    )
                    {
                        if (
                            auto targ = std::get_if<PseudoExpr>(&part.template_args->args.at(1).var);
                            targ &&
                            targ->tokens.size() == 1
                        )
                        {
                            if (auto lit = std::get_if<NumericLiteral>(&targ->tokens.front()))
                            {
                                if (auto i = std::get_if<NumericLiteral::Integer>(&lit->var))
                                {
                                    if (std::holds_alternative<NumericLiteral::Integer::Suffix>(i->suffix))
                                        i->suffix = ""; // Success!
                                }
                            }
                        }
                    }
                }
            }
        }

        // Simplify cv-qualifiers according to the flags.
        // This is a low-level function, prefer `Simplify()`.
        CPPDECL_CONSTEXPR void SimplifyCvQualifiers(SimplifyFlags flags, CvQualifiers &quals)
        {
            // For now we don't remove `msvc_unaligned`, because that would actually lose useful information, I think? Unlike `__ptr32` and `__ptr64` which are useless most of the time.
            if (bool(flags & SimplifyFlags::bit_msvc_remove_ptr32_ptr64))
                quals &= ~(CvQualifiers::msvc_ptr32 | CvQualifiers::msvc_ptr64);
        }

        CPPDECL_CONSTEXPR void SimplifySimpleTypeNonRecursively(SimplifyFlags flags, SimpleType &simple_type)
        {
            if (bool(flags & SimplifyFlags::bit_common_remove_type_prefix))
                simple_type.prefix = SimpleTypePrefix{};

            // Mark `int` as not implied. This has to be checked before `bit_common_remove_redundant_signed`.
            if (bool(flags & SimplifyFlags::bit_common_add_implied_int))
                simple_type.flags &= ~SimpleTypeFlags::implied_int;

            // Here we don't remove `signed` if `int` is implied.
            if (bool(flags & SimplifyFlags::bit_common_remove_redundant_signed) && bool(simple_type.flags & SimpleTypeFlags::explicitly_signed) && !simple_type.IsNonRedundantlySigned() && !bool(simple_type.flags & SimpleTypeFlags::implied_int))
                simple_type.flags &= ~SimpleTypeFlags::explicitly_signed;

            if (bool(flags & SimplifyFlags::bit_common_remove_redundant_int) && bool(simple_type.flags & SimpleTypeFlags::redundant_int))
                simple_type.flags &= ~SimpleTypeFlags::redundant_int;
        }

        CPPDECL_CONSTEXPR void SimplifyNumericLiteral(SimplifyFlags flags, NumericLiteral &lit)
        {
            if (bool(flags & SimplifyFlags::bit_common_normalize_numbers))
            {
                std::string str = ToCode(lit, ToCodeFlags::weakly_canonical_language_agnostic); // Eh.
                std::string_view view = str;
                auto ret = ParseNumericLiteral(view);
                auto new_lit = std::get_if<std::optional<NumericLiteral>>(&ret);
                assert(new_lit && *new_lit && view.empty() && "Numeric literal simplification via roundtrip failed, unable to parse the resulting string.");
                if (new_lit && *new_lit && view.empty())
                {
                    assert((*new_lit)->IsFloatingPoint() == lit.IsFloatingPoint() && "Numeric literal simplification via roundtrip produced a different kind of literal (integral from floating-point, or vice versa).");
                    lit = std::move(**new_lit);
                    assert(ToCode(lit, {}) == str && ToCode(lit, ToCodeFlags::weakly_canonical_language_agnostic) == str && "Numeric literal simplification isn't stable on a roundtrip.");
                }
            }
        }
    };
    // A CRTP base that stacks multiple mixins.
    template <typename Derived, template <typename, typename> typename ...M>
    using SimplifyTraits = typename CombineMixins<Derived, BasicSimplifyTraits<Derived>, M...>::type;

    // The simple traits with no mixins.
    struct DefaultSimplifyTraits : BasicSimplifyTraits<DefaultSimplifyTraits> {};

    // This recursively calls the other `Simplify...()` functions.
    // The `target` is typically a `Type` or `Decl`.
    template <typename Traits = DefaultSimplifyTraits>
    CPPDECL_CONSTEXPR void Simplify(SimplifyFlags flags, auto &target, Traits &&traits = {})
    {
        if (traits.ShouldAct(flags))
        {
            target.template VisitEachComponent<QualifiedName, CvQualifiers, SimpleType, NumericLiteral>(
                {},
                Overload{
                    [&](QualifiedName &name){traits.SimplifyQualifiedNameNonRecursively(flags, name);},
                    [&](CvQualifiers &quals){traits.SimplifyCvQualifiers(flags, quals);},
                    [&](SimpleType &quals){traits.SimplifySimpleTypeNonRecursively(flags, quals);},
                    [&](NumericLiteral &lit){traits.SimplifyNumericLiteral(flags, lit);},
                }
            );
        }
    }
}
