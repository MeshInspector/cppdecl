#pragma once

#include "cppdecl/misc/enum_flags.h"
#include "cppdecl/misc/indirect_optional.h"
#include "cppdecl/misc/overload.h"
#include "cppdecl/misc/platform.h"
#include "cppdecl/misc/string_helpers.h"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// This stores the parsed representation of C++ declarations and types.
// The important types are `cppdecl::Decl` and `cppdecl::Type`.
// See `<cppdecl/declarations/parse.h>` for parsing and `<cppdecl/declarations/to_string.h>` for converting back to strings.


#define CPPDECL_EQUALITY_DECLARE(...) CPPDECL_CONSTEXPR bool operator==(const __VA_ARGS__ &) const;
#define CPPDECL_EQUALITY_DEFINE(...) CPPDECL_CONSTEXPR bool __VA_ARGS__::operator==(const __VA_ARGS__ &) const = default;
// Bugged in MSVC, so we have to use non-member comparisons. I don't think there's any practical difference though.
// MSVC BUG: https://developercommunity.visualstudio.com/t/Cannot-default-friend-comparison-operato/10913839
// #define CPPDECL_EQUALITY_DECLARE(...) friend CPPDECL_CONSTEXPR bool operator==(const __VA_ARGS__ &, const __VA_ARGS__ &);
// #define CPPDECL_EQUALITY_DEFINE(...) CPPDECL_CONSTEXPR bool operator==(const __VA_ARGS__ &, const __VA_ARGS__ &) = default;

namespace cppdecl
{
    enum class VisitEachComponentFlags
    {
        // Don't visit into names that are not type names (declaration names, function parameter names, etc).
        // But we still recurse into them normally.
        no_visit_nontype_names = 1 << 0,
        // Like `no_visit_nontype_names`, but prevents recursing into non-type names.
        // The two flags together force make them be ignored entirely.
        no_recurse_into_nontype_names = 1 << 1,

        // Don't recurse into names (which themselves can contain nested qualified names, among other things).
        // This is primarily to avoid visiting template arguments.
        no_recurse_into_names = 1 << 2,

        // Primarily for internal use. When visiting a qualifier name, indicates to other flags that it's a nontype name.
        // Only pass this to `QualifiedName`.
        this_name_is_nontype = 1 << 3,
    };
    CPPDECL_FLAG_OPERATORS(VisitEachComponentFlags)

    enum class IsBuiltInTypeNameFlags
    {
        allow_void = 1 << 0,
        allow_integral = 1 << 1, // This excludes `bool`, which has a separate flag.
        allow_bool = 1 << 2,
        allow_floating_point = 1 << 3,

        allow_arithmetic = allow_integral | allow_bool | allow_floating_point,
        allow_all = allow_void | allow_arithmetic,
    };
    CPPDECL_FLAG_OPERATORS(IsBuiltInTypeNameFlags)


    // Cv-qualifiers, and/or `__restrict`.
    enum class CvQualifiers
    {
        const_ = 1 << 0,
        volatile_ = 1 << 1,
        restrict_ = 1 << 2,
        msvc_ptr32 = 1 << 3, // MSVC's `__ptr32` that appears in `typeid(...).name()` for 32-bit pointers, as in `int * __ptr32`.
        msvc_ptr64 = 1 << 4, // MSVC's `__ptr64` that appears in `typeid(...).name()` for 64-bit pointers, as in `int * __ptr64`.

        // MSVC's `__unaligned` that appears e.g. in `__unaligned int *`.
        // It's a bit weird in that it seems to be accepted (by Clang) whenever a cv-qualifier is accpeted, but doesn't appear in the resulting type name if it's not in the decl-specifier-seq.
        // E.g. `int *__unaligned` becomes `int *`, which is understandable, but `int *__unaligned *` also becomes `int **`, which is hella weird.
        // Instead of handling all that, we just treat it as a regular cv-qualifier.
        msvc_unaligned = 1 << 5,
    };
    CPPDECL_FLAG_OPERATORS(CvQualifiers)

    // The kind of reference, if any.
    enum class RefQualifier
    {
        none,
        lvalue,
        rvalue,
    };

    // Additional information about a type.
    enum class SimpleTypeFlags
    {
        unsigned_ = 1 << 0,

        // Explicitly `signed`. Mutually exclusive with `unsigned_`. This is usually redundant, unless this is a `char`.
        explicitly_signed = 1 << 1,

        // Explicitly `... int`, where the `int` is unnecessary. It's removed from the output and only the flag is kept.
        // E.g. `long int` and `int long`.
        // Note that we don't set this for `signed int` and `unsigned int` for sanity.
        redundant_int = 1 << 2,

        // This will also have the type name set to `"int"`. We do this when getting `unsigned` and `signed` without the `int`.
        // We don't do this for `long` and such. This is intentionally inconsistent, for sanity.
        // This is mutally exclusive with `redundant_int`.
        implied_int = 1 << 3,
    };
    CPPDECL_FLAG_OPERATORS(SimpleTypeFlags)

    // This covers elaborated type specifiers and some other things.
    enum class SimpleTypePrefix
    {
        none,

        // Elaborated type specifiers:
        struct_,
        class_,
        union_,
        enum_,

        // `typename`.
        // It's nice to have it here, because it's mutually exclusive with elaborated type specifiers anyway, and like them it can only appear before the type name.
        typename_,
    };

    // Those are the flags used by the various `IsSingleWord()` and `AsSingleWord()` functions below.
    enum class SingleWordFlags
    {
        // Allow template arguments. They will not be returned in the resulting string, or course.
        ignore_template_args = 1 << 0,
        // Allow type prefixes. They will not be returned in the resulting string, or course.
        ignore_type_prefixes = 1 << 1,
    };
    CPPDECL_FLAG_OPERATORS(SingleWordFlags)

    struct Attribute;
    struct NumericLiteral;
    struct PseudoExpr;
    struct PseudoExprList;
    struct PunctuationToken;
    struct QualifiedName;
    struct SimpleType;
    struct StringOrCharLiteral;
    struct TemplateArgumentList;
    struct Type;

    template <typename T>
    concept VisitableComponentType =
        std::same_as<T, QualifiedName> ||
        std::same_as<T, CvQualifiers> ||
        std::same_as<T, SimpleType> ||
        std::same_as<T, PunctuationToken> ||
        std::same_as<T, NumericLiteral> ||
        std::same_as<T, StringOrCharLiteral> ||
        std::same_as<T, PseudoExprList> ||
        std::same_as<T, TemplateArgumentList> ||
        std::same_as<T, Attribute>;

    struct TemplateArgument;

    template <typename T>
    concept TemplateArgumentType = std::same_as<T, Type> || std::same_as<T, PseudoExpr>; // Sync with `TemplateArgument::Variant`.

    struct TemplateArgumentList
    {
        std::vector<TemplateArgument> args;

        CPPDECL_EQUALITY_DECLARE(TemplateArgumentList)

        // Checks if this argument list matches `values...`, where each is either a `Type` or a `PseudoExpr`.
        // Both the size and each element must match exactly.
        [[nodiscard]] CPPDECL_CONSTEXPR bool Matches(const TemplateArgumentType auto &... values) const;

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<TemplateArgumentList &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    struct UnqualifiedName;

    // A qualified name.
    struct QualifiedName
    {
        std::vector<UnqualifiedName> parts;
        bool force_global_scope = false; // True if this has a leading `::`.

        // This accepts only single-word type names without `::` or any other punctuation. Accepts `long long` and `long double`, `double long`, but rejects signedness and such.
        [[nodiscard]] static CPPDECL_CONSTEXPR QualifiedName FromSingleWord(std::string part);
        [[nodiscard]] static CPPDECL_CONSTEXPR QualifiedName FromSinglePart(UnqualifiedName part);

