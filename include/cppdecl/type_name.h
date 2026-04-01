#pragma once

#include "cppdecl/declarations/parse.h"
#include "cppdecl/declarations/simplify.h"
#include "cppdecl/declarations/to_string.h"
#include "cppdecl/misc/enum_flags.h"
#include "cppdecl/misc/platform.h"

#if CPPDECL_NEED_DEMANGLER
#include "cppdecl/misc/demangler.h"
#endif

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <string>
#include <typeindex>
#include <typeinfo> // IWYU pragma: keep, because we do actually use `typeid(T)` in this file.

namespace cppdecl
{
    enum class TypeNameFlags
    {
        // Switch to an alternative type name provider.
        // Uses `typeid(...).name()` at runtime and then demangles,
        //   as opposed to using `__PRETTY_FUNCTION__` or `__FUNCSIG__` at compile-time.
        use_typeid = 1 << 0,

        // Don't try to simplify the type name.
        no_simplify = 1 << 1,

        // Don't parse and then stringify the type name, instead return it as is.
        // Implies `no_simplify`.
        no_process = 1 << 2 | no_simplify,

        // Only makes sense in combination with `use_typeid`. Don't demangle the type name.
        // Implies `no_process`.
        no_demangle = 1 << 3 | no_process,

        // Delay any complex transformations until runtime.
        // This should help improve compilation times.
        no_constexpr = 1 << 4,
    };
    CPPDECL_FLAG_OPERATORS(TypeNameFlags)

    namespace detail::TypeName
    {
        template <typename T>
        [[nodiscard]] constexpr std::string_view RawPrettyFuncString()
        {
            #if defined(_MSC_VER) && !defined(__clang__) // Clang warns about `__FUNCSIG__`, so it's easier to just use `__PRETTY_FUNCTION__` for it.
            return __FUNCSIG__;
            #else
            return __PRETTY_FUNCTION__;
            #endif
        }

        struct TypeNameFormat
        {
            std::size_t junk_leading = 0;
            std::size_t junk_total = 0;
        };

        // Here `T` must always be `int`. Using the default template argument to delay the instantion until it's actually needed.
        template <typename T = int>
        constexpr TypeNameFormat type_name_format = []{
            TypeNameFormat ret;
            std::string_view sample = RawPrettyFuncString<T>();
            ret.junk_leading = sample.rfind("int");
            ret.junk_total = sample.size() - 3;
            return ret;
        }();

        template <typename T>
        static constexpr auto type_name_storage = []{
            // This is not at the global scope to make it lazy.
            static_assert(type_name_format<>.junk_leading != std::string_view::npos, "Unable to determine the type name format on this compiler.");

            std::array<char, RawPrettyFuncString<T>().size() - type_name_format<>.junk_total + 1> ret{};
            const char *in = RawPrettyFuncString<T>().data() + type_name_format<>.junk_leading;
            char *out = ret.data();
            std::size_t n = ret.size() - 1;
            // A loop instead of `std::copy_n()`, hopefully this is a bit faster.
            for (std::size_t i = 0; i < n; i++)
                out[i] = in[i];
            return ret;
        }();

        [[nodiscard]] CPPDECL_CONSTEXPR Type ParseTypeDynamic(std::string_view input)
        {
            const std::string_view original_input = input;
            auto result = (ParseType)(input);
            if (auto error = std::get_if<ParseError>(&result))
                throw std::runtime_error("cppdecl::TypeName(): The type name failed to parse. Try adding `TypeNameFlags::no_process` to work around this, and please report this as a bug. The type was: `" + std::string(input) + "`. Error at position " + NumberToString(input.data() - original_input.data()) + ": " + error->message);
            if (!input.empty())
                throw std::runtime_error("cppdecl::TypeName(): The type name didn't parse fully. Try adding `TypeNameFlags::no_process` to work around this, and please report this as a bug. The type was: `" + std::string(input) + "`. The unparsed suffix starts at position " + NumberToString(input.data() - original_input.data()) + ".");
            return std::move(std::get<Type>(result));
        }

        #if CPPDECL_IS_CONSTEXPR
        template <typename T, TypeNameFlags Flags, ToCodeFlags Flags_ToCode, SimplifyFlags Flags_Simplify>
        static constexpr auto type_name_storage_processed = []{
            // See `https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3032r2.html` for why we can't just create a constexpr variable here.
            auto MakeStr = []{
                std::string_view view = RawPrettyFuncString<T>();
                view = {view.data() + type_name_format<>.junk_leading, view.size() - type_name_format<>.junk_total};

                auto result = (ParseType)(view);
                if (std::holds_alternative<ParseError>(result))
                    throw "cppdecl::TypeName(): The type name failed to parse. Try adding `TypeNameFlags::no_process` to work around this, and please report this as a bug.";

                Type &type = std::get<Type>(result);

                if constexpr (!bool(Flags & TypeNameFlags::no_simplify))
                    (Simplify)(bool(Flags_Simplify) ? Flags_Simplify : SimplifyFlags::native_func_name_based_only, type);

                return (ToCode)(type, Flags_ToCode);
            };

            std::array<char, MakeStr().size() + 1> ret{};
            std::string str = MakeStr();
            const char *in = str.data();
            char *out = ret.data();
            std::size_t n = str.size();
            // A loop instead of `std::copy_n()`, hopefully this is a bit faster.
            for (std::size_t i = 0; i < n; i++)
                out[i] = in[i];
            return ret;
        }();
        #endif

