#pragma once

#include "cppdecl/misc/string_helpers.h"

#include <cassert>
#include <string_view>

namespace cppdecl
{
    // Returns true if `input` looks like it contains unnamed types that are impossible to parse at the moment.
    // The reason why this exists is because Clang likes to dump unquoted parts into the type names,
    //   which are in general impossible to parse because of that.
    // We could parse those from other compilers, and from Clang with the assumption that parentheses are balanced, but right now we don't attempt to,
    //   except for Clang's `$_0`, which are valid identifiers (if we include `$` characters, which we do).
    [[nodiscard]] constexpr bool ContainsUnnamedTypes(std::string_view input)
    {
        #ifndef NDEBUG
        const char *error = nullptr;
        #endif
        bool ret = VisitEverythingButStringAndCharLiterals(
            input,
            [](std::string_view part) -> bool
            {
                for (std::string_view bad_string : {
                    // Note, `__cxa_demangle` and `c++filt` produce the same results, and both are simply marked as `typeid` here.
                    // The rows marked `GCC | llvm-cxxfilt` are the result of applying `llvm-cxxfilt` to the names produced specifically by GCC.
                    //   Clang produces other names for those, of the form `$_0` (and with increasing numbers), which we can parse just fine,
                    //   and which roundtrip through `llvm-cxxfilt` unchanged.
                    "{unnamed type#",    // GCC   | typeid                 | struct/class/union/enum
                    "{lambda(",          // GCC   | typeid                 | lambda
                    /*<unnamed struct>*/ // GCC   | __PRETTY_FUNCTION__    | struct/class/union/enum
                    "<lambda(",          // GCC   | __PRETTY_FUNCTION__    | lambda
                    "'unnamed'",         // GCC   | llvm-cxxfilt           | struct/class/union/enum
                    "'lambda'",          // GCC   | llvm-cxxfilt           | lambda
                    "$_",                // Clang | typeid                 | struct/class/union/enum
                    /* same as above */  // Clang | typeid                 | lambda
                    /*(struct at */      // Clang | __PRETTY_FUNCTION__    | struct/class/union/enum
                    "(lambda at ",       // Clang | __PRETTY_FUNCTION__    | lambda
                    "<unnamed-type-",    // MSVC  | __FUNCSIG__ and typeid | struct/class/union/enum
                    "<lambda_",          // MSVC  | __FUNCSIG__ and typeid | lambda
                })
                {
                    if (part.find(bad_string) != std::string_view::npos)
                        return true;
                }

                // GCC   | __PRETTY_FUNCTION__    | struct/class/union/enum
                if (ConsumePunctuation(part, "<unnamed "))
                {
                    return
                        ConsumePunctuation(part, "struct>") ||
                        ConsumePunctuation(part, "class>") ||
                        ConsumePunctuation(part, "union>") ||
                        ConsumePunctuation(part, "enum>");
                }

                // Clang | __PRETTY_FUNCTION__    | struct/class/union/enum
                if (ConsumePunctuation(part, "(unnamed "))
                {
                    return
                        (
                            ConsumePunctuation(part, "struct") ||
                            ConsumePunctuation(part, "class") ||
                            ConsumePunctuation(part, "union") ||
                            ConsumePunctuation(part, "enum")
                        ) &&
                        ConsumePunctuation(part, " at ");
                }

                return false;
            }
            #ifndef NDEBUG
            , &error
            #endif
        );
        #ifndef NDEBUG
        assert(!error);
        #endif
        return ret;
    }
}