        // Inserts an unqualified name part at the end of `parts`.
        template <typename P>               CPPDECL_CONSTEXPR QualifiedName & AddPart(P &&part) &;
        template <typename P> [[nodiscard]] CPPDECL_CONSTEXPR QualifiedName &&AddPart(P &&part) &&;
        // Inserts an unqualified name part at the specified position into `parts`.
        template <typename P>               CPPDECL_CONSTEXPR QualifiedName & AddPart(std::size_t i, P &&part) &;
        template <typename P> [[nodiscard]] CPPDECL_CONSTEXPR QualifiedName &&AddPart(std::size_t i, P &&part) &&;

        // Adds a template argument (either a `Type` or a `PseudoExpr`) to the last part. If it doesn't have a template argument list yet, creates a list for it.
        template <typename A>               CPPDECL_CONSTEXPR QualifiedName & AddTemplateArgument(A &&arg) &;
        template <typename A> [[nodiscard]] CPPDECL_CONSTEXPR QualifiedName &&AddTemplateArgument(A &&arg) &&;

        CPPDECL_EQUALITY_DECLARE(QualifiedName)

        enum class EqualsFlags
        {
            // If the `target` has no template arguments on the LAST part, but we do have them, ignore the template arguments in the comparison of that part.
            allow_missing_final_template_args_in_target = 1 << 0,

            // Allow the `target` to be a prefix of this name (have less unqualified parts).
            // If `allow_missing_final_template_args_in_target` is also specified, then it still applies to the last part in `target`, even though it's no longer the last in self.
            allow_less_parts_in_target = 1 << 1,
        };
        CPPDECL_FLAG_OPERATORS_IN_CLASS(EqualsFlags)

        // Extended equality comparison. Not necessarily symmetric, depending on the flags.
        [[nodiscard]] CPPDECL_CONSTEXPR bool Equals(const QualifiedName &target, EqualsFlags flags) const;

        // Returns true if this is an invalid empty name.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsEmpty() const
        {
            assert(parts.empty() <= !force_global_scope);
            return !force_global_scope && parts.empty();
        }

        // Returns true if this name has at least one `::` in it.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsQualified() const;

        [[nodiscard]] CPPDECL_CONSTEXPR bool LastComponentIsNormalString() const;

        // Returns true if `parts` isn't empty, and the last component is a regular string.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsConversionOperatorName() const;

        [[nodiscard]] CPPDECL_CONSTEXPR bool IsDestructorName() const;

        // This can have false negatives, because e.g. `A::B` can be a constructor if you do `using A = B;`.
        // So this merely checks that the two last parts are the same string, and the secibd part has no template arguments (arguments on
        //   the first part are ignored).
        [[nodiscard]] CPPDECL_CONSTEXPR bool CertainlyIsQualifiedConstructorName() const;


        enum class EmptyReturnType
        {
            no,
            yes,
            maybe_unqual_constructor, // Just a lone unqualified name, that doesn't look like a type keyword.
            maybe_qual_constructor_using_typedef, // `A::B`, perhaps `A` is a typedef for `B` and this is a constructor.
        };

        [[nodiscard]] CPPDECL_CONSTEXPR EmptyReturnType IsFunctionNameRequiringEmptyReturnType() const;

        // If this name is a single word, returns true.
        // This can return `long long`, `long double`, etc. But `unsigned` and such shouldn't be here, they are in `SimpleTypeFlags`, and having non-zero flags makes this return false.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsSingleWord(SingleWordFlags flags = {}) const;
        // If this name is a single word, returns that word. Otherwise returns empty.
        [[nodiscard]] CPPDECL_CONSTEXPR std::string_view AsSingleWord(SingleWordFlags flags = {}) const;

        // If there's only one part and no `::` forcing the global scope,
        //   calls the same method on that (see `UnqualifiedName::IsBuiltInTypeName()` for details).
        // Otherwise returns false.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsBuiltInTypeName(IsBuiltInTypeNameFlags flags = IsBuiltInTypeNameFlags::allow_all) const;