        template <std::size_t N>
        [[nodiscard]] constexpr std::string_view StringViewFromArray(const std::array<char, N> &arr)
        {
            return {arr.data(), N - 1};
        }

        // This is a separate function because in C++20 we can't create `static` variables in constexpr functions.
        template <typename T, TypeNameFlags Flags, ToCodeFlags Flags_ToCode, SimplifyFlags Flags_Simplify>
        [[nodiscard]] const std::string &GetProcessedTypeNameNonConstexpr()
        {
            static const std::string ret = []{
                Type parsed_type = detail::TypeName::ParseTypeDynamic(detail::TypeName::StringViewFromArray(detail::TypeName::type_name_storage<T>));

                if constexpr (!bool(Flags & TypeNameFlags::no_simplify))
                    (Simplify)(bool(Flags_Simplify) ? Flags_Simplify : SimplifyFlags::native_func_name_based_only, parsed_type);

                return (ToCode)(parsed_type, Flags_ToCode);
            }();
            return ret;
        }
    }

    // Obtains the type name at runtime from `std::type_index`.
    // This is a runtime subset of `TypeName()` defined below, so if your type is fixed at compile-time, prefer that function.
    // `use_typeid` in `flags` is useless here, it is always implied.
    // If `flags_simplify` are zero, they default to `native`. To actually avoid simplification, add `no_simplify` to `flags`.
    [[nodiscard]] inline std::string TypeNameDynamic(std::type_index type, TypeNameFlags flags = {}, ToCodeFlags flags_to_code = {}, SimplifyFlags flags_simplify = {})
    {
        std::string ret = type.name();
        if (bool((flags & TypeNameFlags::no_demangle) == TypeNameFlags::no_demangle))
            return ret;

        #if CPPDECL_NEED_DEMANGLER
        ret = Demangler{}(ret.c_str());
        #endif
        if (bool((flags & TypeNameFlags::no_process) == TypeNameFlags::no_process))
            return ret;

        Type parsed_type = detail::TypeName::ParseTypeDynamic(ret);

        if (!bool(flags & TypeNameFlags::no_simplify))
            (Simplify)(bool(flags_simplify) ? flags_simplify : SimplifyFlags::native, parsed_type);

        return (ToCode)(parsed_type, flags_to_code);
    }

    namespace detail::TypeName
    {
        // We need a separate function instead of doing this directly in `TypeName()`,
        //   because that function is `constexpr` (because it also supports pretty-func-based mode), and those can't have local `static` variables pre-C++23.
        template <typename T, TypeNameFlags Flags, ToCodeFlags Flags_ToCode, SimplifyFlags Flags_Simplify>
        static const std::string &CachedDynamicName()
        {
            static const std::string ret = (TypeNameDynamic)(typeid(T), Flags, Flags_ToCode, Flags_Simplify);
            return ret;
        }
    }

    // The returned view is guaranteed to be null-terminated.
    // If `Flags_Simplify` are zero, they are replaced with `bool(Flags & use_typeid) ? native | native_func_name_based_only`.
    // To actually disable simplification, pass `no_simplify` to `Flags`.
    template <typename T, TypeNameFlags Flags = {}, ToCodeFlags Flags_ToCode = {}, SimplifyFlags Flags_Simplify = {}>
    [[nodiscard]] CPPDECL_CONSTEXPR std::string_view TypeName()
    {
        if constexpr (!bool(Flags & TypeNameFlags::use_typeid))
        {
            if constexpr (bool((Flags & TypeNameFlags::no_process) == TypeNameFlags::no_process))
            {
                return detail::TypeName::StringViewFromArray(detail::TypeName::type_name_storage<T>);
            }
            else
            {
                #if CPPDECL_IS_CONSTEXPR
                if constexpr (!bool(Flags & TypeNameFlags::no_constexpr))
                {
                    return detail::TypeName::StringViewFromArray(detail::TypeName::type_name_storage_processed<T, Flags, Flags_ToCode, Flags_Simplify>);
                }
                else
                #endif
                {
                    return detail::TypeName::GetProcessedTypeNameNonConstexpr<T, Flags, Flags_ToCode, Flags_Simplify>();
                }
            }
        }
        else
        {
            return detail::TypeName::CachedDynamicName<T, Flags, Flags_ToCode, Flags_Simplify>();
        }
    }
}
