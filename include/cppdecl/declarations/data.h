#pragma once

#include "cppdecl/misc/enum_flags.h"
#include "cppdecl/misc/indirect_optional.h"
#include "cppdecl/misc/overload.h"
#include "cppdecl/misc/platform.h"
#include "cppdecl/misc/string_helpers.h"

#include <cassert>
#include <concepts>
#include <cstddef>
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
        msvc_ptr64 = 1 << 4, // MSVC's `__ptr64` that appears in `typeid(...).name()` for 32-bit pointers, as in `int * __ptr64`.
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

    struct QualifiedName;
    struct SimpleType;

    template <typename T>
    concept VisitableComponentType = std::same_as<T, QualifiedName> || std::same_as<T, CvQualifiers> || std::same_as<T, SimpleType>;

    struct TemplateArgument;

    struct TemplateArgumentList
    {
        std::vector<TemplateArgument> args;

        friend CPPDECL_CONSTEXPR bool operator==(const TemplateArgumentList &, const TemplateArgumentList &);

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
        template <typename P>               CPPDECL_CONSTEXPR QualifiedName & AddPart(P &&part) &  {parts.emplace_back(std::forward<P>(part)); return *this;}
        template <typename P> [[nodiscard]] CPPDECL_CONSTEXPR QualifiedName &&AddPart(P &&part) && {parts.emplace_back(std::forward<P>(part)); return std::move(*this);}

        friend CPPDECL_CONSTEXPR bool operator==(const QualifiedName &, const QualifiedName &b);

        enum class EqualsFlags
        {
            // If the `target` has no template arguments on the LAST part, but we do have them, ignore the template arguments in the comparison of that part.
            allow_missing_final_template_args_in_target = 1 << 0,

            // Allow the `target` to be a prefix of this name.
            // If `allow_missing_final_template_args_in_target` is also specified, then it still applies to the last part in `target`, even though it's no longer the last in self.
            allow_shorter_target = 1 << 1,
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

    // A type, maybe cv-qualified, but without any pointer qualifiers and such.
    // This also corresponds to the decl-specifier-seq, aka the common part of the type shared between all variables in a declaration.
    struct SimpleType
    {
        CvQualifiers quals{};
        SimpleTypeFlags flags{};
        SimpleTypePrefix prefix{};

        // The type name. Never includes `signed` or `unsigned`, that's in `flags`.
        QualifiedName name;

        // This accepts only single-word type names without `::` or any other punctuation. Accepts `long long` and `long double`, `double long`, but rejects signedness and such.
        [[nodiscard]] static CPPDECL_CONSTEXPR SimpleType FromSingleWord(std::string part);
        [[nodiscard]] static CPPDECL_CONSTEXPR SimpleType FromUnqualifiedName(UnqualifiedName part);
        [[nodiscard]] static CPPDECL_CONSTEXPR SimpleType FromQualifiedName(QualifiedName name);

        friend CPPDECL_CONSTEXPR bool operator==(const SimpleType &, const SimpleType &);

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

        // This accepts only single-word type names without `::` or any other punctuation. Accepts `long long` and `long double`, `double long`, but rejects signedness and such.
        [[nodiscard]] static CPPDECL_CONSTEXPR Type FromSingleWord(std::string part);
        [[nodiscard]] static CPPDECL_CONSTEXPR Type FromUnqualifiedName(UnqualifiedName part);
        [[nodiscard]] static CPPDECL_CONSTEXPR Type FromQualifiedName(QualifiedName name);
        [[nodiscard]] static CPPDECL_CONSTEXPR Type FromSimpleType(SimpleType simple_type);

        friend CPPDECL_CONSTEXPR bool operator==(const Type &, const Type &);

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

        friend CPPDECL_CONSTEXPR bool operator==(const OverloadedOperator &, const OverloadedOperator &);

        // Visit all instances of any of `C...` nested in this. (None for this type.) `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {(void)flags; (void)func;}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {(void)flags; (void)func;}
    };

    // Represents `operator T`.
    struct ConversionOperator
    {
        Type target_type;

        friend CPPDECL_CONSTEXPR bool operator==(const ConversionOperator &, const ConversionOperator &);

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

        friend CPPDECL_CONSTEXPR bool operator==(const UserDefinedLiteral &, const UserDefinedLiteral &);

        // Visit all instances of any of `C...` nested in this. (None for this type.) `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {(void)flags; (void)func;}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {(void)flags; (void)func;}
    };

    // A destructor name of the form `~Blah`.
    struct DestructorName
    {
        SimpleType simple_type;

        friend CPPDECL_CONSTEXPR bool operator==(const DestructorName &, const DestructorName &);

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

        friend CPPDECL_CONSTEXPR bool operator==(const UnqualifiedName &, const UnqualifiedName &);

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

        friend CPPDECL_CONSTEXPR bool operator==(const PunctuationToken &, const PunctuationToken &);

        // Visit all instances of any of `C...` nested in this. (None for this type.) `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {(void)flags; (void)func;}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {(void)flags; (void)func;}
    };

    // Some token that looks like a number.
    struct NumberToken
    {
        struct Integer
        {
            enum class Kind
            {
                decimal,
                binary,
                octal,
                hex,
            };
            Kind kind = Kind::decimal;

            std::string value;

            // Either an entire standard suffix, or a part of one (to which you can add `u`/`U` prefix or suffix).
            enum class SignedSuffix
            {
                l,
                ll,
                z,
            };

            struct Suffix
            {
                SignedSuffix signed_part{};
                bool is_unsigned = false; // Does the suffix include letter `u`?
            };
            // Either one of the built-in literal suffixes, or a custom string.
            // No suffix is stored as an empty string. This also helps us work around the Clang quirk with default member initializers in the first member of a variant.
            std::variant<std::string, Suffix> suffix;
        };
        struct FloatingPoint
        {
            enum class Kind
            {
                decimal,
                hex,
            };
            Kind kind = Kind::decimal;

            std::string value_int; // The integral part, or empty if none.
            std::string value_frac; // The fractional part, or empty if none.
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
        };
        std::variant<Integer, FloatingPoint> var = Integer{}; // Need the initializer to prevent `https://github.com/llvm/llvm-project/issues/36032`.

        friend CPPDECL_CONSTEXPR bool operator==(const NumberToken &, const NumberToken &);

        [[nodiscard]] constexpr bool IsInteger() const {return !IsFloatingPoint();}
        [[nodiscard]] constexpr bool IsFloatingPoint() const {return std::holds_alternative<FloatingPoint>(var);}

        // Visit all instances of any of `C...` nested in this. (None for this type.) `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {(void)flags; (void)func;}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {(void)flags; (void)func;}
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

        friend CPPDECL_CONSTEXPR bool operator==(const StringOrCharLiteral &, const StringOrCharLiteral &);

        // Visit all instances of any of `C...` nested in this. (None for this type.) `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {(void)flags; (void)func;}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {(void)flags; (void)func;}
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

        friend CPPDECL_CONSTEXPR bool operator==(const PseudoExprList &, const PseudoExprList &);

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
        using Token = std::variant<SimpleType, PunctuationToken, NumberToken, StringOrCharLiteral, PseudoExprList, TemplateArgumentList>;

        std::vector<Token> tokens;

        friend CPPDECL_CONSTEXPR bool operator==(const PseudoExpr &, const PseudoExpr &);

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

        friend CPPDECL_CONSTEXPR bool operator==(const Decl &, const Decl &);

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

        friend CPPDECL_CONSTEXPR bool operator==(const MaybeAmbiguous &, const MaybeAmbiguous &) = default;

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
        using Variant = std::variant<Type, PseudoExpr>;
        Variant var;

        friend CPPDECL_CONSTEXPR bool operator==(const TemplateArgument &, const TemplateArgument &);

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func);
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const
        {
            const_cast<TemplateArgument &>(*this).VisitEachComponent<C...>(flags, [&func](auto &comp){func(std::as_const(comp));});
        }
    };

    // A base class for type modifiers (applied by decorators) that have cv-qualifiers (and/or restrict-qualifiers, so references do count).
    struct QualifiedModifier
    {
        CvQualifiers quals{};

        friend CPPDECL_CONSTEXPR bool operator==(const QualifiedModifier &, const QualifiedModifier &);

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
        friend CPPDECL_CONSTEXPR bool operator==(const Pointer &, const Pointer &);

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {QualifiedModifier::VisitEachComponent<C...>(flags, func);}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {QualifiedModifier::VisitEachComponent<C...>(flags, func);}
    };

    // A reference to...
    // Can't have cv-qualifiers, but can have `__restrict`.
    struct Reference : QualifiedModifier
    {
        RefQualifier kind = RefQualifier::lvalue; // Will never be `none.

        friend CPPDECL_CONSTEXPR bool operator==(const Reference &, const Reference &);

        // Visit all instances of any of `C...` nested in this. `func` is `(auto &name) -> void`.
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func)       {QualifiedModifier::VisitEachComponent<C...>(flags, func);}
        template <VisitableComponentType ...C> CPPDECL_CONSTEXPR void VisitEachComponent(VisitEachComponentFlags flags, auto &&func) const {QualifiedModifier::VisitEachComponent<C...>(flags, func);}
    };

    // A member pointer to... (a variable/function of type...)
    struct MemberPointer : QualifiedModifier
    {
        QualifiedName base;

        friend CPPDECL_CONSTEXPR bool operator==(const MemberPointer &, const MemberPointer &);

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

        friend CPPDECL_CONSTEXPR bool operator==(const Array &, const Array &);

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

        friend CPPDECL_CONSTEXPR bool operator==(const Function &, const Function &);

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

        friend CPPDECL_CONSTEXPR bool operator==(const TypeModifier &, const TypeModifier &);

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


    // --- Function definitions:

    CPPDECL_CONSTEXPR bool operator==(const TemplateArgumentList &, const TemplateArgumentList &) = default;

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void TemplateArgumentList::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        for (auto &arg : args)
            arg.VisitEachComponent<C...>(flags, func);
    }

    CPPDECL_CONSTEXPR bool operator==(const OverloadedOperator &, const OverloadedOperator &) = default;
    CPPDECL_CONSTEXPR bool operator==(const ConversionOperator &, const ConversionOperator &) = default;
    CPPDECL_CONSTEXPR bool operator==(const UserDefinedLiteral &, const UserDefinedLiteral &) = default;
    CPPDECL_CONSTEXPR bool operator==(const DestructorName &, const DestructorName &) = default;
    CPPDECL_CONSTEXPR bool operator==(const UnqualifiedName &, const UnqualifiedName &) = default;

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
            std::visit(Overload{
                [](std::string &){}, // Nothing here.
                [&](auto &elem){elem.template VisitEachComponent<C...>(flags, func);}
            }, var);

            if (template_args)
                template_args->VisitEachComponent<C...>(flags & ~VisitEachComponentFlags::this_name_is_nontype, func);
        }
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

    CPPDECL_CONSTEXPR bool operator==(const QualifiedName &, const QualifiedName &) = default;

    CPPDECL_CONSTEXPR bool QualifiedName::Equals(const QualifiedName &target, EqualsFlags flags) const
    {
        if (force_global_scope != target.force_global_scope)
            return false;

        if (bool(flags & EqualsFlags::allow_shorter_target) ? parts.size() < target.parts.size() : parts.size() != target.parts.size())
            return false;

        // Have to use `target.parts.size()` instead of `parts.size()` here and below in this function,
        //   because with `EqualsFlags::allow_shorter_target`, `target.parts.size()` can be smaller than `parts.size()`.
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

    CPPDECL_CONSTEXPR bool operator==(const SimpleType &, const SimpleType &) = default;

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

    CPPDECL_CONSTEXPR bool operator==(const Type &, const Type &) = default;

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

    CPPDECL_CONSTEXPR bool operator==(const PunctuationToken &, const PunctuationToken &) = default;
    CPPDECL_CONSTEXPR bool operator==(const NumberToken &, const NumberToken &) = default;
    CPPDECL_CONSTEXPR bool operator==(const StringOrCharLiteral &, const StringOrCharLiteral &) = default;

    CPPDECL_CONSTEXPR bool operator==(const PseudoExprList &, const PseudoExprList &) = default;

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void PseudoExprList::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        for (auto &elem : elems)
            elem.VisitEachComponent<C...>(flags, func);
    }

    CPPDECL_CONSTEXPR bool operator==(const PseudoExpr &, const PseudoExpr &) = default;

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void PseudoExpr::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        for (auto &token : tokens)
            std::visit([&](auto &elem){elem.template VisitEachComponent<C...>(flags, func);}, token);
    }

    CPPDECL_CONSTEXPR bool operator==(const Decl &, const Decl &) = default;

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void Decl::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        type.VisitEachComponent<C...>(flags, func);
        name.VisitEachComponent<C...>(flags | VisitEachComponentFlags::this_name_is_nontype, func);
    }

    CPPDECL_CONSTEXPR bool operator==(const TemplateArgument &, const TemplateArgument &) = default;

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void TemplateArgument::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        std::visit([&](auto &elem){elem.template VisitEachComponent<C...>(flags, func);}, var);
    }

    CPPDECL_CONSTEXPR bool operator==(const QualifiedModifier &, const QualifiedModifier &) = default;
    CPPDECL_CONSTEXPR bool operator==(const Pointer &, const Pointer &) = default;
    CPPDECL_CONSTEXPR bool operator==(const Reference &, const Reference &) = default;
    CPPDECL_CONSTEXPR bool operator==(const MemberPointer &, const MemberPointer &) = default;
    CPPDECL_CONSTEXPR bool operator==(const Array &, const Array &) = default;

    CPPDECL_CONSTEXPR bool operator==(const Function &, const Function &) = default;

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void Function::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        if constexpr ((std::same_as<C, CvQualifiers> || ...))
            func(cv_quals);

        for (auto &param : params)
            param.VisitEachComponent<C...>(flags, func);
    }

    CPPDECL_CONSTEXPR bool operator==(const TypeModifier &, const TypeModifier &) = default;

    template <VisitableComponentType ...C>
    CPPDECL_CONSTEXPR void TypeModifier::VisitEachComponent(VisitEachComponentFlags flags, auto &&func)
    {
        std::visit([&](auto &elem){elem.template VisitEachComponent<C...>(flags, func);}, var);
    }
}