        // Visit this instance, and all instances of any of `C...` nested in it. `func` is `(auto &name) -> void`.
        // Note! We can have other names nested in this, so you can't just call the function on it directly.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<QualifiedName &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    struct AttributeList
    {
        // Those are not deduplicated at the moment.
        std::vector<Attribute> attrs;

        CPPDECL_EQUALITY_DECLARE(AttributeList)

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<AttributeList &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    // A type, maybe cv-qualified, but without any pointer qualifiers and such.
    // This also corresponds to the decl-specifier-seq, aka the common part of the type shared between all variables in a declaration.
    struct SimpleType
    {
        AttributeList attrs;

        CvQualifiers quals{};
        SimpleTypeFlags flags{};
        SimpleTypePrefix prefix{};

        // The type name. Never includes `signed` or `unsigned`, that's in `flags`.
        QualifiedName name;

        // When adding new fields here, don't forget to update `Equals()`!

        // This accepts only single-word type names without `::` or any other punctuation. Accepts `long long` and `long double`, `double long`, but rejects signedness and such.
        [[nodiscard]] static CPPDECL_CONSTEXPR SimpleType FromSingleWord(std::string part);
        [[nodiscard]] static CPPDECL_CONSTEXPR SimpleType FromUnqualifiedName(UnqualifiedName part);
        [[nodiscard]] static CPPDECL_CONSTEXPR SimpleType FromQualifiedName(QualifiedName name);

        CPPDECL_EQUALITY_DECLARE(SimpleType)

        enum class EqualsFlags
        {
            as_if_target_is_const = 1 << 0,
        };
        CPPDECL_FLAG_OPERATORS_IN_CLASS(EqualsFlags)

        [[nodiscard]] CPPDECL_CONSTEXPR bool Equals(const SimpleType &target, EqualsFlags eq_flags) const
        {
            CvQualifiers target_quals_fixed = target.quals;
            if (bool(eq_flags & EqualsFlags::as_if_target_is_const))
                target_quals_fixed |= CvQualifiers::const_;

            if (quals != target_quals_fixed)
                return false;

            // Compare the remaining members.
            return
                attrs == target.attrs &&
                // Skipping `quals`.
                flags == target.flags &&
                prefix == target.prefix &&
                name == target.name;
        }

        // Returns true if this is an invalid empty type.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsEmpty() const
        {
            assert((!name.IsEmpty() || flags == SimpleTypeFlags{}) && "An empty type should have no flags.");
            assert((!name.IsEmpty() || prefix == SimpleTypePrefix{}) && "An empty type should have no prefix.");
            return name.IsEmpty();
        }
        // This version doesn't assert. Only for internal use.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsEmptyUnsafe() const
        {
            return name.IsEmpty();
        }

        // Returns true if there are no flags, cv-qualifiers or prefixes, only the name.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsOnlyQualifiedName(SingleWordFlags word_flags = {}) const
        {
            return
                quals == CvQualifiers{} &&
                (bool(word_flags & SingleWordFlags::ignore_type_prefixes) || prefix == SimpleTypePrefix{}) &&
                (flags & ~SimpleTypeFlags::implied_int) == SimpleTypeFlags{};
        }

        // If there are no flags, no qualifiers, no modifiers, and the type name is a unqualified word, returns true.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsSingleWord(SingleWordFlags word_flags = {}) const
        {
            return IsOnlyQualifiedName(word_flags) && name.IsSingleWord(word_flags);
        }

        // If there are no flags, no qualifiers, no modifiers, and the type name is a unqualified word, returns the type name. Otherwise returns empty.
        [[nodiscard]] CPPDECL_CONSTEXPR std::string_view AsSingleWord(SingleWordFlags word_flags = {}) const
        {
            // Ingoring `prefix` here would porbably be convenient in some cases. But confusing in others, and inconsistent. So we don't do it.
            if (IsSingleWord(word_flags))
                return name.AsSingleWord(word_flags);
            else
                return {};
        }

        // Returns true if this type is explicitly `signed`, and this `signed` actually has an effect (as in `signed char`).
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsNonRedundantlySigned() const
        {
            return bool(flags & SimpleTypeFlags::explicitly_signed) && name.AsSingleWord() == "char";
        }

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
        {
            attrs.VisitEachComponent<C...>(flags, func);

            if constexpr ((std::same_as<C, CvQualifiers> || ...))
                func(quals);

            name.VisitEachComponent<C...>(flags, func);

            // Using postorder here for consistency with `QualifierName`, which needs to use post-order for simplification reasons.
            if constexpr ((std::same_as<C, SimpleType> || ...))
                func(*this);
        }
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<SimpleType &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    struct TypeModifier;

    // A full type.
    struct Type
    {
        SimpleType simple_type;
        // The first modifier is the top-level one, it's the closest one to the variable name in the declaration.
        std::vector<TypeModifier> modifiers;

        // When adding new fields here, don't forget to update `Equals()`!

        // This accepts only single-word type names without `::` or any other punctuation. Accepts `long long` and `long double`, `double long`, but rejects signedness and such.
        [[nodiscard]] static CPPDECL_CONSTEXPR Type FromSingleWord(std::string part);
        [[nodiscard]] static CPPDECL_CONSTEXPR Type FromUnqualifiedName(UnqualifiedName part);
        [[nodiscard]] static CPPDECL_CONSTEXPR Type FromQualifiedName(QualifiedName name);
        [[nodiscard]] static CPPDECL_CONSTEXPR Type FromSimpleType(SimpleType simple_type);

        CPPDECL_EQUALITY_DECLARE(Type)

        enum class EqualsFlags
        {
            as_if_target_is_const = 1 << 0,
        };
        CPPDECL_FLAG_OPERATORS_IN_CLASS(EqualsFlags)

        CPPDECL_CONSTEXPR bool Equals(const Type &target, EqualsFlags eq_flags) const;

        // Returns true if this is an invalid empty type.
        // While normally an empty `simple_type` implies empty `modifiers`, it's not always the case.
        // Constructors, destructors and conversion operators will have an empty `simple_type` and normally one modifier.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsEmpty() const
        {
            return simple_type.IsEmpty() && modifiers.empty();
        }
        // This version doesn't assert. Only for internal use.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsEmptyUnsafe() const
        {
            return simple_type.IsEmptyUnsafe() && modifiers.empty();
        }

        // Returns true if there are no flags, cv-qualifiers or modifiers, only the name.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsOnlyQualifiedName(SingleWordFlags flags = {}) const
        {
            return modifiers.empty() && simple_type.IsOnlyQualifiedName(flags);
        }

        // Like `IsOnlyQualifiedName()`, but additionally verifies that the name is a single word.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsSingleWord(SingleWordFlags flags = {}) const
        {
            return modifiers.empty() && simple_type.IsOnlyQualifiedName(flags);
        }

        // If `IsSingleWord()` is satisfied, returns the type name word. Otherwise empty.
        [[nodiscard]] CPPDECL_CONSTEXPR std::string_view AsSingleWord(SingleWordFlags flags = {}) const
        {
            if (IsSingleWord(flags))
                return simple_type.AsSingleWord(flags);
            else
                return {};
        }

        // Check the modifier, top-level by default. Returns false on mismatch or if `i` is out of range.
        template <typename T> [[nodiscard]] CPPDECL_CONSTEXPR bool Is(std::size_t i = 0) const {return bool(As<T>(i));}
        // Returns the modifier, top-level by default, if you guess the type correctly, or null otherwise (including if `i` is out of range).
        template <typename T> [[nodiscard]] CPPDECL_CONSTEXPR       T *As(std::size_t i = 0);
        template <typename T> [[nodiscard]] CPPDECL_CONSTEXPR const T *As(std::size_t i = 0) const;

        // Returns true if this is const-qualified (at the top level by default, if `i == 0`).
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsConst(std::size_t i = 0) const;
        // Returns true if this is const-qualified or a reference (at the top level by default, if `i == 0`).
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsConstOrReference(std::size_t i = 0) const;

        // Returns the qualifiers of the `i`th modifier in `modifiers`, or those of `simple_type` if `i == modifiers.size()`.
        // `i == 0` effectively returns the top-level qualifiers.
        // If the `i`th modifier doesn't support qualifiers, returns zero.
        [[nodiscard]] CPPDECL_CONSTEXPR CvQualifiers GetQualifiers(std::size_t i = 0) const;
        // Same but mutable, null if the `i`th modifier can't have qualifiers.
        [[nodiscard]] CPPDECL_CONSTEXPR CvQualifiers *GetQualifiersMut(std::size_t i = 0);

        // Inserts a top-level modifier. That is, at the beginning of the `modifiers` vector.
        template <typename M>               CPPDECL_CONSTEXPR Type & AddModifier(M &&mod, std::size_t i = 0) &;
        template <typename M> [[nodiscard]] CPPDECL_CONSTEXPR Type &&AddModifier(M &&mod, std::size_t i = 0) &&;

                      CPPDECL_CONSTEXPR Type & RemoveModifier(std::size_t i = 0) &;
        [[nodiscard]] CPPDECL_CONSTEXPR Type &&RemoveModifier(std::size_t i = 0) &&;

        // Adds new cv-qualifiers. By default (if `i == 0`) acts on the top-level.
        // Asserts if the `i`th modifier doesn't support qualifiers (and `qual` isn't empty).
        CPPDECL_CONSTEXPR Type &AddQualifiers(CvQualifiers qual, std::size_t i = 0) &
        {
            if (qual == CvQualifiers{})
                return *this; // Don't fail if `quals` is empty, even if this modifier doesn't support qualifiers,
            auto ret = GetQualifiersMut(i);
            assert(ret && "This modifier doesn't support cv-qualifiers.");
            if (ret)
                *ret |= qual;
            return *this;
        }
        [[nodiscard]] CPPDECL_CONSTEXPR Type &&AddQualifiers(CvQualifiers qual, std::size_t i = 0) &&
        {
            AddQualifiers(qual, i);
            return std::move(*this);
        }

        // Removes cv-qualifiers to the top-level modifier if any (does nothing if it can't have cv-qualifiers), or from the `simple_type` otherwise.
        CPPDECL_CONSTEXPR Type &RemoveQualifiers(CvQualifiers qual, std::size_t i = 0) &
        {
            auto ret = GetQualifiersMut(i);
            if (ret) // We silently do nothing if this is false, unlike in `AddQualifiers()`.
                *ret &= ~qual;
            return *this;
        }
        [[nodiscard]] CPPDECL_CONSTEXPR Type &&RemoveQualifiers(CvQualifiers qual, std::size_t i = 0) &&
        {
            RemoveQualifiers(qual, i);
            return std::move(*this);
        }


        // Asserts that `this->simple_type` is empty. Replaces it with the one from `other`.
        // Appends all modifiers from `other` to this.
        CPPDECL_CONSTEXPR void AppendType(Type other);

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<Type &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    // Represents `operator@`.
    struct OverloadedOperator
    {
        // The operator being overloaded.
        std::string token;

        CPPDECL_EQUALITY_DECLARE(OverloadedOperator)

        // Visit all instances of any of `C...` nested in this. (None for this type.) `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {(void)flags; (void)func;}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {(void)flags; (void)func;}
    };

    // Represents `operator T`.
    struct ConversionOperator
    {
        Type target_type;

        CPPDECL_EQUALITY_DECLARE(ConversionOperator)

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {target_type.VisitEachComponent<C...>(flags, func);}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {target_type.VisitEachComponent<C...>(flags, func);}
    };

    // Represents `operator""_blah`.
    struct UserDefinedLiteral
    {
        std::string suffix;

        // Do we have a space between `""` and `_blah`?
        // This is deprecated.
        bool space_before_suffix = false;

        CPPDECL_EQUALITY_DECLARE(UserDefinedLiteral)

        // Visit all instances of any of `C...` nested in this. (None for this type.) `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {(void)flags; (void)func;}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {(void)flags; (void)func;}
    };

    // A destructor name of the form `~Blah`.
    struct DestructorName
    {
        SimpleType simple_type;

        CPPDECL_EQUALITY_DECLARE(DestructorName)

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {simple_type.VisitEachComponent<C...>(flags, func);}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {simple_type.VisitEachComponent<C...>(flags, func);}
    };

    // An unqualified name, possibly with template arguments.
    struct UnqualifiedName
    {
        using Variant = std::variant<std::string, OverloadedOperator, ConversionOperator, UserDefinedLiteral, DestructorName>;

        Variant var;

        // This is optional to distinguish an empty list from no list.
        // Always empty if `var` holds a `DestructorName`, because in that case any template arguments
        //   are a stored in the destructor's target type.
        std::optional<TemplateArgumentList> template_args;

        CPPDECL_EQUALITY_DECLARE(UnqualifiedName)

        enum class EqualsFlags
        {
            // If the `target` has no template arguments but we do have them, ignore the template arguments in the comparison.
            allow_missing_template_args_in_target = 1 << 0,
        };
        CPPDECL_FLAG_OPERATORS_IN_CLASS(EqualsFlags)

        // Extended equality comparison. Not necessarily symmetric, depending on the flags.
        [[nodiscard]] CPPDECL_CONSTEXPR bool Equals(const UnqualifiedName &target, EqualsFlags flags) const;

        // Returns true if `var` holds a `std::string` and `template_args` is empty.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsSingleWord(SingleWordFlags flags = {}) const;

        // If `var` holds a `std::string` and `template_args` is empty, returns the string in `var`. Otherwise returns empty.
        [[nodiscard]] CPPDECL_CONSTEXPR std::string_view AsSingleWord(SingleWordFlags flags = {}) const;

        // Checks if we have template argumens that exactly match `values...`, where each is either a `Type` or a `PseudoExpr`.
        // Both the size and each element must match exactly.
        [[nodiscard]] CPPDECL_CONSTEXPR bool TemplateArgsMatch(const TemplateArgumentType auto &... values) const
        {
            return template_args && template_args->Matches(values...);
        }

        // Whether `var` holds a `std::string`, with a built-in type name.
        // Note that we return true for `long long`, `long double`, and `double long`.
        // But signedness and constness isn't handled here, for that we have `SimpleTypeFlags`.
        // We don't really want to permit the `double long` spelling, and our parser shouldn't emit it, but keeping it here just in case
        //   the user manually sets it, or something?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsBuiltInTypeName(IsBuiltInTypeNameFlags flags = IsBuiltInTypeNameFlags::allow_all) const;

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<UnqualifiedName &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    // Things for non-type template arguments: [

    // Some punctuation token.
    struct PunctuationToken
    {
        std::string value;

        CPPDECL_EQUALITY_DECLARE(PunctuationToken)

        // Visit all instances of any of `C...` nested in this. (None for this type.) `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<PunctuationToken &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    // An integral or floating-point literal.
    struct NumericLiteral
    {
        struct Integer
        {
            enum class Base
            {
                decimal,
                binary,
                octal,
                hex,
            };
            Base base = Base::decimal;

            // This is parsed as is, and can contain `'`s and leading zeroes (minus one zero for octal).
            // For octal numbers starting with `0'...`, this will even begin with the `'`.
            // If `base != decimal`, this will be in the respective base rather than decimal.
            std::string value;

            // Either an entire standard suffix, or a part of one (to which you can add `u`/`U` prefix or suffix).
            enum class SignedSuffix
            {
                none, // No suffix or just `u`.
                l, // `long`
                ll, // `long long`
                z, // signed `size_t` (aka `ptrdiff_t`)
            };

            struct Suffix
            {
                // At least one of the two will be set in the parser output.
                // It's recommended to not use this struct if both are false. Instead store an empty string in `suffix`.

                SignedSuffix signed_part{};
                bool is_unsigned = false; // Does the suffix include letter `u`?

                CPPDECL_EQUALITY_DECLARE(Suffix)
            };
            // Either one of the built-in literal suffixes, or a custom string.
            // No suffix is stored as an empty string. This also helps us work around the Clang quirk with default member initializers in the first member of a variant.
            std::variant<std::string, Suffix> suffix;

            CPPDECL_EQUALITY_DECLARE(Integer)
        };
        struct FloatingPoint
        {
            enum class Base
            {
                decimal,
                hex,
            };
            Base base = Base::decimal;

            // Those are parsed as is, and can contain `'`s and leading zeroes.
            // If `base != decimal`, those will be in the respective base rather than decimal (except for `value_exp`, which is always decimal).
            //
            // At least `value_int` must be non-empty or `value_frac` must be non-null.
            // If `value_frac` is null and `value_exp` is empty, the number is kinda legal, but it will roundtrip to an integer. The parser will not produce those.

            std::string value_int; // The integral part, or empty if none.
            std::optional<std::string> value_frac; // The fractional part. Null if no decimal point. Non-null but empty if there is the point with no digits after it.
            std::string value_exp; // The exponent part, if any. This can optionall begin with a `+` or `-` sign.

            enum class Suffix
            {
                f,
                l,
                f16,
                f32,
                f64,
                f128,
                bf16,
            };
            // Either one of the built-in literal suffixes, or a custom string.
            // No suffix is stored as an empty string, for consistency with `Integer` (see above).
            std::variant<std::string, Suffix> suffix;

            CPPDECL_EQUALITY_DECLARE(FloatingPoint)
        };
        std::variant<Integer, FloatingPoint> var = Integer{}; // Need the initializer to prevent `https://github.com/llvm/llvm-project/issues/36032`.

        CPPDECL_EQUALITY_DECLARE(NumericLiteral)

        [[nodiscard]] constexpr bool IsInteger() const {return !IsFloatingPoint();}
        [[nodiscard]] constexpr bool IsFloatingPoint() const {return std::holds_alternative<FloatingPoint>(var);}

        // If tries to parse the integer. Returns null if `IsInteger() == false`, on overflow, or if the value somehow contains invalid characters.
        // Note that `.ToInteger() == 1234` does work, and silently returns false on parsing failure, which is convenient and intended.
        template <std::unsigned_integral T = std::uint64_t>
        [[nodiscard]] constexpr std::optional<T> ToInteger() const
        {
            if (!IsInteger())
                return {};

            const Integer &i = std::get<Integer>(var);

            bool (*validation_func)(char ch) = nullptr;
            T base = 0;
            switch (i.base)
            {
                case Integer::Base::decimal: validation_func = IsDigit;      base = 10; break;
                case Integer::Base::binary:  validation_func = IsBinDigit;   base = 2; break;
                case Integer::Base::octal:   validation_func = IsOctalDigit; base = 8; break;
                case Integer::Base::hex:     validation_func = IsHexDigit;   base = 16; break;
            }

            T ret{};

            for (char ch : i.value)
            {
                if (ch == '\'')
                    continue;

                if (!validation_func(ch))
                    return {}; // Invalid character in the string.

                T digit;
                if (ch >= 'a')
                    digit = T(ch - 'a' + 10);
                else if (ch >= 'A')
                    digit = T(ch - 'A' + 10);
                else
                    digit = T(ch - '0');

                T new_ret = ret * base;
                if (new_ret / base != ret)
                    return {}; // Overflow!

                ret = new_ret + digit;
            }

            return ret;
        }

        // Visit all instances of any of `C...` nested in this. (None for this type.) `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<NumericLiteral &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    // A string or character literal.
    struct StringOrCharLiteral
    {
        enum class Kind
        {
            character,
            string,
            raw_string,
        };
        Kind kind{};

        enum class Type
        {
            normal,
            wide,
            u8,
            u16,
            u32,
        };
        Type type = Type::normal;

        // The contents are not unescaped.
        std::string value;

        // User-defined literal suffix, if any.
        std::string literal_suffix;

        // This can be non-empty only if `kind == raw_string`.
        // This is the user-specified delimiter between `"` and `(`.
        std::string raw_string_delim;

        CPPDECL_EQUALITY_DECLARE(StringOrCharLiteral)

        // Visit all instances of any of `C...` nested in this. (None for this type.) `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<StringOrCharLiteral &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    struct PseudoExpr;

    // Some list of the form `(...)`, `{...}` or `[...]`.
    struct PseudoExprList
    {
        enum class Kind
        {
            parentheses,
            curly,
            square,
        };
        Kind kind{};

        std::vector<PseudoExpr> elems;

        // Only braced lists can have this.
        bool has_trailing_comma = false;

        CPPDECL_EQUALITY_DECLARE(PseudoExprList)

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<PseudoExprList &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    // This isn't really a proper expression, it's a fairly loose hierarchy of tokens.
    // We use this e.g. for non-type template arguments.
    struct PseudoExpr
    {
        // For simplicity, identifiers go into `SimpleType`, even if not technically types.
        using Token = std::variant<SimpleType, PunctuationToken, NumericLiteral, StringOrCharLiteral, PseudoExprList, TemplateArgumentList>;

        std::vector<Token> tokens;

        CPPDECL_EQUALITY_DECLARE(PseudoExpr)

        [[nodiscard]] CPPDECL_CONSTEXPR bool IsEmpty() const
        {
            return tokens.empty();
        }

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<PseudoExpr &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    // ] -- Things for non-type template arguments.

    // A declaration. Maybe named, maybe not.
    struct Decl
    {
        Type type;
        QualifiedName name;

        CPPDECL_EQUALITY_DECLARE(Decl)

        // Returns true if this is an invalid empty declaration.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsEmpty() const
        {
            // Note, not checking the name. It can legally be empty.
            return type.IsEmpty();
        }

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<Decl &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };


    // A wrapper for a list of ambiguous alternatives. Used for `Decl` and `Type` primarily.
    template <typename T>
    struct MaybeAmbiguous : T
    {
        // If the parsing was ambiguous, this can point to the alternative parse result.
        // In theory multiple alternatives could be chained, but I've yet to find input that causes that.
        IndirectOptional<MaybeAmbiguous<T>> ambiguous_alternative;

        // Another possible ambiguity is in nested declarations (function parameters). If this is the case, this is set recursively in all parents.
        bool has_nested_ambiguities = false;

        CPPDECL_CONSTEXPR MaybeAmbiguous() {}
        CPPDECL_CONSTEXPR MaybeAmbiguous(const T &other) : T(other) {}
        CPPDECL_CONSTEXPR MaybeAmbiguous(T &&other) : T(std::move(other)) {}

        CPPDECL_EQUALITY_DECLARE(MaybeAmbiguous)

        // Returns true if the parsing was ambiguous.
        // Then you can consult `ambiguous_alternative` for the list of alternative parses, either in this object or in some nested declarations,
        //   such as function parameters.
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsAmbiguous() const
        {
            return bool(ambiguous_alternative) || has_nested_ambiguities;
        }

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {T::template VisitEachComponent<C...>(flags, func); if (ambiguous_alternative) ambiguous_alternative->template VisitEachComponent<C...>(flags, func);}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {T::template VisitEachComponent<C...>(flags, func); if (ambiguous_alternative) ambiguous_alternative->template VisitEachComponent<C...>(flags, func);}
    };

    using MaybeAmbiguousDecl = MaybeAmbiguous<Decl>;


    // A template argument.
    struct TemplateArgument
    {
        using Variant = std::variant<Type, PseudoExpr>; // Sync with the `TemplateArgumentType` concept.
        Variant var;

        CPPDECL_EQUALITY_DECLARE(TemplateArgument)

        // Returns true if this instance holds `T` (which is either `Type` or `PseudoExpr`) equal to `value`.
        // You'd think `std::variant` would support this kind of comparison natively, but alas!
        template <TemplateArgumentType T>
        [[nodiscard]] CPPDECL_CONSTEXPR bool Matches(const T &value) const
        {
            auto elem = std::get_if<T>(&var);
            return elem && *elem == value;
        }

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<TemplateArgument &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };


    // Has to be non-member, since a lot of different classes use it.
    enum class ModifierEqualsFlags
    {
        // Pretend the target is `const`, if it can be cv-qualified at all.
        as_if_target_is_const = 1 << 0,
    };
    CPPDECL_FLAG_OPERATORS(ModifierEqualsFlags)

    // A base class for type modifiers (applied by decorators) that have cv-qualifiers (and/or restrict-qualifiers, so references do count).
    struct QualifiedModifier
    {
        CvQualifiers quals{};

        CPPDECL_EQUALITY_DECLARE(QualifiedModifier)

        [[nodiscard]] CPPDECL_CONSTEXPR bool Equals(const QualifiedModifier &target, ModifierEqualsFlags flags) const
        {
            CvQualifiers target_quals_fixed = target.quals;
            if (bool(flags & ModifierEqualsFlags::as_if_target_is_const))
                target_quals_fixed |= CvQualifiers::const_;
            return quals == target_quals_fixed;
        }

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
        {
            (void)flags;

            if constexpr ((std::same_as<C, CvQualifiers> || ...))
                func(quals);
        }
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<QualifiedModifier &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    // A pointer to...
    struct Pointer : QualifiedModifier
    {
        CPPDECL_EQUALITY_DECLARE(Pointer)

        // Shadowing `QualifiedModifier::Equals()` solely to adjust the parameter type.
        [[nodiscard]] CPPDECL_CONSTEXPR bool Equals(const Pointer &target, ModifierEqualsFlags flags) const
        {
            return QualifiedModifier::Equals(target, flags);
        }

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {QualifiedModifier::VisitEachComponent<C...>(flags, func);}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {QualifiedModifier::VisitEachComponent<C...>(flags, func);}
    };

    // A reference to...
    // Can't have cv-qualifiers, but can have `__restrict`.
    struct Reference : QualifiedModifier
    {
        RefQualifier kind = RefQualifier::lvalue; // Will never be `none.

        CPPDECL_EQUALITY_DECLARE(Reference)

        [[nodiscard]] CPPDECL_CONSTEXPR bool Equals(const Reference &target, ModifierEqualsFlags flags) const
        {
            return QualifiedModifier::Equals(target, flags) && kind == target.kind;
        }

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {QualifiedModifier::VisitEachComponent<C...>(flags, func);}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {QualifiedModifier::VisitEachComponent<C...>(flags, func);}
    };

    // A member pointer to... (a variable/function of type...)
    struct MemberPointer : QualifiedModifier
    {
        QualifiedName base;

        CPPDECL_EQUALITY_DECLARE(MemberPointer)

        [[nodiscard]] CPPDECL_CONSTEXPR bool Equals(const MemberPointer &target, ModifierEqualsFlags flags) const
        {
            return QualifiedModifier::Equals(target, flags) && base == target.base;
        }

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
        {
            base.VisitEachComponent<C...>(flags, func);
            QualifiedModifier::VisitEachComponent<C...>(flags, func);
        }
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<MemberPointer &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    // An array of... (elements of type...)
    struct Array
    {
        // This can be empty.
        PseudoExpr size;

        CPPDECL_EQUALITY_DECLARE(Array)

        [[nodiscard]] CPPDECL_CONSTEXPR bool Equals(const Array &target, ModifierEqualsFlags flags) const
        {
            (void)flags;
            return *this == target;
        }

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {size.VisitEachComponent<C...>(flags, func);}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {size.VisitEachComponent<C...>(flags, func);}
    };

    // A function returning...
    struct Function
    {
        std::vector<MaybeAmbiguousDecl> params;
        CvQualifiers cv_quals{};
        RefQualifier ref_qual{};

        bool noexcept_ = false;

        // Uses trailing return type.
        bool uses_trailing_return_type = false;

        // This function has no parameters, which as spelled as `(void)` in C style.
        // This can only be set if `params` is empty.
        bool c_style_void_params = false;

        // This function has a trailing C-style variadic `...` parameter.
        bool c_style_variadic = false;
        // This function has a trailing C-style variadic `...` parameter, which lacks the comma before it (illegal since C++26).
        // This can only be set if `c_style_variadic` is also set.
        bool c_style_variadic_without_comma = false;

        CPPDECL_EQUALITY_DECLARE(Function)

        [[nodiscard]] CPPDECL_CONSTEXPR bool Equals(const Function &target, ModifierEqualsFlags flags) const
        {
            (void)flags;
            return *this == target; // Glad I don't have to reimplement this... Yet.
        }

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<Function &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    // Mostly for internal use. Prefer `TypeModifier::SpelledAfterIdentifier()`.
    // Should this modifier type be spelled to the right of the identifier (if any) or to the left?
    template <typename T> struct ModifierIsSpelledAfterIdentifier {};
    template <> struct ModifierIsSpelledAfterIdentifier<Pointer> : std::false_type {};
    template <> struct ModifierIsSpelledAfterIdentifier<Reference> : std::false_type {};
    template <> struct ModifierIsSpelledAfterIdentifier<MemberPointer> : std::false_type {};
    template <> struct ModifierIsSpelledAfterIdentifier<Array> : std::true_type {};
    template <> struct ModifierIsSpelledAfterIdentifier<Function> : std::true_type {};

    // A type modifier that is a part of a declarator (i.e. applies to only variable in a declaration),
    //   such as "pointer", "array" (unbounded or fixed-size), etc.
    struct TypeModifier
    {
        using Variant = std::variant<Pointer, Reference, MemberPointer, Array, Function>;
        Variant var;

        CPPDECL_EQUALITY_DECLARE(TypeModifier)

        [[nodiscard]] CPPDECL_CONSTEXPR bool Equals(const TypeModifier &target, ModifierEqualsFlags flags) const
        {
            // Simply dispatch to the `var`.
            return std::visit([&]<typename T>(const T &elem){
                auto target_elem = std::get_if<T>(&target.var);
                return target_elem && elem.Equals(*target_elem, flags);
            }, var);
        }

        // Returns the qualifiers of this modifier, if any.
        [[nodiscard]] CPPDECL_CONSTEXPR CvQualifiers GetQualifiers() const
        {
            auto ret = const_cast<TypeModifier &>(*this).GetQualifiersMut();
            return ret ? *ret : CvQualifiers{};
        }
        // Same but mutable, and null if this can't have qualifiers.
        [[nodiscard]] CPPDECL_CONSTEXPR CvQualifiers *GetQualifiersMut()
        {
            // Note that we can't `Overload{...}` between `QualifiedModifier &` and `auto &`, because the latter is a better match for derived classes.
            return std::visit(
                []<typename T>(T &q){
                    if constexpr (std::is_base_of_v<QualifiedModifier, T>)
                        return &q.quals;
                    else
                        return (CvQualifiers *)nullptr;
                },
                var
            );
        }

        // Should this modifier be spelled to the right of the identifier (if any) or to the left?
        // Depends only on the type of the modifier.
        [[nodiscard]] CPPDECL_CONSTEXPR bool SpelledAfterIdentifier() const
        {
            return std::visit([]<typename T>(const T &){return ModifierIsSpelledAfterIdentifier<T>::value;}, var);
        }

        template <typename T> [[nodiscard]] CPPDECL_CONSTEXPR bool Is() const {return bool(As<T>());}
        template <typename T> [[nodiscard]] CPPDECL_CONSTEXPR       T *As()       {return std::get_if<T>(&var);}
        template <typename T> [[nodiscard]] CPPDECL_CONSTEXPR const T *As() const {return std::get_if<T>(&var);}

        // Visit all instances of any of `C...` nested in this, if any.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<TypeModifier &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    struct Attribute
    {
        enum class Style
        {
            cpp, // `[[foo]]`
            gnu, // `__attribute__((foo))`
        };
        Style style = Style::cpp;

        // The first token here will typically be the attribute name.
        PseudoExpr expr{};

        CPPDECL_EQUALITY_DECLARE(Attribute)

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
        {
            expr.VisitEachComponent<C...>(flags, func);

            // Using postorder here for consistency with everything else.
            if constexpr ((std::same_as<C, Attribute> || ...))
                func(*this);
        }
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<Attribute &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };


    // --- Function definitions:

    CPPDECL_EQUALITY_DEFINE(TemplateArgumentList)

    [[nodiscard]] CPPDECL_CONSTEXPR bool TemplateArgumentList::Matches(const TemplateArgumentType auto &... values) const
    {
        std::size_t i = 0;
        return sizeof...(values) == args.size() && (args[i++].Matches(values) && ...);
    }

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void TemplateArgumentList::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        for (auto &arg : args)
            arg.VisitEachComponent<C...>(flags, func);
    }

    CPPDECL_EQUALITY_DEFINE(OverloadedOperator)
    CPPDECL_EQUALITY_DEFINE(ConversionOperator)
    CPPDECL_EQUALITY_DEFINE(UserDefinedLiteral)
    CPPDECL_EQUALITY_DEFINE(DestructorName)
    CPPDECL_EQUALITY_DEFINE(UnqualifiedName)

    CPPDECL_CONSTEXPR bool UnqualifiedName::Equals(const UnqualifiedName &target, EqualsFlags flags) const
    {
        if (!(bool(flags & EqualsFlags::allow_missing_template_args_in_target) && bool(template_args) && !bool(target.template_args)))
        {
            if (template_args != target.template_args)
                return false;
        }

        return var == target.var;
    }

    CPPDECL_CONSTEXPR bool UnqualifiedName::IsSingleWord(SingleWordFlags flags) const
    {
        return
            std::holds_alternative<std::string>(var) &&
            (bool(flags & SingleWordFlags::ignore_template_args) || !template_args);
    }

    CPPDECL_CONSTEXPR std::string_view UnqualifiedName::AsSingleWord(SingleWordFlags flags) const
    {
        if (IsSingleWord(flags))
            return std::get<std::string>(var);
        else
            return "";
    }

    CPPDECL_CONSTEXPR bool UnqualifiedName::IsBuiltInTypeName(IsBuiltInTypeNameFlags flags) const
    {
        std::string_view word = AsSingleWord();
        if (word.empty())
            return false;

        // See the comment on this function for more information.

        if (bool(flags & IsBuiltInTypeNameFlags::allow_void) && IsTypeNameKeywordVoid(word))
            return true;
        if (bool(flags & IsBuiltInTypeNameFlags::allow_integral) && (IsTypeNameKeywordIntegral(word) || word == "long long"))
            return true;
        if (bool(flags & IsBuiltInTypeNameFlags::allow_bool) && IsTypeNameKeywordBool(word))
            return true;
        if (bool(flags & IsBuiltInTypeNameFlags::allow_floating_point) && (IsTypeNameKeywordFloatingPoint(word) || word == "long double" || word == "double long"))
            return true;

        return false;
    }

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void UnqualifiedName::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        if (
            !bool(flags & VisitEachComponentFlags::no_recurse_into_names) &&
            (
                !bool(flags & VisitEachComponentFlags::no_recurse_into_nontype_names) ||
                !bool(flags & VisitEachComponentFlags::this_name_is_nontype)
            )
        )
        {
            if (template_args)
            {
                template_args->VisitEachComponent<C...>(flags & ~VisitEachComponentFlags::this_name_is_nontype, func);

                if constexpr ((std::same_as<C, TemplateArgumentList> || ...))
                    func(*template_args);
            }

            std::visit(Overload{
                [](std::string &){}, // Nothing here.
                [&](auto &elem){elem.template VisitEachComponent<C...>(flags, func);}
            }, var);
        }
    }

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void PunctuationToken::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        (void)flags;

        if constexpr ((std::same_as<C, PunctuationToken> || ...))
            func(*this);
    }

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void NumericLiteral::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        (void)flags;

        if constexpr ((std::same_as<C, NumericLiteral> || ...))
            func(*this);
    }

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void StringOrCharLiteral::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        (void)flags;

        if constexpr ((std::same_as<C, StringOrCharLiteral> || ...))
            func(*this);
    }

    CPPDECL_CONSTEXPR QualifiedName QualifiedName::FromSingleWord(std::string part)
    {
        auto ret = FromSinglePart(UnqualifiedName{.var = std::move(part), .template_args = {}});

        // `IsBuiltInTypeName()` is for checking names with spaces. Some parts of it are redundant here, but it does the job.
        assert((IsValidIdentifier(ret.AsSingleWord()) || ret.IsBuiltInTypeName()) && "This is not a single word.");

        return ret;
    }

    CPPDECL_CONSTEXPR QualifiedName QualifiedName::FromSinglePart(UnqualifiedName part)
    {
        QualifiedName ret;
        ret.parts.push_back(std::move(part));
        return ret;
    }

    template <typename P>               CPPDECL_CONSTEXPR QualifiedName & QualifiedName::AddPart(P &&part) &  {parts.emplace_back(std::forward<P>(part)); return *this;}
    template <typename P> [[nodiscard]] CPPDECL_CONSTEXPR QualifiedName &&QualifiedName::AddPart(P &&part) && {parts.emplace_back(std::forward<P>(part)); return std::move(*this);}

    template <typename P>               CPPDECL_CONSTEXPR QualifiedName & QualifiedName::AddPart(std::size_t i, P &&part) &  {parts.emplace(parts.begin() + std::ptrdiff_t(i), std::forward<P>(part)); return *this;}
    template <typename P> [[nodiscard]] CPPDECL_CONSTEXPR QualifiedName &&QualifiedName::AddPart(std::size_t i, P &&part) && {parts.emplace(parts.begin() + std::ptrdiff_t(i), std::forward<P>(part)); return std::move(*this);}

    template <typename A> CPPDECL_CONSTEXPR QualifiedName &QualifiedName::AddTemplateArgument(A &&arg) &
    {
        UnqualifiedName &part = parts.back();
        if (!part.template_args)
            part.template_args.emplace();
        part.template_args->args.emplace_back(std::forward<A>(arg));
        return *this;
    }

    template <typename A> [[nodiscard]] CPPDECL_CONSTEXPR QualifiedName &&QualifiedName::AddTemplateArgument(A &&arg) &&
    {
        return std::move(AddTemplateArgument(std::forward<A>(arg)));
    }

    CPPDECL_EQUALITY_DEFINE(QualifiedName)

    CPPDECL_CONSTEXPR bool QualifiedName::Equals(const QualifiedName &target, EqualsFlags flags) const
    {
        if (force_global_scope != target.force_global_scope)
            return false;

        if (bool(flags & EqualsFlags::allow_less_parts_in_target) ? parts.size() < target.parts.size() : parts.size() != target.parts.size())
            return false;

        // Have to use `target.parts.size()` instead of `parts.size()` here and below in this function,
        //   because with `EqualsFlags::allow_less_parts_in_target`, `target.parts.size()` can be smaller than `parts.size()`.
        for (std::size_t i = 0; i < target.parts.size(); i++)
        {
            if (!parts[i].Equals(target.parts[i], (bool(flags & EqualsFlags::allow_missing_final_template_args_in_target) && i + 1 == target.parts.size()) * UnqualifiedName::EqualsFlags::allow_missing_template_args_in_target))
                return false;
        }

        return true;
    }

    CPPDECL_CONSTEXPR bool QualifiedName::IsQualified() const
    {
        return force_global_scope || parts.size() > 1;
    }

    CPPDECL_CONSTEXPR bool QualifiedName::LastComponentIsNormalString() const
    {
        return !parts.empty() && std::holds_alternative<std::string>(parts.back().var);
    }

    CPPDECL_CONSTEXPR bool QualifiedName::IsConversionOperatorName() const
    {
        return !parts.empty() && std::holds_alternative<ConversionOperator>(parts.back().var);
    }

    CPPDECL_CONSTEXPR bool QualifiedName::IsDestructorName() const
    {
        return !parts.empty() && std::holds_alternative<DestructorName>(parts.back().var);
    }

    CPPDECL_CONSTEXPR bool QualifiedName::CertainlyIsQualifiedConstructorName() const
    {
        // Need at least two parts.
        if (parts.size() < 2)
            return false;

        // Second part must have no template arguments.
        if (parts.back().template_args)
            return false;

        // Both must be equal strings.
        const std::string *a = std::get_if<std::string>(&parts[parts.size() - 2].var);
        const std::string *b = std::get_if<std::string>(&parts.back().var);
        return a && b && *a == *b;
    }

    CPPDECL_CONSTEXPR QualifiedName::EmptyReturnType QualifiedName::IsFunctionNameRequiringEmptyReturnType() const
    {
        if (parts.empty())
            return EmptyReturnType::no;

        if (IsConversionOperatorName() || IsDestructorName())
            return EmptyReturnType::yes;
        if (CertainlyIsQualifiedConstructorName())
            return EmptyReturnType::yes;

        if (!LastComponentIsNormalString())
            return EmptyReturnType::no;

        // An unqualified constructor perhaps?
        // Or a qualified one that uses a typedef, e.g. `using A = B;`, `A::B()`.

        // Qualified constructors apparently can't have template arguments.
        // Unqualified could have them before C++20, but we're nor allowing this here.
        if (parts.size() > 1 && parts.back().template_args)
            return EmptyReturnType::no;

        if (parts.size() > 1)
            return EmptyReturnType::maybe_qual_constructor_using_typedef;
        else
            return EmptyReturnType::maybe_unqual_constructor;
    }

    CPPDECL_CONSTEXPR bool QualifiedName::IsSingleWord(SingleWordFlags flags) const
    {
        return !force_global_scope && parts.size() == 1 && parts.front().IsSingleWord(flags);
    }

    CPPDECL_CONSTEXPR std::string_view QualifiedName::AsSingleWord(SingleWordFlags flags) const
    {
        if (IsSingleWord(flags))
            return parts.front().AsSingleWord();
        else
            return {};
    }

    CPPDECL_CONSTEXPR bool QualifiedName::IsBuiltInTypeName(IsBuiltInTypeNameFlags flags) const
    {
        if (!force_global_scope && parts.size() == 1)
            return parts.front().IsBuiltInTypeName(flags);

        return false;
    }

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void QualifiedName::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        if (
            !bool(flags & VisitEachComponentFlags::no_recurse_into_names) &&
            (
                !bool(flags & VisitEachComponentFlags::no_recurse_into_nontype_names) ||
                !bool(flags & VisitEachComponentFlags::this_name_is_nontype)
            )
        )
        {
            for (auto &part : parts)
                // Preserving `VisitEachComponentFlags::this_name_is_nontype` here. Unqualified names will strip it themselves.
                part.VisitEachComponent<C...>(flags, func);
        }


        // Should we use preorder or postorder traversal here? Currently it's postorder.
        // The difference matters when simplifying the names. With preorder, we need to compare longer names,
        //   but at the same time the simplification process needs to be done less times.
        // But more importantly, this way we can handle DIFFERENT spellings of different template arguments that simplify to the same spelling.
        // This looks desirable, therefore postorder it is.
        if constexpr ((std::same_as<C, QualifiedName> || ...))
        {
            if (
                !bool(flags & VisitEachComponentFlags::no_visit_nontype_names) ||
                !bool(flags & VisitEachComponentFlags::this_name_is_nontype)
            )
            {
                func(*this);
            }
        }
    }

    CPPDECL_EQUALITY_DEFINE(AttributeList)

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void AttributeList::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        for (Attribute &attr : attrs)
            attr.VisitEachComponent<C...>(flags, func);
    }

    CPPDECL_CONSTEXPR SimpleType SimpleType::FromSingleWord(std::string part)
    {
        SimpleType ret;
        ret.name = QualifiedName::FromSingleWord(std::move(part));
        return ret;
    }

    CPPDECL_CONSTEXPR SimpleType SimpleType::FromUnqualifiedName(UnqualifiedName part)
    {
        SimpleType ret;
        ret.name = QualifiedName::FromSinglePart(std::move(part));
        return ret;
    }

    CPPDECL_CONSTEXPR SimpleType SimpleType::FromQualifiedName(QualifiedName name)
    {
        SimpleType ret;
        ret.name = std::move(name);
        return ret;
    }

    CPPDECL_EQUALITY_DEFINE(SimpleType)

    CPPDECL_CONSTEXPR Type Type::FromSingleWord(std::string part)
    {
        Type ret;
        ret.simple_type = SimpleType::FromSingleWord(std::move(part));
        return ret;
    }

    CPPDECL_CONSTEXPR Type Type::FromUnqualifiedName(UnqualifiedName part)
    {
        Type ret;
        ret.simple_type = SimpleType::FromUnqualifiedName(std::move(part));
        return ret;
    }

    CPPDECL_CONSTEXPR Type Type::FromQualifiedName(QualifiedName name)
    {
        Type ret;
        ret.simple_type = SimpleType::FromQualifiedName(std::move(name));
        return ret;
    }

    CPPDECL_CONSTEXPR Type Type::FromSimpleType(SimpleType simple_type)
    {
        Type ret;
        ret.simple_type = std::move(simple_type);
        return ret;
    }

    CPPDECL_EQUALITY_DEFINE(Type)

    CPPDECL_CONSTEXPR bool Type::Equals(const Type &target, EqualsFlags eq_flags) const
    {
        std::size_t n = modifiers.size();
        if (n != target.modifiers.size())
            return false;

        bool as_if_target_is_const = bool(eq_flags & EqualsFlags::as_if_target_is_const);

        for (std::size_t i = 0; i < n; i++)
        {
            if (!modifiers[i].Equals(target.modifiers[i], ModifierEqualsFlags::as_if_target_is_const * as_if_target_is_const))
                return false;
            as_if_target_is_const = false; // Only applies to the first modifier. If no modifiers, applies to the `simple_type` below.
        }

        return simple_type.Equals(target.simple_type, SimpleType::EqualsFlags::as_if_target_is_const * as_if_target_is_const);
    }

    template <typename T> CPPDECL_CONSTEXPR       T *Type::As(std::size_t i)       {return i < modifiers.size() ? modifiers[i].As<T>() : nullptr;}
    template <typename T> CPPDECL_CONSTEXPR const T *Type::As(std::size_t i) const {return i < modifiers.size() ? modifiers[i].As<T>() : nullptr;}

    CPPDECL_CONSTEXPR bool Type::IsConst(std::size_t i) const
    {
        return bool(GetQualifiers(i) & CvQualifiers::const_);
    }

    CPPDECL_CONSTEXPR bool Type::IsConstOrReference(std::size_t i) const
    {
        return IsConst(i) || Is<Reference>(i);
    }

    CPPDECL_CONSTEXPR CvQualifiers Type::GetQualifiers(std::size_t i) const
    {
        auto ret = const_cast<Type &>(*this).GetQualifiersMut(i);
        return ret ? *ret : CvQualifiers{};
    }

    CPPDECL_CONSTEXPR CvQualifiers *Type::GetQualifiersMut(std::size_t i)
    {
        if (i == modifiers.size())
            return &simple_type.quals;
        else if (i < modifiers.size())
            return modifiers[i].GetQualifiersMut();
        else
            return nullptr;
    }

    template <typename M> CPPDECL_CONSTEXPR Type & Type::AddModifier(M &&mod, std::size_t i) &  {modifiers.emplace(modifiers.begin() + std::ptrdiff_t(i), std::forward<M>(mod)); return *this;}
    template <typename M> CPPDECL_CONSTEXPR Type &&Type::AddModifier(M &&mod, std::size_t i) && {modifiers.emplace(modifiers.begin() + std::ptrdiff_t(i), std::forward<M>(mod)); return std::move(*this);}

    CPPDECL_CONSTEXPR Type &Type::RemoveModifier(std::size_t i) &
    {
        modifiers.erase(modifiers.begin() + std::ptrdiff_t(i));
        return *this;
    }

    CPPDECL_CONSTEXPR Type &&Type::RemoveModifier(std::size_t i) &&
    {
        RemoveModifier(i);
        return std::move(*this);
    }

    CPPDECL_CONSTEXPR void Type::AppendType(Type other)
    {
        assert(simple_type.IsEmpty());

        simple_type = std::move(other.simple_type);
        modifiers.insert(modifiers.end(), std::make_move_iterator(other.modifiers.begin()), std::make_move_iterator(other.modifiers.end()));
    }

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void Type::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        simple_type.VisitEachComponent<C...>(flags, func);
        for (TypeModifier &m : modifiers)
            m.VisitEachComponent<C...>(flags, func);
    }

    CPPDECL_EQUALITY_DEFINE(PunctuationToken)

    CPPDECL_EQUALITY_DEFINE(NumericLiteral::Integer::Suffix)
    CPPDECL_EQUALITY_DEFINE(NumericLiteral::Integer)
    CPPDECL_EQUALITY_DEFINE(NumericLiteral::FloatingPoint)
    CPPDECL_EQUALITY_DEFINE(NumericLiteral)

    CPPDECL_EQUALITY_DEFINE(StringOrCharLiteral)

    CPPDECL_EQUALITY_DEFINE(PseudoExprList)

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void PseudoExprList::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        for (auto &elem : elems)
            elem.VisitEachComponent<C...>(flags, func);
    }

    CPPDECL_EQUALITY_DEFINE(PseudoExpr)

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void PseudoExpr::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        for (auto &token : tokens)
            std::visit([&](auto &elem){elem.template VisitEachComponent<C...>(flags, func);}, token);
    }

    CPPDECL_EQUALITY_DEFINE(Decl)

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void Decl::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        type.VisitEachComponent<C...>(flags, func);
        name.VisitEachComponent<C...>(flags | VisitEachComponentFlags::this_name_is_nontype, func);
    }

    template <typename T>
    CPPDECL_EQUALITY_DEFINE(MaybeAmbiguous<T>)

    CPPDECL_EQUALITY_DEFINE(TemplateArgument)

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void TemplateArgument::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        std::visit([&](auto &elem){elem.template VisitEachComponent<C...>(flags, func);}, var);
    }

    CPPDECL_EQUALITY_DEFINE(QualifiedModifier)
    CPPDECL_EQUALITY_DEFINE(Pointer)
    CPPDECL_EQUALITY_DEFINE(Reference)
    CPPDECL_EQUALITY_DEFINE(MemberPointer)
    CPPDECL_EQUALITY_DEFINE(Array)

    CPPDECL_EQUALITY_DEFINE(Function)

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void Function::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        if constexpr ((std::same_as<C, CvQualifiers> || ...))
            func(cv_quals);

        for (auto &param : params)
            param.VisitEachComponent<C...>(flags, func);
    }

    CPPDECL_EQUALITY_DEFINE(TypeModifier)

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void TypeModifier::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        std::visit([&](auto &elem){elem.template VisitEachComponent<C...>(flags, func);}, var);
    }

    CPPDECL_EQUALITY_DEFINE(Attribute)
}
