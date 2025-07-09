#pragma once

#include "cppdecl/declarations/data.h"
#include "cppdecl/declarations/zip_visit.h"
#include "cppdecl/misc/platform.h"

namespace cppdecl::PlatformDependentTypedefs
{
    enum class Category
    {
        none, // Not a platform-dependent typedef.
        signed_, // A typedef for `long long` on Windows and `long` elsewhere.
        unsigned_, // A typedef for `unsigned long long` on Windows and `long` elsewhere.
    };

    [[nodiscard]] CPPDECL_CONSTEXPR Category ClassifyTypedef(const QualifiedName &name)
    {
        if (name.parts.size() > 2)
            return Category::none;

        // Allow both `std::blah` and `blah`.
        // Don't bother with the version namespaces, I don't think those types can have those?
        if (name.parts.size() == 2 && name.parts.front().AsSingleWord() != "std")
            return Category::none;

        #define  __adjusted_int64_t
        #define  __adjusted_intmax_t
        #define  __adjusted_intptr_t
        #define  __adjusted_int_fast64_t
        #define  __adjusted_int_least64_t
        #define  __adjusted_ptrdiff_t
        #define  __adjusted_size_t
        #define  __adjusted_uint64_t
        #define  __adjusted_uintmax_t
        #define  __adjusted_uintptr_t
        #define  __adjusted_uint_fast64_t
        #define  __adjusted_uint_least64_t

        std::string_view word = name.parts.back().AsSingleWord();

        if (
            word == "int64_t" ||
            word == "intmax_t" ||
            word == "intptr_t" ||
            word == "int_fast64_t" ||
            word == "int_least64_t" ||
            word == "ptrdiff_t"
        )
        {
            return Category::signed_;
        }

        if (
            word == "size_t" ||
            word == "uint64_t" ||
            word == "uintmax_t" ||
            word == "uintptr_t" ||
            word == "uint_fast64_t" ||
            word == "uint_least64_t"
        )
        {
            return Category::unsigned_;
        }

        return Category::none;
    }

    // `canonical` and `pretty` are any type from `data.h`, probably either `Type` or `Decl`.
    // We recursively traverse them simultaneously (if the hierarchy is the same),
    //   and if we find `unsigned long [long]`<->`std::size_t` (or another similar typedef, see `ClassifyTypedef()` above),
    // we replace the spelling in `canonical` with that typedef.
    template <typename T>
    CPPDECL_CONSTEXPR bool MirrorTypedefs(T &canonical, const T &pretty)
    {
        int num_replaced = 0;

        ZipVisitQualifiedNames(canonical, pretty, [&](SimpleType &canonical_type, const SimpleType &pretty_type)
        {
            if (!pretty_type.IsOnlyQualifiedName())
                return;

            Category cat = ClassifyTypedef(pretty_type.name);
            if (cat == Category::none)
                return;

            const bool is_unsigned = cat == Category::unsigned_;

            std::string_view word = canonical_type.AsSingleWord(SingleWordFlags::require_unsigned * is_unsigned);

            // We could accept only one of those depending on the platform, which we would pass via some parameter.
            if (word == "long" || word == "long long")
            {
                // Success!
                num_replaced++;
                canonical_type = pretty_type;
            }
            else if (canonical_type == pretty_type)
            {
                // Count this too, for the check below.
                num_replaced++;
            }
        });

        // Now count the typedefs in `pretty_name` and make sure we're replaced all of them.
        pretty.template VisitEachComponent<QualifiedName>([&](const QualifiedName &name)
        {
            if (ClassifyTypedef(name) != Category::none)
                num_replaced--;
        });

        return num_replaced == 0;
    }

    #error go test this
}
