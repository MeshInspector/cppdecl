#pragma once

#include "cppdecl/declarations/data.h"
#include "cppdecl/misc/enum_flags.h"
#include "cppdecl/misc/overload.h"
#include "cppdecl/misc/platform.h"
#include "cppdecl/misc/string_helpers.h"

#include <cassert>
#include <cstddef>
#include <string>
#include <utility>

// Converts parsed types/declarations back to code or to a textual description.
// * `ToCode(x, {})` converts to code.
// * `ToString(x, {})` converts to textual description.
// You can pass some flags instead of `{}`, see below. But the default flags should usually be good enough.

namespace cppdecl
{
    enum class ToCodeFlags
    {
        // Style flags: [

        // Avoid printing spaces after commas.
        no_space_after_comma = 1 << 0,

        // `int const` instead of `const int`.
        east_const = 1 << 1,

        // Do `int*x` instead of `int *x`. If you also want to add the space after it, consider using `left_align_pointer` directly.
        no_space_before_pointer = 1 << 2,
        // Do `int * x` instead of `int *x`. If you also want to remove the space before it, consider using `left_align_pointer` directly.
        add_space_after_pointer = 1 << 3,

        // Do `int* x` instead of `int *x`.
        left_align_pointer = no_space_before_pointer | add_space_after_pointer,

        // `0X` instead of `0x`.
        numeric_literals_uppercase_prefix = 1 << 4,

        // At most one: [

        // `0xABC` instead of `0xabC`.
        numeric_literals_uppercase_digits = 1 << 5,
        // `0xabc` instead of `0xabC`.
        // This is the only flag here with a lowercase version, because only the digits are stored in their original case.
        // The case of the prefixes/exponents/stock suffixes is lost, so they can default to lowercase.
        numeric_literals_lowercase_digits = 1 << 6,
        // ]

        // `1E2` instead of `1e2`.
        numeric_literals_uppercase_exponent = 1 << 7,
        // `1U` instead of `1u`.
        numeric_literals_uppercase_suffix = 1 << 8,
        // Use caps for everything in numeric literals.
        numeric_literals_uppercase = numeric_literals_uppercase_prefix | numeric_literals_uppercase_digits | numeric_literals_uppercase_exponent | numeric_literals_uppercase_suffix,
        // Use lowercase for everything in numeric literals.
        numeric_literals_lowercase = numeric_literals_lowercase_digits, // Welp.

        // `1lu` instead of `1ul`.
        numeric_literals_suffix_unsigned_last = 1 << 9,
        // ] End style flags.


        // Canonicalization: [

        // Refuse to print trailing return types, convert them to the normal spelling.
        // This is good for making C++-style declarations usable in C.
        force_no_trailing_return_type = 1 << 10,

        // Force print `foo(int...)` as `foo(int, ...)`. The two are equivalent, and the former
        force_comma_before_c_style_variadic = 1 << 11,

        // Don't spell `signed` except in `signed char`.
        force_no_redundant_signed = 1 << 12,

        // Don't spell elaborated type specifiers and `typename`.
        force_no_type_prefix = 1 << 13,

        // Remove any `'`s from the literals.
        numeric_literals_strip_apostrophes = 1 << 14,

        // At most one: [

        // Replace `.42` with `0.42`.
        numeric_literals_force_zero_before_point = 1 << 15,
        // Replace `0.42` with `.42`.
        numeric_literals_no_zero_before_point = 1 << 16,
        // ]

        // At most one: [numeric_literals_no_zero_frac,

        // Replace `42.` with `42.0`.
        numeric_literals_force_zero_after_point = 1 << 17,
        // Replace `42.0` with `42.`.
        numeric_literals_no_zero_after_point = 1 << 18,
        // ]

        // At most one: [

        // Omit zero or empty fractional part, IF there is also exponent.
        // The exponent requirement is there in case you want the resulting numbers to still parse as floating-point literals, rather than integral ones.
        numeric_literals_no_zero_frac_before_exponent = 1 << 19,
        // Omit zero or empty fractional part, regardless of exponent. This will cause numbers without exponent to roundtrip as integers.
        // Conflicts with `numeric_literals_force_zero_after_point` and `numeric_literals_no_zero_after_point`.
        numeric_literals_no_zero_frac = 1 << 20,
        // ]

        // At most one: [

        // Omit zero exponent if the fractional part also exists (except on hex floats, which always require the exponent).
        // The requirement of having the fraction part is there in case you want the resulting numbers to still parse as floating-point literals, rather than integral ones.
        // This has priority over `numeric_literals_no_zero_frac_before_exponent`, but loses to `numeric_literals_no_zero_frac`.
        numeric_literals_no_zero_exponent_after_frac = 1 << 21,
        // Omit zero exponent (except on hex floats, which always require the exponent). This will cause numbers without a fractional part to roundtrip as integers.
        // Conflicts with `numeric_literals_no_zero_frac`.
        numeric_literals_no_zero_exponent = 1 << 22,
        // ]

        // At most one: [

        // Force `1e2` instead of `1e+2`. Also removes the `-` in negative zero exponents.
        numeric_literals_no_exponent_useless_sign = 1 << 23,
        // Force `1e+2` instead of `1e2`.
        // Also flips the digit to `+` on negative zero exponents.
        numeric_literals_force_exponent_plus_sign = 1 << 24,
        // ]

        // Partially canonicalize. Better use `canonical_c_style` or `canonical_cpp_style` to canonicalize fully.
        weakly_canonical_language_agnostic =
            force_no_trailing_return_type |
            force_comma_before_c_style_variadic |
            force_no_redundant_signed |
            force_no_type_prefix |
            numeric_literals_uppercase_digits |
            numeric_literals_strip_apostrophes |
            numeric_literals_force_zero_before_point |
            numeric_literals_force_zero_after_point |
            numeric_literals_no_zero_frac_before_exponent |
            numeric_literals_no_zero_exponent_after_frac |
            numeric_literals_no_exponent_useless_sign,

        // Force `(void)` for empty parameters.
        force_c_style_empty_params = 1 << 25,
        // Force `()` for empty parameters. Those two flags are incompatible.
        force_cpp_style_empty_params = 1 << 26,


        // Canonicalize the type for C. (Which works in C++ too.)
        canonical_c_style = weakly_canonical_language_agnostic | force_c_style_empty_params,
        // Canonicalize the type for C++.
        canonical_cpp_style = weakly_canonical_language_agnostic | force_cpp_style_empty_params,

        // ] End canonicalization.


        // Other stuff: [

        // Only makes sense for function types. Produces `(params...) -> R`.
        // I.e. forces the trailing return type even when `force_no_trailing_return_type` is set, and removes the leading `auto`.
        lambda = 1 << 27,

        // This is only for `Type`s. For other things this will result in an unpredictable behavior.
        // Causes only a half of the type to be emitted, either the left half or the right half. The identifier if any goes between them.
        only_left_half_type = 1 << 28,
        only_right_half_type = 1 << 29,

        // You shouldn't pass this, but you can use this to test any of the two bits above.
        mask_any_half_type = only_left_half_type | only_right_half_type,

        // Only those bits should be propagated to nested `ToCode()` calls.
        mask_propagate = ~(mask_any_half_type | lambda),
        // ]
    };
    CPPDECL_FLAG_OPERATORS(ToCodeFlags)

    enum class ToStringFlags
    {
        // Those are the different modes. Select at most one.
        // If none are specified, defaults to outputting a user-friendy string (which doesn't have its own flag).
        // [

        // Output the debug representation instead of the normal one.
        debug = 1 << 0,

        // Try to print as a single valid identifier. This of course can be lossy, this is just a best effort.
        // We try to keep those readable, rather than lossless.
        identifier = 1 << 1,

        // ]
    };
    CPPDECL_FLAG_OPERATORS(ToStringFlags)


    // If `user_friendly` is true, uses `restrict` instead of `__restrict`.
    [[nodiscard]] CPPDECL_CONSTEXPR std::string CvQualifiersToString(CvQualifiers quals, char sep = ' ', bool user_friendly = false)
    {
        std::string ret;

        bool first = true;
        auto quals_copy = quals;
        for (CvQualifiers bit{1}; bool(quals_copy); bit <<= 1)
        {
            if (bool(quals_copy & bit))
            {
                if (first)
                    first = false;
                else
                    ret += sep;

                quals_copy &= ~bit;

                switch (bit)
                {
                  case CvQualifiers::const_:
                    ret += "const";
                    continue;
                  case CvQualifiers::volatile_:
                    ret += "volatile";
                    continue;
                  case CvQualifiers::restrict_:
                    // `__restrict` is universal. MSVC chokes on `__restrict__`, and `restrict` is C-only (could be an extension in C++).
                    ret += user_friendly ? "restrict" : "__restrict";
                    continue;
                  case CvQualifiers::msvc_ptr32:
                    ret += "__ptr32";
                    continue;
                  case CvQualifiers::msvc_ptr64:
                    ret += "__ptr64";
                    continue;
                  case CvQualifiers::msvc_unaligned:
                    ret += "__unaligned";
                    continue;
                }
                assert(false && "Unknown enum.");
                ret += "??";
            }
        }
        return ret;
    }

    // Returns either `"a"` or `"an"` based on `input`, which is assumed to be returned by `CvQualifiersToString()`.
    // If the input string is empty, respects `default_an` (otherwise it's ignored).
    [[nodiscard]] CPPDECL_CONSTEXPR std::string_view ChooseAVsAnForCvQualifersString(std::string_view input, bool default_an)
    {
        // It's hard to generalize this, as it's based on the sounds rather than on the letters.
        if (!input.empty())
            default_an = input.starts_with("__unaligned");
        return default_an ? "an" : "a";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string_view RefQualifierToString(RefQualifier quals)
    {
        switch (quals)
        {
            case RefQualifier::none:   return "";
            case RefQualifier::lvalue: return "&";
            case RefQualifier::rvalue: return "&&";
        }
        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string_view SimpleTypePrefixToString(SimpleTypePrefix prefix)
    {
        switch (prefix)
        {
            case SimpleTypePrefix::none:      return "";
            case SimpleTypePrefix::struct_:   return "struct";
            case SimpleTypePrefix::class_:    return "class";
            case SimpleTypePrefix::union_:    return "union";
            case SimpleTypePrefix::enum_:     return "enum";
            case SimpleTypePrefix::typename_: return "typename";
        }
        assert(false && "Unknown enum.");
        return "??";
    }


    // Some declarations to break cyclic references: [
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const TemplateArgument &target, ToCodeFlags flags);
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const TemplateArgument &target, ToStringFlags flags);
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const Type &target, ToCodeFlags flags, std::size_t skip_first_modifiers = 0, CvQualifiers ignore_top_level_cv_quals = {});
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const Type &target, ToStringFlags flags);
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const SimpleType &target, ToCodeFlags flags, CvQualifiers ignore_cv_quals = {});
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const SimpleType &target, ToStringFlags flags);
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const TypeModifier &target, ToCodeFlags flags, CvQualifiers ignore_cv_quals = {});
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const TypeModifier &target, ToStringFlags flags);
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const PseudoExpr &target, ToCodeFlags flags);
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const PseudoExpr &target, ToStringFlags flags);
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const Attribute &target, ToCodeFlags flags);
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const Attribute &target, ToStringFlags flags);
    // ]


    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const TemplateArgumentList &target, ToCodeFlags flags)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));

        std::string ret = "<";

        bool first = true;
        for (const TemplateArgument &arg : target.args)
        {
            if (first)
            {
                first = false;
            }
            else
            {
                ret += ',';
                if (!bool(flags & ToCodeFlags::no_space_after_comma))
                    ret += ' ';
            }

            ret += ToCode(arg, flags);
        }

        ret += '>';
        BreakMaximumMunch(ret, 1);

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const TemplateArgumentList &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret;
            for (const TemplateArgument &arg : target.args)
            {
                if (!ret.empty())
                    ret += '_';

                ret += ToString(arg, flags);
            }
            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret = "[";
            bool first = true;
            for (const TemplateArgument &arg : target.args)
            {
                if (first)
                    first = false;
                else
                    ret += ',';

                ret += ToString(arg, flags);
            }
            ret += ']';
            return ret;
        }
        else
        {
            std::string ret;
            if (target.args.empty())
            {
                ret = "empty template arguments";
            }
            else
            {
                ret = NumberToString(target.args.size());
                ret += " template argument";
                if (target.args.size() != 1)
                    ret += 's';
                ret += ": [";

                std::size_t i = 0;
                for (const TemplateArgument &arg : target.args)
                {
                    if (i > 0)
                        ret += ", ";

                    i++;

                    if (target.args.size() != 1)
                    {
                        ret += NumberToString(i);
                        ret += ". ";
                    }

                    ret += ToString(arg, flags);
                }
                ret += ']';
            }
            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const UnqualifiedName::Variant &target, ToCodeFlags flags)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));

        std::string ret;
        std::visit(Overload{
            [&](const std::string &name)
            {
                ret = name;
            },
            [&](const OverloadedOperator &op)
            {
                ret = "operator";
                ret += op.token;
            },
            [&](const ConversionOperator &conv)
            {
                ret = "operator ";
                ret += ToCode(conv.target_type, flags);
            },
            [&](const UserDefinedLiteral &udl)
            {
                ret = "operator\"\"";
                if (udl.space_before_suffix)
                    ret += ' ';
                ret += udl.suffix;
            },
            [&](const DestructorName &dtor)
            {
                ret = '~';
                ret += ToCode(dtor.simple_type, flags);
            },
            [&](const NewDeleteOperator &op)
            {
                ret = "operator ";
                switch (op.kind)
                {
                  case NewDeleteOperator::Kind::new_:
                    ret += "new";
                    break;
                  case NewDeleteOperator::Kind::new_array:
                    ret += "new[]";
                    break;
                  case NewDeleteOperator::Kind::delete_:
                    ret += "delete";
                    break;
                  case NewDeleteOperator::Kind::delete_array:
                    ret += "delete[]";
                    break;
                  default:
                    assert(false);
                }
            },
            [&](const UnspellableName &unsp)
            {
                ret += unsp.name;
            },
        }, target);
        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const UnqualifiedName &target, ToCodeFlags flags)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));

        std::string ret = ToCode(target.var, flags);

        if (target.template_args)
            ret += ToCode(*target.template_args, flags);

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const UnqualifiedName::Variant &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret;

            std::visit(Overload{
                [&](const std::string &name)
                {
                    ret = KeepOnlyIdentifierChars(name);
                },
                [&](const OverloadedOperator &op)
                {
                    ret = "operator_";
                    ret += TokenToIdentifier(op.token, true);
                },
                [&](const ConversionOperator &conv)
                {
                    ret = "conversion_to_";
                    ret += ToString(conv.target_type, flags);
                },
                [&](const UserDefinedLiteral &udl)
                {
                    ret = "udl";
                    if (udl.suffix.starts_with('_'))
                    {
                        ret += udl.suffix;
                    }
                    else
                    {
                        ret += "_std_";
                        ret += udl.suffix;
                    }
                },
                [&](const DestructorName &dtor)
                {
                    ret = ToString(dtor.simple_type, flags);
                    if (!ret.empty())
                        ret += '_';
                    ret += "destructor";
                },
                [&](const NewDeleteOperator &op)
                {
                    ret = "operator_";
                    switch (op.kind)
                    {
                      case NewDeleteOperator::Kind::new_:
                        ret += "new";
                        break;
                      case NewDeleteOperator::Kind::new_array:
                        ret += "new_array";
                        break;
                      case NewDeleteOperator::Kind::delete_:
                        ret += "delete";
                        break;
                      case NewDeleteOperator::Kind::delete_array:
                        ret += "delete_array";
                        break;
                      default:
                        assert(false);
                    }
                },
                [&](const UnspellableName &unsp)
                {
                    ret += KeepOnlyIdentifierChars(unsp.name);
                },
            }, target);

            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret;

            std::visit(Overload{
                [&](const std::string &name)
                {
                    ret += "name=\"";
                    ret += name;
                    ret += '\"';
                },
                [&](const OverloadedOperator &op)
                {
                    ret += "op=`";
                    ret += op.token;
                    ret += '`';
                },
                [&](const ConversionOperator &conv)
                {
                    ret += "conv=`";
                    ret += ToString(conv.target_type, flags);
                    ret += '`';
                },
                [&](const UserDefinedLiteral &udl)
                {
                    ret += "udl=`";
                    ret += udl.suffix;
                    ret += '`';
                    if (udl.space_before_suffix)
                        ret += "(with space before suffix)";
                },
                [&](const DestructorName &dtor)
                {
                    ret += "dtor=`";
                    ret += ToString(dtor.simple_type, flags);
                    ret += '`';
                },
                [&](const NewDeleteOperator &op)
                {
                    ret += "new_del_op=`";
                    switch (op.kind)
                    {
                      case NewDeleteOperator::Kind::new_:
                        ret += "new";
                        break;
                      case NewDeleteOperator::Kind::new_array:
                        ret += "new[]";
                        break;
                      case NewDeleteOperator::Kind::delete_:
                        ret += "delete";
                        break;
                      case NewDeleteOperator::Kind::delete_array:
                        ret += "delete[]";
                        break;
                      default:
                        assert(false);
                    }
                    ret += '`';
                },
                [&](const UnspellableName &unsp)
                {
                    ret += "unsp=`";
                    ret += unsp.name;
                    ret += '`';
                },
            }, target);

            return ret;
        }
        else
        {
            std::string ret;

            std::visit(Overload{
                [&](const std::string &name)
                {
                    ret = '`';
                    ret += name;
                    ret += '`';
                },
                [&](const OverloadedOperator &op)
                {
                    ret = "overloaded operator `";
                    ret += op.token;
                    ret += '`';
                },
                [&](const ConversionOperator &conv)
                {
                    ret = "conversion operator to [";
                    ret += ToString(conv.target_type, flags);
                    ret += ']';
                },
                [&](const UserDefinedLiteral &udl)
                {
                    ret = "user-defined literal `";
                    ret += udl.suffix;
                    ret += '`';
                    if (udl.space_before_suffix)
                        ret += " (with deprecated space before suffix)";
                },
                [&](const DestructorName &dtor)
                {
                    ret = "destructor for type [";
                    ret += ToString(dtor.simple_type, flags);
                    ret += ']';
                },
                [&](const NewDeleteOperator &op)
                {
                    ret = "operator ";
                    switch (op.kind)
                    {
                      case NewDeleteOperator::Kind::new_:
                        ret += "new";
                        break;
                      case NewDeleteOperator::Kind::new_array:
                        ret += "new[]";
                        break;
                      case NewDeleteOperator::Kind::delete_:
                        ret += "delete";
                        break;
                      case NewDeleteOperator::Kind::delete_array:
                        ret += "delete[]";
                        break;
                      default:
                        assert(false);
                    }
                },
                [&](const UnspellableName &unsp)
                {
                    ret += "unspellable name `";
                    ret += unsp.name;
                    ret += '`';
                },
            }, target);

            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const UnqualifiedName &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret = ToString(target.var, flags);

            if (target.template_args)
            {
                std::string targs_str = ToString(*target.template_args, flags);
                if (!targs_str.empty() && !ret.empty())
                    ret += '_';
                ret += targs_str;
            }

            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret = "{";

            ret += ToString(target.var, flags);

            if (target.template_args)
            {
                ret += ",targs=";
                ret += ToString(*target.template_args, flags);
            }

            ret += '}';
            return ret;
        }
        else
        {
            std::string ret = ToString(target.var, flags);

            if (target.template_args)
            {
                ret += " with ";
                ret += ToString(*target.template_args, flags);
            }
            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const QualifiedName &target, ToCodeFlags flags)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));

        std::string ret;
        if (target.force_global_scope)
            ret = "::";

        bool first = true;
        for (const auto &part : target.parts)
        {
            if (first)
                first = false;
            else
                ret += "::";

            ret += ToCode(part, flags);
        }

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const QualifiedName &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret;
            if (target.force_global_scope)
                ret = "global_";
            for (const auto &part : target.parts)
            {
                if (!ret.empty())
                    ret += '_';

                ret += ToString(part, flags);
            }
            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret;
            ret += "{global_scope=";
            ret += target.force_global_scope ? "true" : "false";
            ret += ",parts=[";
            for (bool first = true; const auto &part : target.parts)
            {
                if (first)
                    first = false;
                else
                    ret += ',';

                ret += ToString(part, flags);
            }
            ret += "]}";
            return ret;
        }
        else
        {
            std::string ret;

            if (target.IsEmpty())
            {
                ret += "nothing";
            }
            else
            {
                if (target.force_global_scope)
                    ret += "::";

                bool first = true;
                for (const auto &part : target.parts)
                {
                    if (first)
                        first = false;
                    else
                        ret += "::";

                    ret += ToString(part, flags);
                }
            }

            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const AttributeList &target, ToCodeFlags flags)
    {
        std::string ret;

        bool first = true;

        for (const Attribute &attr : target.attrs)
        {
            if (first)
                first = false;
            else
                ret += ' ';

            ret += ToCode(attr, flags);
        }

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const AttributeList &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            // This does something, for completeness. But calling `ToString(..., identifier)` on a `SimpleType` will just ignore attirbutes.
            std::string ret;
            for (const auto &attr : target.attrs)
            {
                if (!ret.empty())
                    ret += '_';
                ret += ToString(attr, flags);
            }
            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret = "[";
            bool first = true;
            for (const auto &attr : target.attrs)
            {
                if (first)
                    first = false;
                else
                    ret += ",";

                ret += ToString(attr, flags);
            }
            ret += ']';
            return ret;
        }
        else
        {
            std::string ret;
            if (target.attrs.empty())
            {
                ret = "no attributes";
            }
            else
            {
                if (target.attrs.size() == 1)
                {
                    ret += ToString(target.attrs.front(), flags);
                }
                else
                {
                    ret += "attributes [";

                    bool first = true;
                    for (const Attribute &attr : target.attrs)
                    {
                        if (first)
                            first = false;
                        else
                            ret += ", ";

                        std::string attr_str = ToString(attr, flags);

                        constexpr std::string_view removed_part = "attribute ";
                        if (auto pos = attr_str.find(removed_part); pos != std::string::npos)
                        {
                            attr_str.erase(pos, removed_part.size());
                        }

                        ret += attr_str;
                    }
                    ret += ']';
                }
            }
            return ret;
        }
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const SimpleType &target, ToCodeFlags flags, CvQualifiers ignore_cv_quals /* ={} */)
    {
        std::string ret;

        auto WriteName = [&]
        {
            // Write attributes.
            if (!target.attrs.attrs.empty())
            {
                if (!ret.empty())
                    ret += ' ';
                ret += ToCode(target.attrs, flags);
            }

            if (!bool(flags & ToCodeFlags::force_no_type_prefix) && target.prefix != SimpleTypePrefix{})
            {
                if (!ret.empty())
                    ret += ' ';
                ret += SimpleTypePrefixToString(target.prefix);
            }

            if (bool(target.flags & SimpleTypeFlags::explicitly_signed) && (!bool(flags & ToCodeFlags::force_no_redundant_signed) || target.IsNonRedundantlySigned()))
            {
                if (!ret.empty())
                    ret += ' ';
                ret += "signed";
            }
            if (bool(target.flags & SimpleTypeFlags::unsigned_))
            {
                if (!ret.empty())
                    ret += ' ';
                ret += "unsigned";
            }
            if (bool(target.flags & SimpleTypeFlags::c_complex))
            {
                if (!ret.empty())
                    ret += ' ';
                ret += "_Complex";
            }
            if (bool(target.flags & SimpleTypeFlags::c_imaginary))
            {
                if (!ret.empty())
                    ret += ' ';
                ret += "_Imaginary";
            }

            if (bool(target.flags & SimpleTypeFlags::implied_int))
            {
                assert(target.name.AsSingleWord() == "int");
                assert(target.flags & (SimpleTypeFlags::unsigned_ | SimpleTypeFlags::explicitly_signed));
            }
            else if (bool(target.flags & SimpleTypeFlags::c_implied_double))
            {
                assert(target.name.AsSingleWord() == "double");
            }
            else
            {
                if (!ret.empty())
                    ret += ' ';
                ret += ToCode(target.name, flags);
            }

            if (bool(target.flags & SimpleTypeFlags::redundant_int))
            {
                if (!ret.empty())
                    ret += ' ';
                ret += "int";
            }

            // Don't need to handle `implied_int` here because that comes with the type being set to `"int"`.
        };

        auto WriteQuals = [&]
        {
            if (!ret.empty())
                ret += ' ';
            ret += CvQualifiersToString(target.quals & ~ignore_cv_quals);
        };

        if (bool(flags & ToCodeFlags::east_const))
        {
            WriteName();
            WriteQuals();
        }
        else
        {
            WriteQuals();
            WriteName();
        }

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const SimpleType &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret;

            // Not emitting `target.prefix` here, I don't think it's very useful?
            // Same for `attrs`.

            { // Flags.
                auto flags_copy = target.flags;
                for (SimpleTypeFlags bit{1}; bool(flags_copy); bit <<= 1)
                {
                    if (bool(flags_copy & bit))
                    {
                        flags_copy &= ~bit;

                        switch (bit)
                        {
                          case SimpleTypeFlags::unsigned_:
                            ret += "unsigned_";
                            continue;
                          case SimpleTypeFlags::explicitly_signed:
                            if (target.IsNonRedundantlySigned())
                                ret += "signed_";
                            continue;
                          case SimpleTypeFlags::redundant_int:
                            // We could handle this below separately, but we don't.
                            // I don't think this would add any useful information.
                            continue;
                          case SimpleTypeFlags::implied_int:
                            // This does nothing.
                            continue;
                          case SimpleTypeFlags::c_complex:
                            ret += "complex_";
                            continue;
                          case SimpleTypeFlags::c_imaginary:
                            ret += "imaginary_";
                            continue;
                          case SimpleTypeFlags::c_implied_double:
                            // This does nothing.
                            continue;
                        }
                        assert(false && "Unknown enum.");
                    }
                }
            }

            if (target.quals != CvQualifiers{})
            {
                ret += CvQualifiersToString(target.quals, '_', true);
                ret += '_';
            }

            ret += ToString(target.name, flags);

            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret = "{attrs=";
            ret += ToString(target.attrs, flags);
            ret += ",flags=[";
            { // Flags.
                bool first = true;
                auto flags_copy = target.flags;
                for (SimpleTypeFlags bit{1}; bool(flags_copy); bit <<= 1)
                {
                    if (bool(flags_copy & bit))
                    {
                        if (first)
                            first = false;
                        else
                            ret += ',';

                        flags_copy &= ~bit;

                        switch (bit)
                        {
                          case SimpleTypeFlags::unsigned_:
                            ret += "unsigned";
                            continue;
                          case SimpleTypeFlags::explicitly_signed:
                            ret += "explicitly_signed";
                            continue;
                          case SimpleTypeFlags::redundant_int:
                            ret += "redundant_int";
                            continue;
                          case SimpleTypeFlags::implied_int:
                            ret += "implied_int";
                            continue;
                          case SimpleTypeFlags::c_complex:
                            ret += "c_complex";
                            continue;
                          case SimpleTypeFlags::c_imaginary:
                            ret += "c_imaginary";
                            continue;
                          case SimpleTypeFlags::c_implied_double:
                            ret += "c_implied_double";
                            continue;
                        }
                        assert(false && "Unknown enum.");
                        ret += "??";
                    }
                }
                ret += "],";
            }

            if (target.prefix != SimpleTypePrefix{})
            {
                ret += "prefix=";
                ret += SimpleTypePrefixToString(target.prefix);
                ret += ',';
            }

            ret += "quals=[";
            ret += CvQualifiersToString(target.quals, ',', true);

            ret += "],name=";
            ret += ToString(target.name, flags);

            ret += '}';
            return ret;
        }
        else
        {
            std::string ret;

            if (target.quals != CvQualifiers{})
            {
                ret += CvQualifiersToString(target.quals, ' ', true);
                ret += ' ';
            }

            if (bool(target.flags & SimpleTypeFlags::unsigned_))
                ret += "unsigned ";
            if (bool(target.flags & SimpleTypeFlags::explicitly_signed))
                ret += target.IsNonRedundantlySigned() ? "signed " : "explicitly signed ";
            if (bool(target.flags & SimpleTypeFlags::c_complex))
                ret += "complex ";
            if (bool(target.flags & SimpleTypeFlags::c_imaginary))
                ret += "imaginary ";

            if (bool(target.flags & SimpleTypeFlags::implied_int))
            {
                assert(target.name.AsSingleWord() == "int");
                ret += "implied ";
            }
            if (bool(target.flags & SimpleTypeFlags::c_implied_double))
            {
                assert(target.name.AsSingleWord() == "double");
                ret += "implied ";
            }

            ret += ToString(target.name, flags);

            if (bool(target.flags & SimpleTypeFlags::redundant_int))
                ret += " with explicit `int`";

            if (target.prefix != SimpleTypePrefix{})
            {
                ret += ", explicitly a ";
                ret += SimpleTypePrefixToString(target.prefix);
            }

            if (!target.attrs.attrs.empty())
            {
                ret += ", with ";
                ret += ToString(target.attrs, flags);
            }

            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    // If `skip_first_modifiers > 0`, will skip several top-level (first) modifiers.
    // If `ignore_top_level_cv_quals` isn't zero, will ignore those cv-qualifiers of the first non-skipped modifier.
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const Type &target, ToCodeFlags flags, std::size_t skip_first_modifiers /* =0 */, CvQualifiers ignore_top_level_cv_quals /* ={} */)
    {
        assert(skip_first_modifiers <= target.modifiers.size());

        bool uses_trailing_return_type = false;
        bool lambda_style_trailing_return_type = false;
        if (bool(flags & ToCodeFlags::lambda))
        {
            assert(target.Is<Function>());
            uses_trailing_return_type = true;
            lambda_style_trailing_return_type = true;
        }

        // If `uses_trailing_return_type == false` this is equal to `modifiers.size()`.
        std::size_t trailing_return_type_start_index = skip_first_modifiers;

        if (uses_trailing_return_type)
        {
            trailing_return_type_start_index++;
        }
        else
        {
            while (trailing_return_type_start_index < target.modifiers.size())
            {
                if (!bool(flags & ToCodeFlags::force_no_trailing_return_type))
                {
                    if (auto func = std::get_if<Function>(&target.modifiers[trailing_return_type_start_index].var); func && func->uses_trailing_return_type)
                    {
                        uses_trailing_return_type = true;
                        trailing_return_type_start_index++;
                        break;
                    }
                }
                trailing_return_type_start_index++;
            }
        }

        std::string ret;
        if (!bool(flags & ToCodeFlags::only_right_half_type))
        {
            ret =
                lambda_style_trailing_return_type ? "" :
                uses_trailing_return_type ? "auto" :
                ToCode(target.simple_type, flags & ToCodeFlags::mask_propagate, target.modifiers.size() == skip_first_modifiers ? ignore_top_level_cv_quals : CvQualifiers{});
        }


        // Normally doesn't erase the space after `::*` unless `even_after_member_pointer == true`.
        auto MaybeErasePrecedingSpace = [&](bool even_after_member_pointer = false)
        {
            // Erase preceding space if needed.
            if (
                ret.ends_with(' ') &&
                (
                    even_after_member_pointer ||
                    // If there's a `:*` before the space, don't erase it. Member pointers look better with spaces.
                    ret.size() < 3 ||
                    ret[ret.size() - 2] != '*' ||
                    ret[ret.size() - 3] != ':'
                )
            )
            {
                ret.pop_back();
            }
        };


        std::size_t pos = trailing_return_type_start_index;

        // This goes deeper as we approach the center of the declaration.
        // This returns true if we need to stop the recursion because the rest is in a trailing return type.
        auto lambda = [&](auto &lambda) -> void
        {
            if (pos <= skip_first_modifiers)
                return;

            pos--;
            const bool is_top_level_modifier = pos <= skip_first_modifiers;

            const TypeModifier &m = target.modifiers[pos];

            const CvQualifiers ignored_cv_quals = pos == skip_first_modifiers ? ignore_top_level_cv_quals : CvQualifiers{};

            const bool spelled_after_identifier = m.SpelledAfterIdentifier();
            const bool need_parens =
                ( // Because we're switching from spelled-after-identifier to spelled-before-identifier modifiers.
                    pos >= skip_first_modifiers + 1 &&
                    spelled_after_identifier &&
                    !target.modifiers[pos-1].SpelledAfterIdentifier()
                ) ||
                ( // Because this is a member pointer with a class name that starts with `::`.
                    // This is ambiguous if the target type name isn't a keyword, and needs the parentheses to disambiguate.
                    // But we just add them unconditionally. Relying on keywords rejecting `::` after them sounds jank.
                    std::get_if<MemberPointer>(&m.var) && std::get<MemberPointer>(m.var).base.force_global_scope
                )
                ;

            if (!bool(flags & ToCodeFlags::only_right_half_type))
            {
                if (need_parens)
                {
                    // Space before?
                    // This one is unconditional for now, we could add a flag.
                    if (!ret.empty() && IsIdentifierChar(ret.back()))
                        ret += ' ';

                    ret += '(';
                }

                if (!spelled_after_identifier)
                {
                    MaybeErasePrecedingSpace(std::holds_alternative<MemberPointer>(m.var));

                    // Space before?
                    if (
                        std::holds_alternative<MemberPointer>(m.var)
                        ? !ret.ends_with('(')
                        : (
                            !bool(flags & ToCodeFlags::no_space_before_pointer) &&
                            !ret.empty() && !ret.ends_with('*') && !ret.ends_with('&') && !ret.ends_with('(') && !ret.ends_with(' ')
                        )
                    )
                    {
                        ret += ' ';
                    }

                    ret += ToCode(m, flags & ToCodeFlags::mask_propagate, ignored_cv_quals);

                    // Space after?
                    if (std::holds_alternative<MemberPointer>(m.var) || bool(flags & ToCodeFlags::add_space_after_pointer))
                        ret += ' ';
                }
            }

            lambda(lambda);

            if (!bool(flags & ToCodeFlags::only_left_half_type))
            {
                if (need_parens)
                {
                    MaybeErasePrecedingSpace(true);
                    ret += ')';
                }

                MaybeErasePrecedingSpace();

                if (spelled_after_identifier)
                {
                    ret += ToCode(m, flags & ToCodeFlags::mask_propagate, ignored_cv_quals);

                    // Inject our own `->` for forced trailing return type, if needed.
                    if (lambda_style_trailing_return_type && is_top_level_modifier && m.Is<Function>() && (!m.As<Function>()->uses_trailing_return_type || bool(flags & ToCodeFlags::force_no_trailing_return_type)))
                        ret += " -> ";
                }
            }
        };
        lambda(lambda);

        if (uses_trailing_return_type)
        {
            if (!bool(flags & ToCodeFlags::only_left_half_type))
                ret += ToCode(target, flags & ToCodeFlags::mask_propagate, trailing_return_type_start_index);
        }
        else
        {
            // This is in the `else` as a little optimization.
            if (!bool(flags & ToCodeFlags::only_left_half_type))
                MaybeErasePrecedingSpace(true);
        }

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const Type &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret = ToString(target.simple_type, flags);
            for (std::size_t i = target.modifiers.size(); i-- > 0;)
            {
                if (!ret.empty())
                    ret += '_';
                ret += ToString(target.modifiers[i], flags);
            }
            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret;
            for (const TypeModifier &mod : target.modifiers)
            {
                ret += ToString(mod, flags);
                ret += ' ';
            }
            ret += ToString(target.simple_type, flags);
            return ret;
        }
        else
        {
            std::string ret;
            if (target.IsEmpty())
            {
                ret += "no type";
            }
            else
            {
                for (const TypeModifier &mod : target.modifiers)
                {
                    ret += ToString(mod, flags);
                    ret += ' ';
                }
            }
            ret += ToString(target.simple_type, flags);
            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const PunctuationToken &target, ToCodeFlags flags)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));
        (void)flags;

        return target.value;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const PunctuationToken &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            return std::string(TokenToIdentifier(target.value, true));
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            return "punct`" + target.value + "`";
        }
        else
        {
            return "punctuation `" + target.value + "`";
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const NumericLiteral &target, ToCodeFlags flags)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));

        std::string ret;

        auto AppendInteger = [&](std::string_view input)
        {
            assert(bool(flags & ToCodeFlags::numeric_literals_uppercase_digits) + bool(flags & ToCodeFlags::numeric_literals_lowercase_digits) <= 1);

            if (bool(flags & ToCodeFlags::numeric_literals_strip_apostrophes))
            {
                for (char ch : input)
                {
                    if (ch == '\'')
                        continue;

                    if (bool(flags & ToCodeFlags::numeric_literals_uppercase_digits))
                        ret += ToUpper(ch);
                    else if (bool(flags & ToCodeFlags::numeric_literals_lowercase_digits))
                        ret += ToLower(ch);
                    else
                        ret += ch;
                }
            }
            else
            {
                ret += input;
            }
        };

        auto EmptyOrAllZeroes = [](std::string_view input) -> bool
        {
            for (char ch : input)
            {
                if (ch != '0' && ch != '\'')
                    return false;
            }
            return true;
        };

        auto ExpEmptyOrAllZeroes = [&](std::string_view input) -> bool
        {
            (void)(ConsumePunctuation(input, "+") || ConsumePunctuation(input, "-"));
            return EmptyOrAllZeroes(input);
        };

        std::visit(Overload{
            [&](const NumericLiteral::Integer &i)
            {
                switch (i.base)
                {
                    case NumericLiteral::Integer::Base::decimal: break; // Nothing.
                    case NumericLiteral::Integer::Base::binary:  ret += bool(flags & ToCodeFlags::numeric_literals_uppercase_prefix) ? "0B" : "0b"; break;
                    case NumericLiteral::Integer::Base::octal:   ret += '0'; break;
                    case NumericLiteral::Integer::Base::hex:     ret += bool(flags & ToCodeFlags::numeric_literals_uppercase_prefix) ? "0X" : "0x"; break;
                }

                AppendInteger(i.value);

                // Suffix.
                std::visit(Overload{
                    [&](std::string_view str)
                    {
                        ret += str;
                    },
                    [&](const NumericLiteral::Integer::Suffix &suffix)
                    {
                        const bool trailing_unsigned = bool(flags & ToCodeFlags::numeric_literals_suffix_unsigned_last);
                        const bool caps = bool(flags & ToCodeFlags::numeric_literals_uppercase_suffix);
                        if (suffix.is_unsigned && !trailing_unsigned)
                            ret += caps ? 'U' : 'u';

                        switch (suffix.signed_part)
                        {
                            case NumericLiteral::Integer::SignedSuffix::none: break; // Nothing.
                            case NumericLiteral::Integer::SignedSuffix::l:    ret += caps ? "L"  : "l";  break;
                            case NumericLiteral::Integer::SignedSuffix::ll:   ret += caps ? "LL" : "ll"; break;
                            case NumericLiteral::Integer::SignedSuffix::z:    ret += caps ? "Z"  : "z";  break;
                        }

                        if (suffix.is_unsigned && trailing_unsigned)
                            ret += caps ? 'U' : 'u';
                    },
                }, i.suffix);
            },
            [&](const NumericLiteral::FloatingPoint &f)
            {
                const bool is_hex = f.base == NumericLiteral::FloatingPoint::Base::hex;
                if (is_hex)
                    ret += bool(flags & ToCodeFlags::numeric_literals_uppercase_prefix) ? "0X" : "0x";

                // Make sure we don't have conflicting canonicalizaiton flags for the integral part.
                assert(bool(flags & ToCodeFlags::numeric_literals_force_zero_before_point) + bool(flags & ToCodeFlags::numeric_literals_no_zero_before_point) <= 1);

                // Integral part.
                if (bool(flags & ToCodeFlags::numeric_literals_force_zero_before_point) && f.value_int.empty()) // Sic, not using `EmptyOrAllZeroes()`.
                    ret += '0';
                else if (bool(flags & ToCodeFlags::numeric_literals_no_zero_before_point) && EmptyOrAllZeroes(f.value_int))
                    ; // Nothing.
                else
                    AppendInteger(f.value_int);

                // Make sure we don't have conflicting canonicalizaiton flags.
                assert(bool(flags & ToCodeFlags::numeric_literals_force_zero_after_point) + bool(flags & ToCodeFlags::numeric_literals_no_zero_after_point) + bool(flags & ToCodeFlags::numeric_literals_no_zero_frac) <= 1); // Sic. Only `no_zero_frac` conflicts with the other two, while `no_zero_frac_before_exponent` doesn't.
                assert(bool(flags & ToCodeFlags::numeric_literals_no_zero_frac_before_exponent) + bool(flags & ToCodeFlags::numeric_literals_no_zero_frac) <= 1);
                assert(bool(flags & ToCodeFlags::numeric_literals_no_zero_exponent_after_frac) + bool(flags & ToCodeFlags::numeric_literals_no_zero_exponent) <= 1);

                const bool have_exp_tentative = is_hex || (bool(flags & ToCodeFlags::numeric_literals_no_zero_exponent) || (bool(flags & ToCodeFlags::numeric_literals_no_zero_exponent_after_frac) && f.value_frac) ? !ExpEmptyOrAllZeroes(f.value_exp) : !f.value_exp.empty());

                const bool have_frac =
                    f.value_frac &&
                    !(
                        (
                            bool(flags & ToCodeFlags::numeric_literals_no_zero_frac) ||
                            (bool(flags & ToCodeFlags::numeric_literals_no_zero_frac_before_exponent) && have_exp_tentative)
                        ) &&
                        EmptyOrAllZeroes(*f.value_frac)
                    );

                const bool have_exp = have_exp_tentative || (bool(flags & ToCodeFlags::numeric_literals_no_zero_exponent_after_frac) && !have_frac && !f.value_exp.empty() && ExpEmptyOrAllZeroes(f.value_exp));

                // The fractional part (or at least the decimal point).
                if (have_frac)
                {
                    ret += '.';

                    if (bool(flags & ToCodeFlags::numeric_literals_force_zero_after_point) && f.value_frac->empty()) // Sic, not using `EmptyOrAllZeroes()`.
                        ret += '0';
                    else if (bool(flags & ToCodeFlags::numeric_literals_no_zero_after_point) && EmptyOrAllZeroes(*f.value_frac))
                        ; // Nothing.
                    else
                        AppendInteger(*f.value_frac);
                }

                // Exponent.
                if (have_exp)
                {
                    if (is_hex)
                        ret += bool(flags & ToCodeFlags::numeric_literals_uppercase_exponent) ? 'P' : 'p';
                    else
                        ret += bool(flags & ToCodeFlags::numeric_literals_uppercase_exponent) ? 'E' : 'e';

                    // Make sure we don't have conflicting canonicalizaiton flags for the exponent.
                    assert(bool(flags & ToCodeFlags::numeric_literals_force_exponent_plus_sign) + bool(flags & ToCodeFlags::numeric_literals_no_exponent_useless_sign) <= 1);

                    std::string_view exp_view = f.value_exp;
                    if (bool(flags & ToCodeFlags::numeric_literals_force_exponent_plus_sign))
                    {
                        // Flip the sign on negative zero exponents.
                        if (exp_view.starts_with('-'))
                        {
                            if (EmptyOrAllZeroes(exp_view.substr(1)))
                            {
                                ret += '+';
                                exp_view.remove_prefix(1);
                            }
                        }
                        else if (!exp_view.starts_with('+'))
                            ret += '+';
                    }
                    else if (bool(flags & ToCodeFlags::numeric_literals_no_exponent_useless_sign))
                    {
                        if (f.value_exp.starts_with('+') || (f.value_exp.starts_with('-') && EmptyOrAllZeroes(f.value_exp.substr(1))))
                            exp_view.remove_prefix(1);
                    }

                    AppendInteger(exp_view);
                }

                // Suffix.
                std::visit(Overload{
                    [&](std::string_view str)
                    {
                        ret += str;
                    },
                    [&](NumericLiteral::FloatingPoint::Suffix suffix)
                    {
                        const bool caps = bool(flags & ToCodeFlags::numeric_literals_uppercase_suffix);

                        switch (suffix)
                        {
                            case NumericLiteral::FloatingPoint::Suffix::f:    ret += caps ? "F"    : "f";    break;
                            case NumericLiteral::FloatingPoint::Suffix::l:    ret += caps ? "L"    : "l";    break;
                            case NumericLiteral::FloatingPoint::Suffix::f16:  ret += caps ? "F16"  : "f16";  break;
                            case NumericLiteral::FloatingPoint::Suffix::f32:  ret += caps ? "F32"  : "f32";  break;
                            case NumericLiteral::FloatingPoint::Suffix::f64:  ret += caps ? "F64"  : "f64";  break;
                            case NumericLiteral::FloatingPoint::Suffix::f128: ret += caps ? "F128" : "f128"; break;
                            case NumericLiteral::FloatingPoint::Suffix::bf16: ret += caps ? "BF16" : "bf16"; break;
                        }
                    },
                }, f.suffix);
            },
        }, target.var);

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const NumericLiteral &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            // For integers, first try to dump the decimal value.
            if (target.IsInteger())
            {
                if (auto opt = target.ToInteger())
                {
                    std::string ret = NumberToString(*opt);
                    // Keep the custom suffix if any.
                    if (auto suffix = std::get_if<std::string>(&std::get<NumericLiteral::Integer>(target.var).suffix))
                        ret += *suffix;
                    return ret;
                }
            }

            // On failure, or if not an integer, dump the value as is.
            std::string str = ToCode(target, ToCodeFlags::weakly_canonical_language_agnostic);

            std::string ret;
            for (char ch : str)
            {
                if (IsIdentifierChar(ch))
                    ret += ch;
                else if (ch == '.')
                    ret += "point";
                else if (ch == '+')
                    ret += "plus"; // For the exponent.
                else if (ch == '-')
                    ret += "minus"; // For the exponent.
            }

            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret;

            std::visit(Overload{
                [&](const NumericLiteral::Integer &i)
                {
                    ret += "int{base=";

                    switch (i.base)
                    {
                        case NumericLiteral::Integer::Base::decimal: ret += "10"; break;
                        case NumericLiteral::Integer::Base::binary:  ret += "2"; break;
                        case NumericLiteral::Integer::Base::octal:   ret += "8"; break;
                        case NumericLiteral::Integer::Base::hex:     ret += "16"; break;
                    }

                    ret += ",value=`";
                    ret += i.value;
                    ret += "`,suffix=";

                    std::visit(Overload{
                        [&](std::string_view str)
                        {
                            if (str.empty())
                            {
                                ret += "none";
                            }
                            else
                            {
                                ret += "udl`";
                                ret += str;
                                ret += "`";
                            }
                        },
                        [&](const NumericLiteral::Integer::Suffix &suffix)
                        {
                            if (suffix.is_unsigned)
                                ret += "u+";
                            switch (suffix.signed_part)
                            {
                                case NumericLiteral::Integer::SignedSuffix::none: ret.pop_back(); break; // Remove the trailing `+`.
                                case NumericLiteral::Integer::SignedSuffix::l: ret += "l"; break;
                                case NumericLiteral::Integer::SignedSuffix::ll: ret += "ll"; break;
                                case NumericLiteral::Integer::SignedSuffix::z: ret += "z"; break;
                            }
                        },
                    }, i.suffix);

                    ret += '}';
                },
                [&](const NumericLiteral::FloatingPoint &f)
                {
                    ret += "float{base=";

                    switch (f.base)
                    {
                        case NumericLiteral::FloatingPoint::Base::decimal: ret += "10"; break;
                        case NumericLiteral::FloatingPoint::Base::hex:     ret += "16"; break;
                    }

                    ret += ",int=`";
                    ret += f.value_int;
                    ret += "`,frac=";
                    if (f.value_frac)
                    {
                        ret += '`';
                        ret += *f.value_frac;
                        ret += '`';
                    }
                    else
                    {
                        ret += "none";
                    }
                    ret += ",exp=`";
                    ret += f.value_exp;
                    ret += "`,suffix=";

                    std::visit(Overload{
                        [&](std::string_view str)
                        {
                            if (str.empty())
                            {
                                ret += "none";
                            }
                            else
                            {
                                ret += "udl`";
                                ret += str;
                                ret += "`";
                            }
                        },
                        [&](NumericLiteral::FloatingPoint::Suffix suffix)
                        {
                            switch (suffix)
                            {
                                case NumericLiteral::FloatingPoint::Suffix::f:    ret += "f"; break;
                                case NumericLiteral::FloatingPoint::Suffix::l:    ret += "l"; break;
                                case NumericLiteral::FloatingPoint::Suffix::f16:  ret += "f16"; break;
                                case NumericLiteral::FloatingPoint::Suffix::f32:  ret += "f32"; break;
                                case NumericLiteral::FloatingPoint::Suffix::f64:  ret += "f64"; break;
                                case NumericLiteral::FloatingPoint::Suffix::f128: ret += "f128"; break;
                                case NumericLiteral::FloatingPoint::Suffix::bf16: ret += "bf16"; break;
                            }
                        },
                    }, f.suffix);

                    ret += '}';
                },
            }, target.var);

            return ret;
        }
        else
        {
            std::string ret;

            auto AppendInteger = [&](std::string_view input)
            {
                for (char ch : input)
                {
                    if (ch != '\'')
                        ret += ch;
                }
            };

            auto EmptyOrAllZeroes = [](std::string_view input) -> bool
            {
                for (char ch : input)
                {
                    if (ch != '0' && ch != '\'')
                        return false;
                }
                return true;
            };

            std::visit(Overload{
                [&](const NumericLiteral::Integer &i)
                {
                    const bool empty_or_all_zeroes = EmptyOrAllZeroes(i.value);

                    switch (i.base)
                    {
                        case NumericLiteral::Integer::Base::decimal: break; // Nothing?
                        case NumericLiteral::Integer::Base::binary:  ret += "binary "; break;
                        case NumericLiteral::Integer::Base::octal:   if (!empty_or_all_zeroes) ret += "octal "; break;
                        case NumericLiteral::Integer::Base::hex:     ret += "hexadecimal "; break;
                    }

                    ret += "integer ";

                    if (empty_or_all_zeroes)
                        ret += '0'; // This will be empty for `0`, which is an octal literal with an empty value.
                    else
                        AppendInteger(i.value);

                    if (i.base != NumericLiteral::Integer::Base::decimal && !empty_or_all_zeroes)
                    {
                        if (auto opt = target.ToInteger())
                        {
                            ret += " (decimal ";
                            ret += NumberToString(*opt);
                            ret += ")";
                        }
                    }

                    std::visit(Overload{
                        [&](std::string_view str)
                        {
                            if (!str.empty())
                            {
                                ret += " with user-defined suffix `";
                                ret += str;
                                ret += "`";
                            }
                        },
                        [&](const NumericLiteral::Integer::Suffix &suffix)
                        {
                            ret += " with preferred type `"; // This is merely "preferred" because it can be automatically upgraded to a larger type.
                            if (suffix.signed_part == NumericLiteral::Integer::SignedSuffix::z)
                            {
                                ret += suffix.is_unsigned ? "size_t" : "ptrdiff_t";
                            }
                            else
                            {
                                if (suffix.is_unsigned)
                                    ret += "unsigned ";
                                switch (suffix.signed_part)
                                {
                                    case NumericLiteral::Integer::SignedSuffix::none: ret += "int"; break; // I guess?
                                    case NumericLiteral::Integer::SignedSuffix::l:    ret += "long"; break;
                                    case NumericLiteral::Integer::SignedSuffix::ll:   ret += "long long"; break;
                                    case NumericLiteral::Integer::SignedSuffix::z:    break; // Was already handled above.
                                }
                            }
                            ret += '`';
                        },
                    }, i.suffix);
                },
                [&](const NumericLiteral::FloatingPoint &f)
                {
                    if (f.base == NumericLiteral::FloatingPoint::Base::hex)
                        ret += "hexadecimal ";

                    ret += "floating-point number ";

                    // Integral part.
                    if (EmptyOrAllZeroes(f.value_int))
                    {
                        ret += '0'; // This looks better.
                    }
                    else
                    {
                        if (f.base == NumericLiteral::FloatingPoint::Base::hex)
                            ret += "0x"; // This makes things more clear.

                        AppendInteger(f.value_int);
                    }

                    if (f.value_frac && !EmptyOrAllZeroes(*f.value_frac))
                    {
                        ret += '.';
                        if (f.base == NumericLiteral::FloatingPoint::Base::hex)
                            ret += "0x"; // This makes things more clear.

                        AppendInteger(*f.value_frac);
                    }

                    // Exponent.
                    if (!EmptyOrAllZeroes(f.value_exp))
                    {
                        ret += " * ";
                        if (f.base == NumericLiteral::FloatingPoint::Base::hex)
                            ret += "2^"; // Sic, not `16^`.
                        else
                            ret += "10^";
                        AppendInteger(f.value_exp);
                    }

                    // Suffix.
                    std::visit(Overload{
                        [&](std::string_view str)
                        {
                            if (!str.empty())
                            {
                                ret += " with user-defined suffix `";
                                ret += str;
                                ret += "`";
                            }
                        },
                        [&](NumericLiteral::FloatingPoint::Suffix suffix)
                        {
                            ret += " of type `";

                            switch (suffix)
                            {
                                case NumericLiteral::FloatingPoint::Suffix::f:    ret += "float"; break;
                                case NumericLiteral::FloatingPoint::Suffix::l:    ret += "long double"; break;
                                case NumericLiteral::FloatingPoint::Suffix::f16:  ret += "std::float16_t";  break;
                                case NumericLiteral::FloatingPoint::Suffix::f32:  ret += "std::float32_t";  break;
                                case NumericLiteral::FloatingPoint::Suffix::f64:  ret += "std::float64_t";  break;
                                case NumericLiteral::FloatingPoint::Suffix::f128: ret += "std::float128_t"; break;
                                case NumericLiteral::FloatingPoint::Suffix::bf16: ret += "std::bfloat16_t"; break;
                            }
                            ret += '`';
                        },
                    }, f.suffix);
                },
            }, target.var);

            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const StringOrCharLiteral &target, ToCodeFlags flags)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));
        (void)flags;

        std::string ret;

        switch (target.type)
        {
            case StringOrCharLiteral::Type::normal: break;
            case StringOrCharLiteral::Type::wide:   ret += "L"; break;
            case StringOrCharLiteral::Type::u8:     ret += "u8"; break;
            case StringOrCharLiteral::Type::u16:    ret += "u"; break;
            case StringOrCharLiteral::Type::u32:    ret += "U"; break;
        }

        switch (target.kind)
        {
          case StringOrCharLiteral::Kind::character:
            ret += '\'';
            break;
          case StringOrCharLiteral::Kind::string:
            ret += '"';
            break;
          case StringOrCharLiteral::Kind::raw_string:
            ret += "R\"";
            ret += target.raw_string_delim;
            ret += '(';
            break;
        }

        ret += target.value;

        switch (target.kind)
        {
          case StringOrCharLiteral::Kind::character:
            ret += '\'';
            break;
          case StringOrCharLiteral::Kind::string:
            ret += '"';
            break;
          case StringOrCharLiteral::Kind::raw_string:
            ret += ')';
            ret += target.raw_string_delim;
            ret += '"';
            break;
        }

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const StringOrCharLiteral &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            // Not emitting the type and literal kind here. Hopefully they aren't very useful?
            return KeepOnlyIdentifierChars(target.value);
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret;
            switch (target.kind)
            {
                case StringOrCharLiteral::Kind::character:  ret += "char`"; break;
                case StringOrCharLiteral::Kind::string:     ret += "str`"; break;
                case StringOrCharLiteral::Kind::raw_string: ret += "rawstr`"; break;
            }
            ret += target.value;
            ret += '`';
            if (!target.literal_suffix.empty())
            {
                ret += "(suffix`";
                ret += target.literal_suffix;
                ret += "`)";
            }
            if (!target.raw_string_delim.empty())
            {
                ret += "(delim`";
                ret += target.raw_string_delim;
                ret += "`)";
            }
            return ret;
        }
        else
        {
            std::string ret;
            switch (target.kind)
            {
                case StringOrCharLiteral::Kind::character:  ret += "character `"; break;
                case StringOrCharLiteral::Kind::string:     ret += "string `"; break;
                case StringOrCharLiteral::Kind::raw_string: ret += "raw string `"; break;
            }
            ret += target.value;
            ret += '`';
            if (!target.literal_suffix.empty())
            {
                ret += "with suffix `";
                ret += target.literal_suffix;
                ret += "`";
            }
            if (!target.raw_string_delim.empty())
            {
                ret += "with delimiter `";
                ret += target.raw_string_delim;
                ret += "`";
            }
            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const PseudoExprList &target, ToCodeFlags flags)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));

        std::string ret;

        switch (target.kind)
        {
            case PseudoExprList::Kind::parentheses: ret += '('; break;
            case PseudoExprList::Kind::curly:       ret += '{'; break;
            case PseudoExprList::Kind::square:      ret += '['; break;
        }

        bool first = true;
        for (const PseudoExpr &elem : target.elems)
        {
            if (first)
            {
                first = false;
            }
            else
            {
                ret += ',';
                if (!bool(flags & ToCodeFlags::no_space_after_comma))
                    ret += ' ';
            }

            ret += ToCode(elem, flags);
        }

        switch (target.kind)
        {
            case PseudoExprList::Kind::parentheses: ret += ')'; break;
            case PseudoExprList::Kind::curly:       ret += '}'; break;
            case PseudoExprList::Kind::square:      ret += ']'; break;
        }

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const PseudoExprList &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret;
            for (const auto &elem : target.elems)
            {
                if (!ret.empty())
                    ret += '_';
                ret += ToString(elem, flags);
            }
            return ret;
        }

        const char *braces = nullptr;
        switch (target.kind)
        {
            case PseudoExprList::Kind::parentheses: braces = "()"; break;
            case PseudoExprList::Kind::curly:       braces = "{}"; break;
            case PseudoExprList::Kind::square:      braces = "[]"; break;
        }

        if (bool(flags & ToStringFlags::debug))
        {
            std::string ret = "list";
            ret += braces[0];

            bool first = true;
            for (const auto &elem : target.elems)
            {
                if (first)
                    first = false;
                else
                    ret += ',';

                ret += ToString(elem, flags);
            }

            ret += braces[1];

            if (target.has_trailing_comma)
                ret += "(has trailing comma)";
            return ret;
        }
        else
        {
            std::string ret = "list ";
            ret += braces[0];

            bool first = true;
            for (const auto &elem : target.elems)
            {
                if (first)
                    first = false;
                else
                    ret += ", ";

                ret += ToString(elem, flags);
            }
            if (target.has_trailing_comma)
                ret += ",";

            ret += braces[1];

            if (target.has_trailing_comma)
                ret += " with trailing comma";
            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const PseudoExpr &target, ToCodeFlags flags)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));

        std::string ret;

        for (const auto &token : target.tokens)
        {
            std::string elem_str = std::visit([&](const auto &elem){return ToCode(elem, flags);}, token);

            // Separating whitespace between identifiers.
            bool need_separating_whitespace = !ret.empty() && !elem_str.empty() && IsIdentifierChar(ret.back()) && IsIdentifierChar(elem_str.front());
            if (need_separating_whitespace)
                ret += ' ';

            ret += elem_str;


            // Avoid maximum munch by inserting whitespace between tokens.
            if (!need_separating_whitespace)
                BreakMaximumMunch(ret, elem_str.size());
        }

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const PseudoExpr &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret;
            for (const auto &token : target.tokens)
            {
                if (!ret.empty())
                    ret += '_';
                ret += std::visit([&](const auto &elem){return ToString(elem, flags);}, token);
            }
            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret = "[";
            bool first = true;
            for (const auto &token : target.tokens)
            {
                if (first)
                    first = false;
                else
                    ret += ',';

                ret += std::visit([&](const auto &elem){return ToString(elem, flags);}, token);
            }
            ret += ']';
            return ret;
        }
        else
        {
            std::string ret = "[";
            bool first = true;
            for (const auto &token : target.tokens)
            {
                if (first)
                    first = false;
                else
                    ret += ", ";

                ret += std::visit([&](const auto &elem){return ToString(elem, flags);}, token);
            }
            ret += ']';
            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const Decl &target, ToCodeFlags flags)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));

        std::string ret;

        if (target.name.IsEmpty())
        {
            // Purely an optimization, to avoid assmebling the type from two halves.
            ret = ToCode(target.type, flags);
        }
        else
        {
            ret = ToCode(target.type, flags | ToCodeFlags::only_left_half_type);

            std::string name_str = ToCode(target.name, flags);

            // Separating whitespace if needed.
            if (!ret.empty() && !name_str.empty() && IsIdentifierChar(ret.back()) && IsIdentifierChar(name_str.front()))
                ret += ' ';
            ret += name_str;

            ret += ToCode(target.type, flags | ToCodeFlags::only_right_half_type);
        }

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const Decl &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret = ToString(target.type, flags);
            std::string name = ToString(target.name, flags);

            if (!ret.empty() && !name.empty())
                ret += '_';

            ret += name;

            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret = "{type=\"";
            ret += ToString(target.type, flags);
            ret += "\",name=\"";
            ret += ToString(target.name, flags);
            ret += "\"}";
            return ret;
        }
        else
        {
            std::string ret;
            std::string type_str = ToString(target.type, flags);
            std::string_view type_view = type_str;

            // If this is a function type that returns nothing, adjust it to say "a constructor" instead of "a function".
            if (
                target.type.simple_type.IsEmpty() && !target.type.modifiers.empty() &&
                std::holds_alternative<Function>(target.type.modifiers.front().var) &&
                target.name.LastComponentIsNormalString() &&
                ConsumeWord(type_view, "a function")
            )
            {
                constexpr std::string_view suffix = ", returning nothing";
                if (type_view.ends_with(suffix))
                    type_view.remove_suffix(suffix.size());
                else
                    assert(false); // Hmm.

                std::string new_str = "a constructor";
                new_str += type_view;
                type_str = std::move(new_str);
                type_view = type_str;
            }

            // Similarly for destructors.
            if (
                target.type.simple_type.IsEmpty() && !target.type.modifiers.empty() &&
                std::holds_alternative<Function>(target.type.modifiers.front().var) &&
                target.name.IsDestructorName() &&
                ConsumeWord(type_view, "a function")
            )
            {
                constexpr std::string_view suffix = ", returning nothing";
                if (type_view.ends_with(suffix))
                    type_view.remove_suffix(suffix.size());
                else
                    assert(false); // Hmm.

                std::string new_str = "a destructor";
                new_str += type_view;
                type_str = std::move(new_str);
                type_view = type_str;
            }

            if (target.name.IsEmpty())
            {
                ret += "unnamed";
                if (ConsumeWord(type_view, "a") || ConsumeWord(type_view, "an"))
                {
                    // If the type starts with `a `, remove that.
                    ret += type_view;
                }
                else if (target.type.IsEmpty())
                {
                    ret += " with no type"; // This shouldn't happen?
                }
                else
                {
                    ret += " of type ";
                    ret += type_str;
                }
            }
            else
            {
                ret += ToString(target.name, flags);
                if (StartsWithWord(type_str, "a") || StartsWithWord(type_str, "an"))
                {
                    ret += ", ";
                    ret += type_str;
                }
                else if (target.type.IsEmpty())
                {
                    if (target.name.IsConversionOperatorName() || target.name.IsDestructorName())
                    {
                        // Nothing.
                    }
                    else if (target.name.LastComponentIsNormalString())
                    {
                        ret += ", a constructor without a parameter list"; // Right?
                    }
                    else
                    {
                        ret += " with no type"; // This shouldn't happen?
                    }
                }
                else
                {
                    ret += " of type ";
                    ret += type_str;
                }
            }

            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const TemplateArgument &target, ToCodeFlags flags)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));

        return std::visit([&](const auto &elem){return ToCode(elem, flags);}, target.var);
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const TemplateArgument &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            return std::visit([&](const auto &type){return ToString(type, flags);}, target.var);
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            return std::visit(Overload{
                [&](const Type &type){return "type:" + ToString(type, flags);},
                [&](const PseudoExpr &expr){return "expr" + ToString(expr, flags);},
            }, target.var);
        }
        else
        {
            return std::visit(Overload{
                [&](const Type &type)
                {
                    std::string type_str = ToString(type, flags);
                    std::string_view type_view = type_str;
                    (void)ConsumePunctuation(type_view, "type "); // Remove the word "type", if any.

                    std::string ret = "possibly type: ";
                    ret += type_view;
                    return ret;
                },
                [&](const PseudoExpr &expr)
                {
                    return "non-type: " + ToString(expr, flags);
                },
            }, target.var);
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    // `MaybeAmbiguous` doesn't need a custom `ToCode()` here we don't want to print ambiguities there.
    // So only `ToString()`.
    template <typename T>
    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const MaybeAmbiguous<T> &target, ToStringFlags flags)
    {
        // For identifiers, don't show the ambiguous alternatives.
        if (bool(flags & ToStringFlags::identifier))
            return ToString(static_cast<const T &>(target), flags);

        if (!target.ambiguous_alternative)
        {
            return ToString(static_cast<const T &>(target), flags);
        }
        else
        {
            if (bool(flags & ToStringFlags::debug))
            {
                std::string ret = "either [";
                ret += ToString(static_cast<const T &>(target), flags);
                ret += "] or [";
                ret += ToString(*target.ambiguous_alternative, flags);
                ret += ']';
                return ret;
            }
            else
            {
                std::string ret = "ambiguous, either [";
                ret += ToString(static_cast<const T &>(target), flags);

                const MaybeAmbiguous<T> *cur = target.ambiguous_alternative.get();
                do
                {
                    ret += "] or [";
                    ret += ToString(static_cast<const T &>(*cur), flags);

                    cur = cur->ambiguous_alternative.get();
                }
                while (cur);

                ret += ']';

                return ret;
            }
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const Pointer &target, ToCodeFlags flags, CvQualifiers ignore_cv_quals)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));
        (void)flags;

        return "*" + CvQualifiersToString(target.quals & ~ignore_cv_quals);
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const Pointer &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret = CvQualifiersToString(target.quals, '_', true);
            if (!ret.empty())
                ret += '_';
            ret += "ptr";
            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret = CvQualifiersToString(target.quals, ' ', true);
            if (!ret.empty())
                ret += ' ';
            ret += "pointer to";
            return ret;
        }
        else
        {
            std::string quals = CvQualifiersToString(target.quals, ' ', true);
            std::string ret(ChooseAVsAnForCvQualifersString(quals, false));
            ret += ' ';
            ret += quals;
            if (target.quals != CvQualifiers{})
                ret += ' ';
            ret += "pointer to";
            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const Reference &target, ToCodeFlags flags, CvQualifiers ignore_cv_quals)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));
        (void)flags;

        std::string ret(RefQualifierToString(target.kind));
        ret += CvQualifiersToString(target.quals & ~ignore_cv_quals);
        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const Reference &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret = CvQualifiersToString(target.quals, '_', true);
            if (!ret.empty())
                ret += '_';
            if (target.kind == RefQualifier::lvalue)
                ret += "ref";
            else if (target.kind == RefQualifier::rvalue)
                ret += "rvalue_ref";

            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret = CvQualifiersToString(target.quals, ' ', true);
            if (!ret.empty())
                ret += ' ';

            switch (target.kind)
            {
              case RefQualifier::none:
                assert(false && "No reference type specified.");
                break;
              case RefQualifier::lvalue:
                ret += "lvalue";
                break;
              case RefQualifier::rvalue:
                ret += "rvalue";
                break;
            }
            assert(!ret.empty() && "Unknown enum.");
            if (ret.empty())
                ret += "??";

            ret += " reference to";
            return ret;
        }
        else
        {
            std::string quals = CvQualifiersToString(target.quals, ' ', true);
            std::string ret(ChooseAVsAnForCvQualifersString(quals, true));
            ret += ' ';
            ret += quals;

            if (target.quals != CvQualifiers{})
                ret += ' ';

            switch (target.kind)
            {
              case RefQualifier::lvalue:
                ret += "lvalue reference to";
                break;
              case RefQualifier::rvalue:
                ret += "rvalue reference to";
                break;
              default:
                assert(false && "Unknown enum.");
            }
            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const MemberPointer &target, ToCodeFlags flags, CvQualifiers ignore_cv_quals)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));

        std::string ret = ToCode(target.base, flags);
        ret += "::*";
        ret += CvQualifiersToString(target.quals & ~ignore_cv_quals);
        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const MemberPointer &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret = CvQualifiersToString(target.quals, '_', true);
            if (!ret.empty())
                ret += '_';
            ret += "memptr";
            std::string type_str = ToString(target.base, flags);
            if (!type_str.empty())
            {
                ret += '_';
                ret += type_str;
            }
            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret = CvQualifiersToString(target.quals, ' ', true);
            if (!ret.empty())
                ret += ' ';
            ret += "pointer-to-member of class ";
            ret += ToString(target.base, flags);
            ret += " of type";
            return ret;
        }
        else
        {
            std::string quals = CvQualifiersToString(target.quals, ' ', true);
            std::string ret(ChooseAVsAnForCvQualifersString(quals, false));
            ret += ' ';
            ret += quals;

            if (target.quals != CvQualifiers{})
                ret += ' ';
            ret += "pointer-to-member of class ";
            ret += ToString(target.base, flags);
            ret += " of type";
            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const Array &target, ToCodeFlags flags, CvQualifiers /*ignore_cv_quals*/)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));

        std::string ret = "[";
        ret += ToCode(target.size, flags);
        ret += ']';
        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const Array &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret = "array";
            if (!target.size.IsEmpty())
            {
                ret += '_';
                ret += ToString(target.size, flags);
            }
            return ret;
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret;
            if (target.size.IsEmpty())
            {
                ret = "array of unknown bound of";
            }
            else
            {
                ret = "array of size ";
                ret += ToString(target.size, flags);
                ret += " of";
            }
            return ret;
        }
        else
        {
            std::string ret;
            if (target.size.IsEmpty())
            {
                ret = "an array of unknown bound of";
            }
            else
            {
                ret = "an array of size ";
                ret += ToString(target.size, flags);
                ret += " of";
            }
            return ret;
        }

        assert(false && "Unknown enum.");
        return "??";
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const Function &target, ToCodeFlags flags, CvQualifiers /*ignore_cv_quals*/)
    {
        // Function cv-qualifiers are not the actual cv-qualifiers of the type, so we ignore the `ignore_cv_quals`.
        // Maybe from the usability perspective we shouldn't ignore it, who knows.

        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));

        // At most one of `force_{c,cpp}_style_empty_params`.
        assert(!(bool(flags & ToCodeFlags::force_c_style_empty_params) && bool(flags & ToCodeFlags::force_cpp_style_empty_params)));

        // It's up to the caller to replace their type with `auto` if any of the function modifiers have that flag set.
        // And also the caller must paste the trailing return type after this string (we add `->` ourselves).

        std::string ret = "(";

        bool first = true;
        for (const auto &param : target.params)
        {
            if (first)
            {
                first = false;
            }
            else
            {
                ret += ',';
                if (!bool(flags & ToCodeFlags::no_space_after_comma))
                    ret += ' ';
            }

            ret += ToCode(param, flags);
        }

        if (target.c_style_variadic)
        {
            if (!target.params.empty() && (!target.c_style_variadic_without_comma || bool(flags & ToCodeFlags::force_comma_before_c_style_variadic)))
            {
                ret += ',';
                if (!bool(flags & ToCodeFlags::no_space_after_comma))
                    ret += ' ';
            }
            ret += "...";
        }

        if (target.params.empty() && !target.c_style_variadic &&
            (
                // Respect `target.c_style_void_params` unless overridden by the flags (in either direction).
                // Both flags can't appear at the same time.
                (target.c_style_void_params && !bool(flags & ToCodeFlags::force_cpp_style_empty_params)) ||
                bool(flags & ToCodeFlags::force_c_style_empty_params)
            )
        )
        {
            ret += "void";
        }

        ret += ')';

        if (target.cv_quals != CvQualifiers{})
        {
            ret += ' ';
            ret += CvQualifiersToString(target.cv_quals);
        }
        if (target.ref_qual != RefQualifier::none)
        {
            ret += ' ';
            ret += RefQualifierToString(target.ref_qual);
        }

        if (target.noexcept_)
            ret += " noexcept";

        if (target.uses_trailing_return_type && !bool(flags & ToCodeFlags::force_no_trailing_return_type))
            ret += " -> "; // The caller must add the type after this.

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const Function &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            std::string ret = "func";

            if (!target.params.empty() || target.c_style_variadic)
            {
                ret += "_from";

                for (const auto &param : target.params)
                {
                    ret += '_';
                    ret += ToString(param, flags);
                }

                if (target.c_style_variadic)
                    ret += "_ellipsis";
            }

            if (target.cv_quals != CvQualifiers{})
            {
                ret += '_';
                ret += CvQualifiersToString(target.cv_quals, '_', true);
            }

            switch (target.ref_qual)
            {
              case RefQualifier::none:
                // Nothing.
                break;
              case RefQualifier::lvalue:
                ret += "_lvalue";
                break;
              case RefQualifier::rvalue:
                ret += "_rvalue";
                break;
            }

            if (target.noexcept_)
                ret += "_noexcept";

            return ret;
        }

        std::string ret = "a function ";

        bool parens = false;
        auto AddDetail = [&]
        {
            if (!parens)
            {
                parens = true;
                ret += '(';
            }
            else
            {
                ret += ", ";
            }
        };

        if (target.cv_quals != CvQualifiers{})
        {
            AddDetail();
            ret += CvQualifiersToString(target.cv_quals, '-', true);
            ret += "-qualified";
        }

        if (target.ref_qual != RefQualifier{})
        {
            AddDetail();
            switch (target.ref_qual)
            {
              case RefQualifier::lvalue:
                ret += "lvalue-ref-qualified";
                break;
              case RefQualifier::rvalue:
                ret += "rvalue-ref-qualified";
                break;
              default:
                assert(false && "Unknown enum.");
            }
        }

        if (target.noexcept_)
        {
            AddDetail();
            ret += "noexcept";
        }

        if (parens)
            ret += ") ";

        ret += "taking ";
        if (target.params.empty())
        {
            ret += "no parameters";
            if (target.c_style_void_params)
                ret += " (spelled with C-style void)";
        }
        else
        {
            ret += NumberToString(target.params.size());
            ret += " parameter";
            if (target.params.size() != 1)
                ret += 's';
            ret += ": [";
            std::size_t i = 0;
            for (const auto &elem : target.params)
            {
                if (i > 0)
                    ret += ", ";

                i++;

                if (target.params.size() != 1)
                {
                    ret += NumberToString(i);
                    ret += ". ";
                }

                ret += ToString(elem, flags);
            }
            ret += ']';
        }
        if (target.c_style_variadic)
        {
            ret += " and a C-style variadic parameter";
            if (target.c_style_variadic_without_comma)
                ret += " (with a missing comma before it)"; // Illegal since C++26.
        }

        ret += ", returning";
        if (target.uses_trailing_return_type)
            ret += " (via trailing return type)";

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const TypeModifier &target, ToCodeFlags flags, CvQualifiers ignore_cv_quals)
    {
        assert(!bool(flags & ToCodeFlags::mask_any_half_type));
        assert(!bool(flags & ToCodeFlags::lambda));

        return std::visit([&](const auto &elem){return ToCode(elem, flags, ignore_cv_quals);}, target.var);
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const TypeModifier &target, ToStringFlags flags)
    {
        return std::visit([&](const auto &elem){return ToString(elem, flags);}, target.var);
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToCode(const Attribute &target, ToCodeFlags flags)
    {
        std::string ret;

        switch (target.style)
        {
          case Attribute::Style::cpp:
            ret = "[[";
            ret += ToCode(target.expr, flags);
            ret += "]]";
            return ret;
          case Attribute::Style::gnu:
            ret = "__attribute__((";
            ret += ToCode(target.expr, flags);
            ret += "))";
            return ret;
        }

        assert(false && "Invalid attribute style enum.");
        ret = "??";

        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR std::string ToString(const Attribute &target, ToStringFlags flags)
    {
        if (bool(flags & ToStringFlags::identifier))
        {
            // This does something, for completeness. But calling `ToString(..., identifier)` on a `SimpleType` will just ignore attirbutes.
            return ToString(target.expr, flags);
        }
        else if (bool(flags & ToStringFlags::debug))
        {
            std::string ret = "{style=";

            [[maybe_unused]] bool style_known = false;
            switch (target.style)
            {
              case Attribute::Style::cpp:
                ret += "cpp";
                style_known = true;
                break;
              case Attribute::Style::gnu:
                ret += "gnu";
                style_known = true;
                break;
            }
            assert(style_known && "Invalid attribute style enum.");

            ret += ",attr=";
            ret += ToString(target.expr, flags);
            ret += "}";
            return ret;
        }
        else
        {
            std::string ret;

            [[maybe_unused]] bool style_known = false;
            switch (target.style)
            {
              case Attribute::Style::cpp:
                // Nothing.
                style_known = true;
                break;
              case Attribute::Style::gnu:
                ret += "GNU-style ";
                style_known = true;
                break;
            }
            assert(style_known && "Invalid attribute style enum.");

            ret += "attribute ";
            ret += ToString(target.expr, flags);

            return ret;
        }
    }
}
