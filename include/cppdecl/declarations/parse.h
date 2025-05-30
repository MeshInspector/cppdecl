#pragma once

#include "cppdecl/declarations/data.h"
#include "cppdecl/misc/platform.h"
#include "cppdecl/misc/string_helpers.h"

#include <algorithm>
#include <iterator>
#include <type_traits>
#include <utility>
#include <variant>

// Those functions parse various language constructs. There's a lot here, but you mainly want two functions:
// * `ParseType()` to parse types.
// * `ParseDecl()` to parse types or declarations (this is a superset of `ParseType`).
// In any case, the return value is a `std::variant` of either the result or a parsing error.
// The input `std::string_view` has the parsed prefix of it removed. On failure, the new start points to the error.
// On success it contains the unparsed suffix.
// On success you should probably check that it's empty. (The trailing whitespace should be stripped by us automatically.)

namespace cppdecl
{
    struct ParseError
    {
        const char *message = nullptr;
    };


    using ParseTemplateArgumentListResult = std::variant<std::optional<TemplateArgumentList>, ParseError>;
    [[nodiscard]] CPPDECL_CONSTEXPR ParseTemplateArgumentListResult ParseTemplateArgumentList(std::string_view &input);


    enum class ParseTypeFlags
    {
        // This is for target types of conversion operators.
        // Accept only the declarators that would be to the left of a variable name, stop on those that would be to the right.
        // Also don't accept `(`.
        only_left_side_declarators_without_parens = 1 << 0,
    };
    CPPDECL_FLAG_OPERATORS(ParseTypeFlags)

    using ParseTypeResult = std::variant<Type, ParseError>;
    [[nodiscard]] CPPDECL_CONSTEXPR ParseTypeResult ParseType(std::string_view &input, ParseTypeFlags flags = {});


    enum class ParseSimpleTypeFlags
    {
        // Reject any types containing `::`.
        // This is needed for destructor types as in `A::~B::C` (this must reject `::C`).
        only_unqualified = 1 << 0,

        // Reject elaborated type specifiers, and also `typename`.
        no_type_prefix = 1 << 1,

        // Don't insist on an actual type, allow any qualified name.
        allow_arbitrary_names = 1 << 2,
    };
    CPPDECL_FLAG_OPERATORS(ParseSimpleTypeFlags)

    using ParseSimpleTypeResult = std::variant<SimpleType, ParseError>;
    [[nodiscard]] CPPDECL_CONSTEXPR ParseSimpleTypeResult ParseSimpleType(std::string_view &input, ParseSimpleTypeFlags flags = {});


    using ParseQualifiersResult = std::variant<CvQualifiers, ParseError>;

    // Returns a bit-or of 0 or more qualifiers. Silently fails if there are no qualifiers to parse.
    // Returns an error on duplicate qualifiers.
    // Removes leading but not trailing whitespace.
    // NOTE: This currently isn't used to parse the top-level cv-qualifiers in decl-specifier-seq,
    //   so this freely accepts `__restrict` and other weird things that can't appear there.
    [[nodiscard]] CPPDECL_CONSTEXPR ParseQualifiersResult ParseCvQualifiers(std::string_view &input)
    {
        ParseQualifiersResult ret{};
        CvQualifiers &ret_quals = std::get<CvQualifiers>(ret);

        while (true)
        {
            auto input_copy = input;
            TrimLeadingWhitespace(input_copy);
            auto input_copy_after_whitespace = input_copy;

            CvQualifiers bit{};

            if (ConsumeWord(input_copy, "const"))
                bit = CvQualifiers::const_;
            else if (ConsumeWord(input_copy, "volatile"))
                bit = CvQualifiers::volatile_;
            // Here we include the non-conformant (`restrict`) spelling too. TODO a flag to only allow conformant C++ spellings?
            else if (ConsumeWord(input_copy, "__restrict") || ConsumeWord(input_copy, "__restrict__") || ConsumeWord(input_copy, "restrict"))
                bit = CvQualifiers::restrict_;
            // Weird MSVC stuff: [
            else if (ConsumeWord(input_copy, "__ptr32"))
                bit = CvQualifiers::msvc_ptr32;
            else if (ConsumeWord(input_copy, "__ptr64"))
                bit = CvQualifiers::msvc_ptr64;
            // ]

            if (!bool(bit))
                return ret;

            if (bool(bit & ret_quals))
            {
                input = input_copy_after_whitespace;
                return ret = ParseError{.message = "Duplicate cv-qualifier."}, ret;
            }

            ret_quals |= bit;
            input = input_copy;
        }

        return ret;
    }

    // Trims leading whitespace and consumes 0..2 `&`.
    [[nodiscard]] CPPDECL_CONSTEXPR RefQualifier ParseRefQualifier(std::string_view &input)
    {
        RefQualifier ret = RefQualifier::none;

        std::string_view input_copy = input;
        TrimLeadingWhitespace(input_copy);
        if (input_copy.starts_with('&'))
        {
            input = input_copy;

            input.remove_prefix(1);
            ret = RefQualifier::lvalue;
            // Don't trim whitespace here! Whitespace between the two ampersands is illegal.
            if (input.starts_with('&'))
            {
                input.remove_prefix(1);
                ret = RefQualifier::rvalue;
            }
        }
        return ret;
    }

    [[nodiscard]] CPPDECL_CONSTEXPR SimpleTypePrefix StringToSimpleTypePrefix(std::string_view str)
    {
        if (str == "struct")
            return SimpleTypePrefix::struct_;
        if (str == "class")
            return SimpleTypePrefix::class_;
        if (str == "union")
            return SimpleTypePrefix::union_;
        if (str == "enum")
            return SimpleTypePrefix::enum_;

        if (str == "typename")
            return SimpleTypePrefix::typename_;

        return SimpleTypePrefix::none;
    }


    enum class ParseQualifiedNameFlags
    {
        // Only parse an unqualified name, stop at any `::` (including the leading one).
        only_unqualified = 1 << 0,

        // Reject names where the last unqualified component isn't a normal name (e.g. is a destructor, UDL, or conversion operator).
        only_valid_types = 1 << 1,

        // Reject names that are 100% types.
        only_valid_nontypes = 1 << 2,

        // Allow destructor names that literally start with `~`, e.g. `~A` and `~A::B` (we don't police name equality because typedefs mess that up).
        // Things like `A::~B` are always allowed regardless of this flag.
        allow_unqualified_destructors = 1 << 3,

        // Allow `true`, `false`, `nullptr` as the name.
        allow_builtin_names = 1 << 4,
    };
    CPPDECL_FLAG_OPERATORS(ParseQualifiedNameFlags)

    // NOTE: This can return either a `QualifiedName` OR a `MemberPointer` on success (the latter is returned if it's followed by `:: * [cv]`.
    using ParseQualifiedNameResult = std::variant<QualifiedName, MemberPointer, ParseError>;

    // Returns a `QualifiedName` with no elements if there's nothing to parse.
    // Ignores leading whitespace. Modifies `input` to remove the parsed prefix (unless there was an error).
    // When `input` is modified, the trailing whitespace is stripped automatically. This happens even if there was nothing to parse.
    // If the input ends with `:: * [cv]` (as in a member pointer), returns a `MemberPointer` instead of a `QualifiedName`.
    // NOTE: This doesn't understand `long long` (hence "Low"), use `ParseDecl()` to support that.
    [[nodiscard]] CPPDECL_CONSTEXPR ParseQualifiedNameResult ParseQualifiedName(std::string_view &input, ParseQualifiedNameFlags flags = {})
    {
        ParseQualifiedNameResult ret;

        if (bool(flags & ParseQualifiedNameFlags::only_valid_types) && bool(flags & ParseQualifiedNameFlags::only_valid_nontypes))
            return ret = ParseError{.message = "Bad usage, invalid flags: Rejecting both types and nontypes."}, ret;

        QualifiedName &ret_name = std::get<QualifiedName>(ret);

        TrimLeadingWhitespace(input);

        const std::string_view input_before_parse = input;

        if (input.empty())
            return ret; // Nothing to parse.

        std::string_view s = input;

        bool only_unqualified = bool(flags & ParseQualifiedNameFlags::only_unqualified);

        // Handle leading `::`.
        if (s.starts_with("::"))
        {
            if (only_unqualified)
                return ret; // Nope.

            ret_name.force_global_scope = true;
            s.remove_prefix(2);
            TrimLeadingWhitespace(s);
        }

        bool allow_destructors = bool(flags & ParseQualifiedNameFlags::allow_unqualified_destructors);

        bool first = true;
        bool first_part_is_known_type = false;

        if ((!s.empty() && IsNonDigitIdentifierChar(s.front())) || (allow_destructors && s.starts_with("~")))
        {
            while (true)
            {
                const std::string_view input_before_this_unqual_part = s; // Sic, not `= input`.

                bool stop_on_this_iteration = false;

                // A destructor?
                if (allow_destructors && ConsumePunctuation(s, "~"))
                {
                    first = false;

                    // Looks like a destructor.
                    TrimLeadingWhitespace(s);

                    auto type_result = ParseSimpleType(s, ParseSimpleTypeFlags::only_unqualified | ParseSimpleTypeFlags::no_type_prefix);
                    if (auto error = std::get_if<ParseError>(&type_result))
                    {
                        input = s;
                        return ret = *error, ret;
                    }

                    UnqualifiedName new_unqual_part;
                    DestructorName &dtor = new_unqual_part.var.emplace<DestructorName>();
                    dtor.simple_type = std::move(std::get<SimpleType>(type_result));

                    ret_name.parts.push_back(std::move(new_unqual_part));

                    input = s;
                }
                else
                {
                    // Not a destructor.

                    const char *cur = s.data();

                    do
                        cur++;
                    while (cur < input.data() + input.size() && IsIdentifierChar(*cur));

                    std::string_view new_word = std::string_view(s.data(), cur);
                    s.remove_prefix(std::size_t(cur - s.data()));
                    TrimLeadingWhitespace(s);

                    if (first)
                    {
                        first = false;
                        first_part_is_known_type = IsTypeRelatedKeyword(new_word);

                        if (first_part_is_known_type && bool(flags & ParseQualifiedNameFlags::only_valid_nontypes))
                        {
                            input = input_before_parse;
                            return ret;
                        }

                        if (IsLiteralConstantKeyword(new_word))
                        {
                            if (bool(flags & ParseQualifiedNameFlags::only_valid_types) || !bool(flags & ParseQualifiedNameFlags::allow_builtin_names))
                            {
                                input = input_before_parse;
                                return ret;
                            }
                            else
                            {
                                stop_on_this_iteration = true;
                            }
                        }
                    }
                    else
                    {
                        if (IsTypeRelatedKeyword(new_word))
                        {
                            // Trying to use a built-in type before a `::`.
                            // We used to treat this as a hard error, but apparently `int ::T::* x` is valid, so we can't reject it here.
                            return ret;
                        }

                        if (IsLiteralConstantKeyword(new_word))
                        {
                            input = input_before_this_unqual_part;
                            return ret = ParseError{.message = "This keyword can't be a part of a qualified name."}, ret;
                        }
                    }


                    UnqualifiedName new_unqual_part;

                    // Is this something weird?
                    if (new_word == "operator")
                    {
                        // The whitespace was already stripped at this point.

                        if (ConsumePunctuation(s, "\"\""))
                        {
                            // User-defined literal.

                            UserDefinedLiteral &udl = new_unqual_part.var.emplace<UserDefinedLiteral>();

                            udl.space_before_suffix = TrimLeadingWhitespace(s);
                            if (s.empty() || !IsNonDigitIdentifierChar(s.front()))
                            {
                                input = s;
                                ret = ParseError{.message = "Expected identifier after `\"\"` in a user-defined literal."};
                                return ret;
                            }

                            do
                            {
                                udl.suffix += s.front();
                                s.remove_prefix(1);
                            }
                            while (!s.empty() && IsIdentifierChar(s.front()));
                        }
                        else if (std::string_view op_token; ConsumeOperatorToken(s, op_token))
                        {
                            // Overloaded operator.
                            OverloadedOperator &op = new_unqual_part.var.emplace<OverloadedOperator>();
                            op.token = op_token;
                        }
                        else
                        {
                            // Has to be a conversion operator at this point.
                            auto type_result = ParseType(s, ParseTypeFlags::only_left_side_declarators_without_parens);
                            if (auto error = std::get_if<ParseError>(&type_result))
                            {
                                input = s;
                                return ret = *error, ret;
                            }

                            ConversionOperator &conv = new_unqual_part.var.emplace<ConversionOperator>();
                            conv.target_type = std::move(std::get<Type>(type_result));
                        }
                    }

                    // If this isn't a special `operator` after all the checks above...

                    if (auto name = std::get_if<std::string>(&new_unqual_part.var))
                        *name = new_word;

                    // Advance `input` to parse the template argument list. That's parsed with the real `input` to show a good error.
                    input = s;

                    // Consume the template arguments, if any.
                    auto arglist_result = ParseTemplateArgumentList(input);
                    if (auto error = std::get_if<ParseError>(&arglist_result))
                        return ret = *error, ret;
                    new_unqual_part.template_args = std::move(std::get<std::optional<TemplateArgumentList>>(arglist_result));

                    ret_name.parts.push_back(std::move(new_unqual_part));

                    s = input;
                }

                // Can be redundant in some cases, but just in case.
                TrimLeadingWhitespace(input);
                s = input;

                // If this is a `true`, `false` or `nullptr`, break now.
                if (stop_on_this_iteration)
                    break;

                // Allow destructors on the next iterations, if not already.
                allow_destructors = true;

                // Make sure we have `:: letter` after this, or break.
                if (!only_unqualified && ConsumePunctuation(s, "::"))
                {
                    TrimLeadingWhitespace(s);
                    if (!s.empty())
                    {
                        if (IsNonDigitIdentifierChar(s.front()) || (allow_destructors && s.starts_with('~')))
                        {
                            if (first_part_is_known_type)
                                return ret;

                            continue;
                        }

                        if (s.front() == '*') // This looks like a member pointer.
                        {
                            s.remove_prefix(1);
                            auto parsed_quals = ParseCvQualifiers(s);
                            input = s;
                            if (auto error = std::get_if<ParseError>(&parsed_quals))
                                return ret = *error, ret;
                            MemberPointer memptr;
                            memptr.quals = std::get<CvQualifiers>(parsed_quals);
                            memptr.base = std::move(ret_name);
                            ret = std::move(memptr);
                            return ret;
                        }
                    }
                }


                // Stop if the last component doesn't form a valid type, and we want one.
                if (bool(flags & ParseQualifiedNameFlags::only_valid_types) && !std::holds_alternative<std::string>(ret_name.parts.back().var))
                {
                    // Roll back everything. This can't be an error.
                    input = input_before_parse;
                    ret_name = {};
                    return ret;
                }

                break;
            }
        }

        // Reset `force_global_scope` if the input was empty and `::` was just junk.
        if (ret_name.parts.empty())
            ret_name.force_global_scope = false;

        // Do a final validation.
        if (
            // If we're trying to produce a type and the name doesn't end with a string...
            (bool(flags & ParseQualifiedNameFlags::only_valid_types) && !ret_name.LastComponentIsNormalString())
            // Uncommenting this would reject `A::A` as types. Not sure if this is a good idea.
            // (bool(flags & ParseQualifiedNameFlags::only_valid_types) && ret_name.CertainlyIsQualifiedConstructorName())
        )
        {
            input = input_before_parse;
            ret_name = {};
            return ret;
        }

        return ret;
    }


    // True on success, false if nothing to do, error if this looks illegal.
    using TryAddNameToTypeResult = std::variant<bool, ParseError>;

    enum class TryAddNameToTypeFlags
    {
        no_type_prefix = 1 << 0,
    };
    CPPDECL_FLAG_OPERATORS(TryAddNameToTypeFlags)

    // Tries to modify this type by adding another name to it.
    // This always succeeds if the type is empty, and then it only accepts things like
    //   adding `long` to another `long`, or `unsigned` plus something else, etc.
    // NOTE: This does nothing if `name` is empty, and in that case you should probably stop whatever you're doing to avoid infinite loops.
    template <typename T> requires std::is_same_v<std::remove_cvref_t<T>, QualifiedName>
    [[nodiscard]] CPPDECL_CONSTEXPR TryAddNameToTypeResult TryAddNameToType(SimpleType &type, /*QualifiedName*/ T &&new_name, TryAddNameToTypeFlags flags)
    {
        if (new_name.IsEmpty())
            return false; // The name is empty, nothing to do.

        const std::string_view word = new_name.AsSingleWord();
        const std::string_view existing_word = type.name.AsSingleWord();

        if (word == "const")
        {
            if (bool(type.quals & CvQualifiers::const_))
                return ParseError{.message = "Repeated `const`."};
            type.quals |= CvQualifiers::const_;
            return true;
        }
        if (word == "volatile")
        {
            if (bool(type.quals & CvQualifiers::volatile_))
                return ParseError{.message = "Repeated `volatile`."};
            type.quals |= CvQualifiers::volatile_;
            return true;
        }
        if (word == "unsigned")
        {
            if (bool(type.flags & SimpleTypeFlags::unsigned_))
                return ParseError{.message = "Repeated `unsigned`."};
            if (bool(type.flags & SimpleTypeFlags::explicitly_signed))
                return ParseError{.message = "Both `signed` and `unsigned` on the same type."};
            if (!type.name.IsEmpty() && !type.name.IsBuiltInTypeName(IsBuiltInTypeNameFlags::allow_integral))
                return ParseError{.message = "Can only apply `unsigned` directly to built-in arithmetic types."}; // Yes, you can't use it on a typedef.
            type.flags |= SimpleTypeFlags::unsigned_;
            return true;
        }
        if (word == "signed")
        {
            if (bool(type.flags & SimpleTypeFlags::explicitly_signed))
                return ParseError{.message = "Repeated `signed`."};
            if (bool(type.flags & SimpleTypeFlags::unsigned_))
                return ParseError{.message = "Both `unsigned` and `signed` on the same type."};
            if (!type.name.IsEmpty() && !type.name.IsBuiltInTypeName(IsBuiltInTypeNameFlags::allow_integral))
                return ParseError{.message = "Can only apply `signed` directly to built-in arithmetic types."}; // Yes, you can't use it on a typedef.
            type.flags |= SimpleTypeFlags::explicitly_signed;
            return true;
        }
        if (SimpleTypePrefix new_prefix = StringToSimpleTypePrefix(word); new_prefix != SimpleTypePrefix{})
        {
            if (bool(flags & TryAddNameToTypeFlags::no_type_prefix))
                return ParseError{.message = "Destructor type can't include type prefixes."}; // This could be a soft error, but is there any scenario where a hard error would be an issue? I don't know one.
            if (!type.name.IsEmpty())
                return ParseError{.message = "The type prefix can't appear after the type."};
            if (type.prefix != SimpleTypePrefix{})
                return ParseError{.message = "More than one type prefix."};
            type.prefix = new_prefix;
            return true;
        }
        // Combining together all the `[long [long]] [int]` bullshit:
        // int + short, int + long  -> remove the `int`, keep the new spelling and set the flag.
        if (existing_word == "int" && (word == "short" || word == "long"))
        {
            if (bool(type.flags & SimpleTypeFlags::redundant_int))
                return ParseError{.message = "Repeated `int`."};
            type.flags |= SimpleTypeFlags::redundant_int;
            type.name.parts.front().var = std::string(word);
            return true;
        }
        // short + int, long + int, long long + int  -> set the flag and ignore `int`
        if (word == "int" && (existing_word == "short" || existing_word == "long" || existing_word == "long long"))
        {
            if (bool(type.flags & SimpleTypeFlags::redundant_int))
                return ParseError{.message = "Repeated `int`."};
            type.flags |= SimpleTypeFlags::redundant_int;
            return true;
        }
        // long + long
        if (word == "long" && existing_word == "long")
        {
            type.name.parts.front().var = "long long";
            return true;
        }
        // long + double
        if ((word == "long" && existing_word == "double") || (word == "double" && existing_word == "long"))
        {
            type.name.parts.front().var = "long double";
            return true;
        }

        // The original type was empty, replace it completely.
        // Note that this has to be late.
        if (type.IsEmptyUnsafe())
        {
            // Handle `[un]signed` + something other than a builtin integral type.
            // This is a SOFT error because `signed A;` is a variable declaration.
            // Note that the reverse (`A signed;`), which is handled above, is a HARD error (I don't see any usecase where it needs to be soft).
            if (bool(type.flags & (SimpleTypeFlags::unsigned_ | SimpleTypeFlags::explicitly_signed)) && !new_name.IsBuiltInTypeName(IsBuiltInTypeNameFlags::allow_integral))
                return false;

            if (type.prefix == SimpleTypePrefix::typename_)
            {
                // Disabling this for now because MSVC allows this crap.
                // if (!new_name.IsQualified())
                //     return ParseError{.message = "`typename` must be followed by a qualified name."};
            }
            else if (type.prefix != SimpleTypePrefix{})
            {
                if (new_name.IsBuiltInTypeName())
                    return ParseError{.message = "Elaborated type specifier applied to a built-in type."};
            }

            // The move is useless right now, since this is a const reference.
            // This is probably better than passing by value and always copying though.
            type.name = std::forward<T>(new_name);
            return true;
        }

        // The name being added is a keyword that looks suspicious.
        if (IsTypeRelatedKeyword(word))
            return ParseError{.message = "Can't add this keyword to the preceding type."};

        return false; // Don't know what this is.
    }

    // Parse a "simple type". Very similar to `ParseQualifiedName`, but also combines `long` + `long`, and similar things.
    // Returns an empty type if nothing to parse.
    [[nodiscard]] CPPDECL_CONSTEXPR ParseSimpleTypeResult ParseSimpleType(std::string_view &input, ParseSimpleTypeFlags flags)
    {
        ParseSimpleTypeResult ret;
        SimpleType &ret_type = std::get<SimpleType>(ret);

        const ParseQualifiedNameFlags qual_name_flags =
            ParseQualifiedNameFlags::only_valid_types * !bool(flags & ParseSimpleTypeFlags::allow_arbitrary_names) |
            ParseQualifiedNameFlags::only_unqualified * bool(flags & ParseSimpleTypeFlags::only_unqualified) |
            ParseQualifiedNameFlags::allow_builtin_names * bool(flags & ParseSimpleTypeFlags::allow_arbitrary_names);

        while (true)
        {
            const std::string_view input_before_name = input;

            auto name_result = ParseQualifiedName(input, qual_name_flags);
            if (auto error = std::get_if<ParseError>(&name_result))
                return ret = *error, ret;

            if (std::holds_alternative<MemberPointer>(name_result))
            {
                input = input_before_name;
                break; // Can't use this for anything, undo this element and stop.
            }

            QualifiedName &new_name = std::get<QualifiedName>(name_result);

            if (new_name.IsEmpty())
                break; // No more names to parse, stop.

            auto add_name_result = TryAddNameToType(ret_type, new_name, bool(flags & ParseSimpleTypeFlags::no_type_prefix) * TryAddNameToTypeFlags::no_type_prefix);
            if (auto error = std::get_if<ParseError>(&add_name_result))
            {
                input = input_before_name;
                TrimLeadingWhitespace(input);
                return ret = *error, ret;
            }

            bool added = std::get<bool>(add_name_result);
            if (!added)
            {
                input = input_before_name;
                break; // Some unknown syntax here, undo this element and stop.
            }
        }

        return ret;
    }


    using ParseNumberTokenResult = std::variant<NumberToken, ParseError>;

    [[nodiscard]] CPPDECL_CONSTEXPR ParseNumberTokenResult ParseNumberToken(std::string_view &input)
    {
        TrimLeadingWhitespace(input);
        const std::string_view input_before_parse = input;

        ParseNumberTokenResult ret;
        NumberToken &ret_token = std::get<NumberToken>(ret);
        NumberToken::Integer *ret_int = &std::get<NumberToken::Integer>(ret_token.var);
        NumberToken::FloatingPoint *ret_float = nullptr;

        bool allow_apostrophe = false;
        bool (*validation_func)(char) = IsDigit;

        if (ConsumePunctuation(input, "0x") || ConsumePunctuation(input, "0X"))
        {
            ret_int->kind = NumberToken::Integer::Kind::hex;
            validation_func = IsHexDigit;
        }
        else if (ConsumePunctuation(input, "0b") || ConsumePunctuation(input, "0B"))
        {
            ret_int->kind = NumberToken::Integer::Kind::binary;
            validation_func = IsBinDigit;
        }
        else if (ConsumePunctuation(input, "0"))
        {
            ret_int->kind = NumberToken::Integer::Kind::octal;
            validation_func = IsOctalDigit;
            allow_apostrophe = true;
        }

        // Consume the integral part, if any.
        while (!input.empty())
        {
            char ch = input.front();

            if (validation_func(ch) || (ch == '\'' && allow_apostrophe))
            {
                ret_int->value += ch;
                input.remove_prefix(1);
                allow_apostrophe = true;
            }
            else
            {
                break;
            }
        }

        const bool is_hex = ret_int->kind == NumberToken::Integer::Kind::hex;

        auto ConvertToFloatingPoint = [&]() -> ParseError
        {
            if (ret_float)
                return {}; // Already floating-point.

            // Complain if this is not a decimal or hex literal.
            if (ret_int->kind != NumberToken::Integer::Kind::decimal && ret_int->kind != NumberToken::Integer::Kind::hex)
            {
                return ParseError{.message = ret_int->kind == NumberToken::Integer::Kind::binary ? "Binary literals can't be fractional." : "Octal literals can't be fractional."};
            }

            // Convert to a floating-point literal.
            NumberToken::FloatingPoint new_float; // Need a temporary variable to move the contents over from the integer literal.
            new_float.value_int = std::move(ret_int->value);
            new_float.kind = is_hex ? NumberToken::FloatingPoint::Kind::hex : NumberToken::FloatingPoint::Kind::decimal;
            ret_float = &ret_token.var.emplace<NumberToken::FloatingPoint>(std::move(new_float));
            ret_int = nullptr;

            return {};
        };

        // The fractional part, if any.
        const std::string_view input_before_frac = input;
        if (ConsumePunctuation(input, "."))
        {
            // Convert to a floating-point literal.
            if (ParseError error = ConvertToFloatingPoint(); error.message)
            {
                input = input_before_frac;
                return ret = error, ret;
            }

            // Consume the fractional part.
            allow_apostrophe = false;
            while (!input.empty())
            {
                char ch = input.front();
                if (validation_func(ch) || (ch == '\'' && allow_apostrophe))
                {
                    ret_float->value_frac += ch;
                    input.remove_prefix(1);
                    allow_apostrophe = true;
                }
            }

            if (ret_float->value_int.empty() && ret_float->value_frac.empty())
                return ret = ParseError{.message = "In a floating-point literal, expected at least one digit before or after the decimal point."}, ret;
        }

        // Consume the exponent, if any.
        if (ConsumePunctuation(input, is_hex ? "p" : "e") || ConsumePunctuation(input, is_hex ? "P" : "E"))
        {
            // Convert to a floating-point literal.
            if (ParseError error = ConvertToFloatingPoint(); error.message)
            {
                input = input_before_frac;
                return ret = error, ret;
            }

            if (ConsumePunctuation(input, "+"))
                ret_float->value_exp = "+";
            else if (ConsumePunctuation(input, "-"))
                ret_float->value_exp = "-";

            allow_apostrophe = false;
            while (!input.empty())
            {
                char ch = input.front();
                if (IsDigit(ch) || (ch == '\'' && allow_apostrophe)) // Those are always decimal, so no `validation_func` here.
                {
                    ret_float->value_exp += ch;
                    input.remove_prefix(1);
                    allow_apostrophe = true;
                }
            }

            if (!allow_apostrophe)
                return ret = ParseError{.message = "Expected the exponent."}, ret;
        }

        // Consume the suffix, if any.
        if (ret_float)
        {
            // A floating-point suffix.

            std::string &suffix_str = std::get<std::string>(ret_float->suffix);
            while (!input.empty() && IsIdentifierChar(input.front()))
            {
                input.remove_prefix(1);
                suffix_str += input.front();
            }

            // Decode the suffix if possible
            if      (suffix_str == "f" || suffix_str == "F") ret_float->suffix = NumberToken::FloatingPoint::Suffix::f;
            else if (suffix_str == "l" || suffix_str == "L") ret_float->suffix = NumberToken::FloatingPoint::Suffix::l;
            else if (suffix_str == "f16" || suffix_str == "F16") ret_float->suffix = NumberToken::FloatingPoint::Suffix::f16;
            else if (suffix_str == "f32" || suffix_str == "F32") ret_float->suffix = NumberToken::FloatingPoint::Suffix::f32;
            else if (suffix_str == "f64" || suffix_str == "F64") ret_float->suffix = NumberToken::FloatingPoint::Suffix::f64;
            else if (suffix_str == "f128" || suffix_str == "F128") ret_float->suffix = NumberToken::FloatingPoint::Suffix::f128;    else if (suffix_str == "bf16" || suffix_str == "BF16") ret_float->suffix = NumberToken::FloatingPoint::Suffix::bf16;

        }
        else
        {
            // An integral suffix.

            std::string &suffix_str = std::get<std::string>(ret_int->suffix);
            while (!input.empty() && IsIdentifierChar(input.front()))
            {
                input.remove_prefix(1);
                suffix_str += input.front();
            }

            // Decode the suffix if possible.
            NumberToken::Integer::Suffix new_suffix;
            std::string_view suffix_view = suffix_str;
            if (
                ConsumePunctuation(suffix_view, "u") || ConsumeTrailingPunctuation(suffix_view, "u") ||
                ConsumePunctuation(suffix_view, "U") || ConsumeTrailingPunctuation(suffix_view, "U")
            )
            {
                new_suffix.is_unsigned = true;
            }

            bool ok = true;
            if      (suffix_view == "l"  || suffix_view == "L" ) new_suffix.signed_part = NumberToken::Integer::SignedSuffix::l;
            else if (suffix_view == "ll" || suffix_view == "LL") new_suffix.signed_part = NumberToken::Integer::SignedSuffix::ll;
            else if (suffix_view == "z"  || suffix_view == "Z" ) new_suffix.signed_part = NumberToken::Integer::SignedSuffix::z;
            else
            {
                ok = false;
            }

            // If the suffix decoded successfully, replace the string with the result of decoding.
            if (ok)
                ret_int->suffix = std::move(new_suffix);
        }

        return ret;
    }

    enum class ParsePseudoExprFlags
    {
        // Stop parsing on `>`. Good for template argument lists.
        // Otherwise will assume that it's a punctuation symbol.
        stop_on_gt_sign = 1 << 0,
    };
    CPPDECL_FLAG_OPERATORS(ParsePseudoExprFlags)

    using ParsePseudoExprResult = std::variant<PseudoExpr, ParseError>;
    // Parse an expression. Even though we call those expressions, it's a fairly loose collection of tokens.
    // We continue parsing until we hit a comma or a closing bracket: `)`,`}`,`]`,`>`.
    // Can return an empty expression.
    [[nodiscard]] CPPDECL_CONSTEXPR ParsePseudoExprResult ParsePseudoExpr(std::string_view &input, ParsePseudoExprFlags flags = {})
    {
        ParsePseudoExprResult ret;
        PseudoExpr &ret_expr = std::get<PseudoExpr>(ret);

        while (true)
        {
            TrimLeadingWhitespace(input);

            if (
                input.empty() ||
                input.starts_with(',') || input.starts_with(')') || input.starts_with('}') || input.starts_with(']') ||
                (bool(flags & ParsePseudoExprFlags::stop_on_gt_sign) && input.starts_with('>'))
            )
            {
                return ret;
            }

            // Number.
            if (IsDigit(input.front()))
            {
                NumberToken num;
                do
                {
                    num.value += input.front();
                    input.remove_prefix(1);
                }
                while (
                    !input.empty() &&
                    (
                        IsDigit(input.front()) ||
                        IsAlpha(input.front()) ||
                        input.front() == '.' ||
                        input.front() == '\'' ||
                        (
                            (input.front() == '+' || input.front() == '-') &&
                            !num.value.empty() &&
                            (
                                num.value.back() == 'e' ||
                                num.value.back() == 'E' ||
                                num.value.back() == 'p' ||
                                num.value.back() == 'P'
                            )
                        )
                    )
                );

                ret_expr.tokens.emplace_back(std::move(num));
                continue;
            }

            { // String or character literal.
                std::string_view input_copy = input;

                StringOrCharLiteral lit;

                if (ConsumePunctuation(input_copy, "L"))
                    lit.type = StringOrCharLiteral::Type::wide;
                else if (ConsumePunctuation(input_copy, "u8")) // Check before `u` since that is a substring of this.
                    lit.type = StringOrCharLiteral::Type::u8;
                else if (ConsumePunctuation(input_copy, "u"))
                    lit.type = StringOrCharLiteral::Type::u16;
                else if (ConsumePunctuation(input_copy, "U"))
                    lit.type = StringOrCharLiteral::Type::u32;

                bool ok = true;
                if (ConsumePunctuation(input_copy, "\""))
                    lit.kind = StringOrCharLiteral::Kind::string;
                else if (ConsumePunctuation(input_copy, "'"))
                    lit.kind = StringOrCharLiteral::Kind::character;
                else if (ConsumePunctuation(input_copy, "R\""))
                    lit.kind = StringOrCharLiteral::Kind::raw_string;
                else
                    ok = false;

                if (ok)
                {
                    // Now we're sure that it's a string literal.

                    const std::string_view input_at_start_of_literal = input;
                    input = input_copy;

                    if (lit.kind != StringOrCharLiteral::Kind::raw_string)
                    {
                        char quote = lit.kind == StringOrCharLiteral::Kind::character ? '\'' : '"';
                        while (true)
                        {
                            if (input.empty())
                            {
                                const char *error = nullptr;
                                switch (lit.kind)
                                {
                                    case StringOrCharLiteral::Kind::character:  error = "Unterminated character literal."; break;
                                    case StringOrCharLiteral::Kind::string:     error = "Unterminated string literal."; break;
                                    case StringOrCharLiteral::Kind::raw_string: break; // Unreachable.
                                }
                                input = input_at_start_of_literal;
                                return ret = ParseError{.message = error}, ret;
                            }

                            const auto input_at_character = input;

                            if (ConsumePunctuation(input, "\\"))
                            {
                                if (input.empty())
                                {
                                    input = input_at_character;
                                    return ret = ParseError{.message = "Unterminated escape sequence."}, ret;
                                }
                                // Add the escape sequence to the literal as is.
                                lit.value += '\\';
                                lit.value += input.front();
                                input.remove_prefix(1);
                            }

                            if (input.starts_with(quote))
                            {
                                input.remove_prefix(1);
                                break; // Ending quote.
                            }

                            // Add the character to the literal.
                            lit.value += input.front();
                            input.remove_prefix(1);
                        }
                    }
                    else
                    {
                        // Get the delimiter.
                        while (true)
                        {
                            if (input.empty())
                                return input = input_at_start_of_literal, ret = ParseError{.message = "Unterminated opening delimiter of a raw string literal."}, ret;

                            if (ConsumePunctuation(input, "("))
                                break; // End of delimiter.

                            // If not a valid delimiter character...
                            // Valid characters are as specified in https://eel.is/c++draft/tab:lex.charset.basic minus those in https://eel.is/c++draft/lex.string#nt:d-char
                            if (!(IsAlpha(input.front()) || IsDigit(input.front()) || std::string_view("!\"#$%&\')*+,-./:;<=>?@[]^_`{|}~").find(input.front()) != std::string_view::npos))
                                return ret = ParseError{.message = "Invalid character in a raw string literal delimiter."}, ret;

                            if (lit.raw_string_delim.size() >= 16)
                            {
                                input.remove_prefix(1);
                                return ret = ParseError{.message = "Raw string literal delimiter is too long."}, ret;
                            }

                            lit.raw_string_delim += input.front();
                            input.remove_prefix(1);
                        }

                        // Parse the string.
                        while (true)
                        {
                            if (input.empty())
                                return input = input_at_start_of_literal, ret = ParseError{.message = "Unterminated raw string literal."}, ret;

                            if (input.front() == ')' && input.size() >= lit.raw_string_delim.size() + 2 && input[lit.raw_string_delim.size()+1] == '"' && input.substr(1, lit.raw_string_delim.size()) == lit.raw_string_delim)
                            {
                                // Found the final delimiter, stop.
                                input.remove_prefix(lit.raw_string_delim.size() + 2);
                                break;
                            }

                            lit.value += input.front();
                            input.remove_prefix(1);
                        }
                    }

                    // Parse the literal suffix if any.
                    if (!input.empty() && IsNonDigitIdentifierChar(input.front()))
                    {
                        do
                        {
                            lit.literal_suffix += input.front();
                            input.remove_prefix(1);
                        }
                        while (!input.empty() && IsIdentifierChar(input.front()));
                    }

                    ret_expr.tokens.emplace_back(std::move(lit));
                    continue;
                }
            }

            { // List in brackets.
                PseudoExprList list;

                std::string_view closing_bracket;
                if (ConsumePunctuation(input, "("))
                    list.kind = PseudoExprList::Kind::parentheses, closing_bracket = ")";
                else if (ConsumePunctuation(input, "{"))
                    list.kind = PseudoExprList::Kind::curly,       closing_bracket = "}";
                else if (ConsumePunctuation(input, "["))
                    list.kind = PseudoExprList::Kind::square,      closing_bracket = "]";

                if (!closing_bracket.empty())
                {
                    // Now we're sure this is a list.

                    TrimLeadingWhitespace(input);

                    // Is not empty?
                    if (!ConsumePunctuation(input, closing_bracket))
                    {
                        // Parse the elements.
                        while (true)
                        {
                            auto expr_result = ParsePseudoExpr(input);
                            if (auto error = std::get_if<ParseError>(&expr_result))
                                return ret = *error, ret;

                            list.elems.push_back(std::move(std::get<PseudoExpr>(expr_result)));
                            TrimLeadingWhitespace(input);

                            if (ConsumePunctuation(input, closing_bracket))
                                break; // End of list.

                            if (ConsumePunctuation(input, ","))
                            {
                                bool allow_trailing_comma = list.kind == PseudoExprList::Kind::curly;
                                if (allow_trailing_comma)
                                {
                                    TrimLeadingWhitespace(input);
                                    if (ConsumePunctuation(input, closing_bracket))
                                    {
                                        list.has_trailing_comma = true;
                                        break; // End of list.
                                    }
                                }

                                continue; // Next element.
                            }

                            // Invalid syntax at this point.
                            const char *error = nullptr;
                            switch (list.kind)
                            {
                                case PseudoExprList::Kind::parentheses: error = "Expected expression or `)` or `,`."; break;
                                case PseudoExprList::Kind::curly:       error = "Expected expression or `}` or `,`."; break;
                                case PseudoExprList::Kind::square:      error = "Expected expression or `]` or `,`."; break;
                            }

                            return ret = ParseError{.message = error}, ret;
                        }
                    }

                    ret_expr.tokens.emplace_back(std::move(list));
                    continue;
                }
            }

            { // Template argument list.
                auto arglist_result = ParseTemplateArgumentList(input);
                if (auto error = std::get_if<ParseError>(&arglist_result))
                    return ret = *error, ret;
                auto &arglist_opt = std::get<std::optional<TemplateArgumentList>>(arglist_result);
                if (arglist_opt)
                {
                    ret_expr.tokens.emplace_back(std::move(*arglist_opt));
                    continue;
                }

                // If we're here, this means there was no `<` at all, so we continue.
            }

            { // `SimpleType`, which includes identifiers.
                auto type_result = ParseSimpleType(input, ParseSimpleTypeFlags::allow_arbitrary_names);
                if (auto error = std::get_if<ParseError>(&type_result))
                    return ret = *error, ret;

                auto &type = std::get<SimpleType>(type_result);
                if (!type.IsEmpty())
                {
                    ret_expr.tokens.emplace_back(std::move(std::get<SimpleType>(type_result)));
                    continue;
                }
            }

            // Make sure only punctuation remains at this point.
            assert(input.empty() || !IsIdentifierChar(input.front()));

            { // Punctuation. This must be last, this catches all unknown tokens.
                PunctuationToken punct;
                using namespace std::string_view_literals;

                bool found = false;

                // Some known tokens.
                for (std::string_view token : {
                    // All multicharacter tokens from: https://eel.is/c++draft/lex.operators#nt:operator-or-punctuator
                    // Excluding identifier-like operator names, we don't bother supporting those. (Because then what about all the other keywords
                    //   what were catched by `SimpleType`? Screw that.)
                    // Also excluding operators (that can be after `operator`, these are handled below).
                    "<:"sv, ":>"sv, "<%"sv , "%>"sv , "..."sv,
                    "::"sv, ".*"sv,
                })
                {
                    if (ConsumePunctuation(input, token))
                    {
                        found = true;
                        punct.value = token;
                        break;
                    }
                }

                // Handle operator tokens.
                // `ConsumeOperatorToken` can accept `()` and `[]`, which we don't care about, but they should've been already handled
                //   by the "list" token type above.
                // Rejecting single-character tokens here to avoid `++` being split into `+ +`.
                if (std::string_view op_token; !found && ConsumeOperatorToken(input, op_token, ConsumeOperatorTokenFlags::reject_single_character_operators))
                {
                    found = true;
                    punct.value = op_token;
                }

                if (!found)
                {
                    // Didn't find any multicharacter tokens, add just one character and stop.
                    punct.value = input.substr(0, 1);
                    input.remove_prefix(1);
                }

                ret_expr.tokens.emplace_back(std::move(punct));
            }
        }
    }


    // Not setting any flags is an error.
    enum class ParseDeclFlags
    {
        // Accept unnamed declarations.
        accept_unnamed = 1 << 0,
        // Accept named declarations with unqualified names only.
        accept_unqualified_named = 1 << 1,
        // Accept named declarations with qualified names.
        // This requires `accept_unqualified_named` to also be set! Prefer `accept_all_named` for this reason.
        accept_qualified_named = 1 << 2,
        // Accept named declarations with both unqualified and qualified names.
        accept_all_named = accept_unqualified_named | accept_qualified_named,

        accept_everything = accept_unnamed | accept_all_named,

        // --- Those are primarily for internal use:

        // Only consider declarations with non-empty return types.
        force_non_empty_return_type = 1 << 3,

        // Only consider declarations with empty return types.
        // This requires at least one `accept_..._named`.
        force_empty_return_type = 1 << 4,

        // This is for target types of conversion operators.
        // Accept only the declarators that would be to the left of a variable name, stop on those that would be to the right.
        // Also don't accept `(`, and don't accept any names.
        // This shouldn't be used with any `accept_...` other than `accept_unnamed`.
        accept_unnamed_only_left_side_declarators_without_parens = 1 << 5,
    };
    CPPDECL_FLAG_OPERATORS(ParseDeclFlags)
    [[nodiscard]] CPPDECL_CONSTEXPR bool DeclFlagsAcceptName(ParseDeclFlags flags, const QualifiedName &name)
    {
        if (name.IsEmpty())
            return bool(flags & ParseDeclFlags::accept_unnamed);
        else if (name.IsQualified())
            return bool(flags & ParseDeclFlags::accept_qualified_named);
        else
            return bool(flags & ParseDeclFlags::accept_unqualified_named);
    }

    using ParseDeclResult = std::variant<MaybeAmbiguousDecl, ParseError>;
    // Parses a declaration (named or unnamed), returns `ParseError` on failure.
    // Tries to resolve ambiguities based on `flags`, and based on the amount of characters consumed (more is better).
    // If any ambiguities remain after that, returns one of them, preferring those without redundant parentheses (preferring to interpret them
    //   as function parameters), sets `.IsAmbiguous() == true` in the result, and attaches the ambiguous alternatives
    //   (see `.ambiguous_alternative`). Note that ambiguities can happen not only at the top level, but also in function parameters. `.IsAmbiguous()`
    //   checks for that recursively.
    [[nodiscard]] CPPDECL_CONSTEXPR ParseDeclResult ParseDecl(std::string_view &input, ParseDeclFlags flags)
    {
        const std::string_view input_before_decl = input;

        ParseDeclResult ret;
        MaybeAmbiguousDecl &ret_decl = std::get<MaybeAmbiguousDecl>(ret);

        { // Make sure the flags are ok.
            // If "qualified names" is set, "unqualified names" must also be set.
            if (!bool(flags & (ParseDeclFlags::accept_unnamed | ParseDeclFlags::accept_all_named)))
                return ret = ParseError{.message = "Bad usage, invalid flags: Must permit unnamed and/or named declarations."}, ret;
            if (bool(flags & ParseDeclFlags::force_empty_return_type) && !bool(flags & ParseDeclFlags::accept_all_named))
                return ret = ParseError{.message = "Bad usage, invalid flags: Empty return type is only allowed for named declarations."}, ret;
            if (bool(flags & ParseDeclFlags::accept_unnamed_only_left_side_declarators_without_parens) && bool(flags & ParseDeclFlags::accept_all_named))
                return ret = ParseError{.message = "Bad usage, invalid flags: 'Unnamed declarators without parens' mode can only be used with unnamed declarations."}, ret;

            // Intentionally don't check that `allow_empty_return_type_in_unlikely_cases` implies any of `allow_..._named` to allow
            //   setting it even when not needed.
        }


        // First declare the declarator stack.
        // It's quite early, since we didn't parse the decl-specifier-seq yet, but parsing that can immediately emit
        //   a single member-pointer modifier, and that must be pushed to the stack rather than directly to the return type.

        struct OpenParen
        {
            std::string_view input; // The remaining input starting with this `(`.
            MaybeAmbiguousDecl ret_backup; // The backup of `ret_decl` at this position.
        };

        struct DeclaratorStackEntry
        {
            using Var = std::variant<OpenParen, Pointer, Reference, MemberPointer>;
            Var var;

            std::string_view location; // Copy of the input string right BEFORE this was parsed.
        };

        // We don't normally pop from this (except when trying different parsing strategies to parse a function or something).
        // Instead we modify `declarator_stack_pos`.
        std::vector<DeclaratorStackEntry> declarator_stack;


        // We parse what looks like the variable name into this.
        ParseQualifiedNameResult candidate_decl_name;
        std::string_view input_before_candidate_decl_name;


        const bool force_empty_return_type = (flags & ParseDeclFlags::force_empty_return_type) == ParseDeclFlags::force_empty_return_type;

        bool stop_because_found_name = false;

        // Parse the decl-specifier-seq. This can also fill some extra data... (A single member pointer, or a declaration name.)
        // Note that `force_empty_return_type` shouldn't skip specifiers that are not a part of the return type,
        //   but we don't have any at the moment.
        if (!force_empty_return_type)
        {
            while (true)
            {
                // `ParseQualifiedName` would automatically trim this, but I want to store the correct position for the error message, if we get one.
                TrimLeadingWhitespace(input);

                const auto input_before_parse = input;
                ParseQualifiedNameResult result = ParseQualifiedName(input, ParseQualifiedNameFlags::only_valid_types);
                if (auto error = std::get_if<ParseError>(&result))
                    return ret = *error, ret;

                if (auto memptr = std::get_if<MemberPointer>(&result))
                {
                    if (ret_decl.type.IsEmpty())
                    {
                        input = input_before_parse;
                        return ret = ParseError{.message = "Expected the pointee type before the member pointer."}, ret;
                    }
                    // Note, we're pushing to the declarator stack, not directly to the result. That would produce a wrong order.
                    declarator_stack.emplace_back(std::move(*memptr), input_before_parse);
                    break;
                }

                QualifiedName &name = std::get<QualifiedName>(result);

                if (name.IsEmpty())
                    break;

                auto adding_name_result = TryAddNameToType(ret_decl.type.simple_type, name, {});
                if (auto error = std::get_if<ParseError>(&adding_name_result))
                    return ret = *error, input = input_before_parse, ret;
                bool name_added = std::get<bool>(adding_name_result);

                if (!name_added)
                {
                    // This is probably a variable name (or function name) at this point.

                    if (!DeclFlagsAcceptName(flags, name))
                    {
                        // We didn't expect a variable name (or expected an unqualified one and got qualified).
                        // Roll it back and stop. This isn't an error here (for no particular reason, it just seems to make sense?).
                        // But it is an error down below when we're inside of a declarator.
                        input = input_before_parse;

                        // Don't `return` immediately, we also need to add the `"int"` to the type below. Then we return.
                        stop_because_found_name = true;
                        break;
                    }
                    else
                    {
                        candidate_decl_name = std::move(name);
                        input_before_candidate_decl_name = input_before_parse;
                        break;
                    }
                }
            }
        }

        // Add implcit `int` if we have `unsigned` or `signed`. And set the `implied_int` flag to indicate that.
        if (ret_decl.type.IsEmptyUnsafe())
        {
            if (bool(ret_decl.type.simple_type.flags & (SimpleTypeFlags::unsigned_ | SimpleTypeFlags::explicitly_signed)))
            {
                ret_decl.type.simple_type.flags |= SimpleTypeFlags::implied_int;
                ret_decl.type.simple_type.name.parts.push_back(UnqualifiedName{.var = "int", .template_args = {}});
            }
        }

        // Stop if we found a variable name after this.
        // We do this after adding the implicit `int` above.
        if (stop_because_found_name)
            return ret;


        // If the type is empty... And if we don't allow empty types right now.
        // We can allow those for constructors, destructors and conversion operators.
        const bool allow_empty_simple_type = !bool(flags & ParseDeclFlags::force_non_empty_return_type) && bool(flags & ParseDeclFlags::accept_all_named);
        if (!allow_empty_simple_type && ret_decl.type.simple_type.IsEmpty())
            return ret = ParseError{.message = "Expected a type."}, ret;


        // Now the declarators:

        bool have_any_parens_in_declarator_on_initial_parse = false;

        // Is this a target type of a conversion operator?
        // Then we don't accept the declarators that go after the variable name,
        //   and moreover stop at any `(` whatsoever.
        // This implies `accept_unnamed`.
        bool left_side_only_and_no_parens = bool(flags & ParseDeclFlags::accept_unnamed_only_left_side_declarators_without_parens);

        // If we didn't already get a variable name from parsing the decl-specifier-seq, parse until we find one, or until we're sure there's none.
        if (std::get<QualifiedName>(candidate_decl_name).IsEmpty())
        {
            while (true)
            {
                TrimLeadingWhitespace(input);
                if (!left_side_only_and_no_parens && input.starts_with('('))
                {
                    declarator_stack.emplace_back(OpenParen{.input = input, .ret_backup = ret_decl}, input);
                    input.remove_prefix(1);
                    have_any_parens_in_declarator_on_initial_parse = true;
                    continue;
                }
                if (input.starts_with('*'))
                {
                    if (ret_decl.type.simple_type.IsEmpty())
                        return ret = ParseError{.message = bool(flags & ParseDeclFlags::force_empty_return_type) ? "Expected a name." : "Expected a type or a name."}, ret;

                    const std::string_view input_at_ptr = input;

                    input.remove_prefix(1);
                    Pointer ptr;

                    auto parsed_quals = ParseCvQualifiers(input);
                    if (auto error = std::get_if<ParseError>(&parsed_quals))
                        return ret = *error, ret;
                    ptr.quals = std::get<CvQualifiers>(parsed_quals);

                    declarator_stack.emplace_back(std::move(ptr), input_at_ptr);
                    continue;
                }
                if (input.starts_with('&'))
                {
                    if (ret_decl.type.simple_type.IsEmpty())
                        return ret = ParseError{.message = bool(flags & ParseDeclFlags::force_empty_return_type) ? "Expected a name." : "Expected a type or a name."}, ret;

                    const std::string_view input_at_ref = input;

                    Reference ref;
                    ref.kind = ParseRefQualifier(input);

                    const auto input_at_quals = input;
                    auto parsed_quals = ParseCvQualifiers(input);
                    if (auto error = std::get_if<ParseError>(&parsed_quals))
                        return ret = *error, ret;
                    ref.quals = std::get<CvQualifiers>(parsed_quals);

                    if (bool(ref.quals & CvQualifiers::const_))
                        return input = input_at_quals, TrimLeadingWhitespace(input), ret = ParseError{.message = "References can't be const-qualified."}, ret;
                    if (bool(ref.quals & CvQualifiers::volatile_))
                        return input = input_at_quals, TrimLeadingWhitespace(input), ret = ParseError{.message = "References can't be volatile-qualified."}, ret;

                    declarator_stack.emplace_back(std::move(ref), input_at_ref);
                    continue;
                }


                // Now what looks like the variable name:

                // Trimming whitespace would happen automatically in `ParseQualifiedName` anyway,
                //   but I want to record the location after the whitespace.
                TrimLeadingWhitespace(input);

                input_before_candidate_decl_name = input;
                candidate_decl_name = ParseQualifiedName(input, ParseQualifiedNameFlags::allow_unqualified_destructors | ParseQualifiedNameFlags::only_valid_nontypes);
                if (auto error = std::get_if<ParseError>(&candidate_decl_name))
                    return ret = *error, ret;

                if (auto memptr = std::get_if<MemberPointer>(&candidate_decl_name))
                {
                    if (ret_decl.type.simple_type.IsEmpty())
                    {
                        input = input_before_candidate_decl_name;
                        // The "else" message seems to be unreachable, we already check for the same thing earlier.
                        return ret = ParseError{.message = bool(flags & ParseDeclFlags::force_empty_return_type) ? "Expected a name, but found a member pointer." : "Expected the pointee type before the member pointer."}, ret;
                    }

                    declarator_stack.emplace_back(std::move(*memptr), input_before_candidate_decl_name);
                    candidate_decl_name = {}; // Nuke the name. It's checked before this loop, so it should be empty, just in case.
                    continue;
                }

                // If this is a conversion operator target type, undo parsing the name and nope out of here.
                if (left_side_only_and_no_parens)
                {
                    candidate_decl_name = {};
                    input = input_before_candidate_decl_name;
                    break; // Nope out of here.
                }

                // Now we break with possibily non-empty `candidate_decl_name`, to continue handling it in `ParseRemainingDecl`.
                // We must do that because any errors in it must cause a reparse.
                break;
            }
        }


        auto ParseRemainingDecl = [&]() -> ParseDeclResult
        {
            // Must not touch `ret` in this function. This variable shadows it.
            [[maybe_unused]] constexpr int ret = -1;


            // Returns a message saying "X must be a function" for the kind of `ret_decl.name`. Or null if it's a plain name or empty.
            auto MakeMustBeAFunctionError = [&]() -> const char *
            {
                if (ret_decl.name.parts.empty())
                    return nullptr;
                return std::visit(Overload{
                    [](const std::string &) {return (const char *)nullptr;},
                    [](const OverloadedOperator &) {return "Overloaded operator must be a function.";},
                    [](const ConversionOperator &) {return "Conversion operator must be a function.";},
                    [](const UserDefinedLiteral &) {return "User-defined literal must be a function.";},
                    [](const DestructorName &) {return "Destructor must be a function.";},
                }, ret_decl.name.parts.back().var);
            };


            // Continue parsing the variable name, if any.
            // This can only happen on the first parse. Repeated attempts will always have an empty name.
            QualifiedName &name = std::get<QualifiedName>(candidate_decl_name);
            if (!name.IsEmpty())
            {
                if (IsTypeRelatedKeyword(name.AsSingleWord()))
                {
                    input = input_before_candidate_decl_name;
                    return ParseError{.message = "Can't add this keyword to the preceding type."};
                }

                if (!DeclFlagsAcceptName(flags, name))
                {
                    input = input_before_candidate_decl_name;
                    if (have_any_parens_in_declarator_on_initial_parse)
                    {
                        if (!bool(flags & ParseDeclFlags::accept_all_named))
                            return ParseError{.message = "Expected only a type but got a named declaration."};
                        else if (bool(flags & ParseDeclFlags::accept_unqualified_named))
                            return ParseError{.message = "Expected an unqualified name but got a qualified one."};
                        else
                            return ParseError{.message = "Expected a qualified name but got an unqualified one."};
                    }
                    return ret_decl; // Refuse to parse the rest, the declaration ends here. Not emit a hard error either, maybe it's just junk?
                }

                ret_decl.name = std::move(name);

                // Complain if this should be a function (because it's an operator or something like that) but already isn't one.
                if (!declarator_stack.empty() || !ret_decl.type.modifiers.empty())
                {
                    if (auto error = MakeMustBeAFunctionError())
                    {
                        input = input_before_candidate_decl_name;
                        return ParseError{.message = error};
                    }
                }

                // Complain if this should return an empty type but doesn't.
                // This isn't a strict check, we can miss something. (E.g. `int MyClass()` is a constructor,
                //   but we can't possibly tell.)
                if (!ret_decl.type.simple_type.IsEmpty() && ret_decl.name.IsFunctionNameRequiringEmptyReturnType() == QualifiedName::EmptyReturnType::yes)
                {
                    input = input_before_candidate_decl_name;
                    return ParseError{.message = std::visit(Overload{
                        [](const std::string &) {return "A constructor must have no return type.";},
                        [](const OverloadedOperator &) {assert(false); return (const char *)nullptr;},
                        [](const ConversionOperator &) {return "A conversion operator must have no return type.";},
                        [](const UserDefinedLiteral &) {assert(false); return (const char *)nullptr;},
                        [](const DestructorName &) {return "A destructor must have no return type.";},
                    }, ret_decl.name.parts.back().var)};
                }

                // Complain if this should return something but doesn't.
                if (ret_decl.type.simple_type.IsEmpty())
                {
                    auto guess = ret_decl.name.IsFunctionNameRequiringEmptyReturnType();
                    if (guess != QualifiedName::EmptyReturnType::yes && guess != QualifiedName::EmptyReturnType::maybe_unqual_constructor)
                    {
                        input = input_before_candidate_decl_name;
                        return ParseError{.message = "Expected a type."};
                    }
                }
            }

            // If we have no type and no name, complain.
            // This can only happen here if we're allowing empty return types.
            if (ret_decl.type.simple_type.IsEmpty() && ret_decl.name.IsEmpty())
            {
                input = input_before_decl;
                TrimLeadingWhitespace(input);

                if (force_empty_return_type)
                    return ParseError{.message = "Expected a name."};
                else
                    return ParseError{.message = "Expected a type or a name."};
            }

            // If we have no name but wanted one, complain.
            if (!bool(flags & ParseDeclFlags::accept_unnamed) && ret_decl.name.IsEmpty())
            {
                return ParseError{.message = "Expected a name."};
            }


            std::size_t declarator_stack_pos = declarator_stack.size();

            // Make sure we don't have empty `()` parentheses without a declarator between them.
            // I'm not sure if it's possible to trigger this at all without falling back to the function parameter list parsing.
            TrimLeadingWhitespace(input);
            if (ret_decl.name.IsEmpty() && declarator_stack_pos > 0 && std::holds_alternative<OpenParen>(declarator_stack[declarator_stack_pos-1].var) && input.starts_with(')'))
                return ParseError{.message = "Parentheses around a declarator can't be empty."};


            // Decrements `declarator_stack_pos` and processes the element removed in this manner.
            // Returns true when popping `()`.
            // Writes an error to `out_error` and returns true on failure.
            auto PopDeclaratorFromStack = [&](std::optional<ParseError> &out_error) -> bool
            {
                DeclaratorStackEntry &this_elem = declarator_stack[declarator_stack_pos-1];

                bool done = std::visit([&]<typename T>(T &elem)
                {
                    // Check for illegal modifier combinations.
                    // Here we only check the modifiers that appear to the left of the name.
                    if (!ret_decl.type.modifiers.empty())
                    {
                        if (std::holds_alternative<Reference>(this_elem.var))
                        {
                            if (std::holds_alternative<Pointer>(ret_decl.type.modifiers.back().var))
                            {
                                out_error = ParseError{.message = "Pointers to references are not allowed."};
                                input = this_elem.location;
                                return true;
                            }
                            if (std::holds_alternative<MemberPointer>(ret_decl.type.modifiers.back().var))
                            {
                                out_error = ParseError{.message = "Member pointers to references are not allowed."};
                                input = this_elem.location;
                                return true;
                            }
                            if (std::holds_alternative<Array>(ret_decl.type.modifiers.back().var))
                            {
                                out_error = ParseError{.message = "Arrays of references are not allowed."};
                                input = this_elem.location;
                                return true;
                            }
                        }
                    }

                    if constexpr (std::is_same_v<T, OpenParen>)
                    {
                        return true;
                    }
                    else
                    {
                        ret_decl.type.modifiers.emplace_back(std::move(elem));
                        return false;
                    }
                }, this_elem.var);

                declarator_stack_pos--;

                return done;
            };

            // Continue parsing to the end of the declaration.
            if (!left_side_only_and_no_parens)
            {
                while (true)
                {
                    TrimLeadingWhitespace(input);

                    const std::string_view input_before_modifier = input;

                    if (input.starts_with(')'))
                    {
                        bool done = false;
                        while (!done)
                        {
                            if (declarator_stack_pos == 0)
                                return ret_decl; // Extra `)` after input, but this is not an error. This is important e.g. for the last function parameter.

                            std::optional<ParseError> error;
                            done = PopDeclaratorFromStack(error);
                            if (error)
                                return *error;
                        }

                        input.remove_prefix(1);
                        continue;
                    }

                    if (ConsumePunctuation(input, "["))
                    {
                        // Check for banned element type modifiers.
                        if (!ret_decl.type.modifiers.empty())
                        {
                            if (std::holds_alternative<Function>(ret_decl.type.modifiers.back().var))
                            {
                                input = input_before_modifier;
                                return ParseError{.message = "Function return type can't be an array."};
                            }
                        }

                        // Complain if the return type is empty.
                        if (ret_decl.type.simple_type.IsEmpty())
                        {
                            input = input_before_modifier;
                            bool force_empty = bool(flags & ParseDeclFlags::force_empty_return_type);
                            bool force_non_empty = bool(flags & ParseDeclFlags::force_non_empty_return_type);
                            return ParseError{.message = force_empty || (!force_non_empty && ret_decl.name.IsFunctionNameRequiringEmptyReturnType() == QualifiedName::EmptyReturnType::yes) ? "Assumed this was a function declaration with an empty return type, but found an array." : "Missing element type for the array."};
                        }

                        auto expr_result = ParsePseudoExpr(input);
                        if (auto error = std::get_if<ParseError>(&expr_result))
                            return *error;

                        Array arr;
                        arr.size = std::move(std::get<PseudoExpr>(expr_result));

                        if (!ConsumePunctuation(input, "]"))
                            return ParseError{.message = "Expected `]` after array size."};

                        ret_decl.type.modifiers.emplace_back(std::move(arr));
                        continue;
                    }

                    if (ConsumePunctuation(input, "("))
                    {
                        // Check for banned return type modifiers.
                        if (!ret_decl.type.modifiers.empty())
                        {
                            if (std::holds_alternative<Array>(ret_decl.type.modifiers.back().var))
                            {
                                input = input_before_modifier;
                                return ParseError{.message = "Arrays of functions are not allowed."};
                            }
                            if (std::holds_alternative<Function>(ret_decl.type.modifiers.back().var))
                            {
                                input = input_before_modifier;
                                return ParseError{.message = "Function return type can't be a function."};
                            }
                        }

                        Function func;

                        TrimLeadingWhitespace(input);

                        if (ConsumePunctuation(input, "..."))
                        {
                            TrimLeadingWhitespace(input);
                            if (!ConsumePunctuation(input, ")"))
                                return ParseError{.message = "Expected `)` after a C-style variadic parameter."};
                            func.c_style_variadic = true;
                        }
                        else if (ConsumePunctuation(input, ")"))
                        {
                            // Empty argument list.
                        }
                        else
                        {
                            TrimLeadingWhitespace(input);

                            { // Check for C-style `(void)` parameter list.
                                func.c_style_void_params = false;
                                std::string_view input_copy = input;
                                if (ConsumeWord(input_copy, "void"))
                                {
                                    TrimLeadingWhitespace(input_copy);
                                    if (input_copy.starts_with(')'))
                                    {
                                        input = input_copy;
                                        input.remove_prefix(1);
                                        func.c_style_void_params = true;
                                    }
                                }
                            }

                            // Parse the parameter list properly.
                            if (!func.c_style_void_params)
                            {
                                while (true)
                                {
                                    const auto input_before_param = input;

                                    auto param_result = ParseDecl(input, ParseDeclFlags::accept_unnamed | ParseDeclFlags::accept_unqualified_named | ParseDeclFlags::force_non_empty_return_type);
                                    if (auto error = std::get_if<ParseError>(&param_result))
                                        return *error;
                                    MaybeAmbiguousDecl &param_decl = std::get<MaybeAmbiguousDecl>(param_result);

                                    if (param_decl.IsEmpty())
                                        return input = input_before_param, ParseError{.message = "Expected a function parameter."};

                                    // Propagate the ambiguity flag.
                                    ret_decl.has_nested_ambiguities = param_decl.IsAmbiguous();
                                    func.params.push_back(std::move(param_decl));

                                    if (ConsumePunctuation(input, ")"))
                                    {
                                        break; // No more parameters.
                                    }
                                    if (ConsumePunctuation(input, ","))
                                    {
                                        // Check for C-style variadic.
                                        TrimLeadingWhitespace(input);
                                        if (ConsumePunctuation(input, "..."))
                                        {
                                            TrimLeadingWhitespace(input);
                                            if (!ConsumePunctuation(input, ")"))
                                                return ParseError{.message = "Expected `)` after a C-style variadic parameter."};

                                            func.c_style_variadic = true;
                                            break; // No more parameters.
                                        }

                                        continue; // Have another parameter.
                                    }
                                    if (ConsumePunctuation(input, "..."))
                                    {
                                        // C-style variadic without the comma.

                                        TrimLeadingWhitespace(input);
                                        if (!ConsumePunctuation(input, ")"))
                                            return ParseError{.message = "Expected `)` after a C-style variadic parameter."};

                                        func.c_style_variadic = true;
                                        func.c_style_variadic_without_comma = true;
                                        break; // No more parameters.
                                    }

                                    return ParseError{.message = "Expected `)` or `,` or `...` in function parameter list."};
                                }
                            }
                        }

                        // Parse cv-qualifiers.
                        auto cvref_result = ParseCvQualifiers(input);
                        if (auto error = std::get_if<ParseError>(&cvref_result))
                            return *error;
                        func.cv_quals = std::get<CvQualifiers>(cvref_result);

                        // Parse ref-qualifiers. This automatically removes leading whitespace.
                        func.ref_qual = ParseRefQualifier(input);
                        TrimLeadingWhitespace(input);

                        // Noexcept?
                        if (ConsumeWord(input, "noexcept"))
                        {
                            TrimLeadingWhitespace(input);
                            func.noexcept_ = true;
                            // Not trimming trailing whitespace here, it's not strictly necessary.
                        }

                        // Trailing return type?
                        const std::string_view input_before_trailing_arrow = input;
                        if (ConsumePunctuation(input, "->"))
                        {
                            // Complain if the trailing return type is inside of parentheses.
                            // In particular this ensures that only one function modifier can use the trailing return type syntax,
                            //   because all but one function modifiers will parenthesized.
                            // Clang and MSVC reject this (Clang straight up tells about the parentheses), but GCC doesn't.
                            // I'm not gonna suppor this for GCC alone.
                            if (std::any_of(declarator_stack.begin(), declarator_stack.begin() + std::ptrdiff_t(declarator_stack_pos), [](const DeclaratorStackEntry &e){return std::holds_alternative<OpenParen>(e.var);}))
                            {
                                input = input_before_trailing_arrow;
                                return ParseError{.message = "Trailing return type can't be nested in parentheses."};
                            }

                            // Complain if the return type wasn't `auto` before.
                            // Note the `.simple_type.` part. We don't want to reject non-empty `.modifiers`, because
                            //   the ones we have at this parsing stage don't apply to the return type, but rather to the function itself.
                            if (
                                ret_decl.type.simple_type.AsSingleWord() != "auto" ||
                                // For the same reason, make sure the remaining declarator list is empty,
                                //   since otherwise it would add unwanted stuff to our `auto` type.
                                // NOTE: In theory it would be fine if it only contained parentheses, but we also reject any parentheses above,
                                //   so no point in respecting them here.
                                declarator_stack_pos > 0
                            )
                            {
                                input = input_before_trailing_arrow;
                                return ParseError{.message = "A trailing return type is specified, but the previousy specified return type wasn't `auto`."};
                            }

                            const std::string_view input_before_type = input;

                            auto ret_result = ParseType(input);
                            if (auto error = std::get_if<ParseError>(&ret_result))
                                return *error;

                            func.uses_trailing_return_type = true;

                            // Replace the return type with the new one.

                            auto &new_type = std::get<Type>(ret_result);

                            // Check that it's a valid return type.
                            // The logic here is duplicated between trailing and non-trailing return types. Sad, but whatever.
                            if (new_type.Is<Array>())
                            {
                                input = input_before_type;
                                TrimLeadingWhitespace(input);
                                return ParseError{.message = "Function return type can't be an array."};
                            }
                            if (new_type.Is<Function>())
                            {
                                input = input_before_type;
                                TrimLeadingWhitespace(input);
                                return ParseError{.message = "Function return type can't be a function."};
                            }

                            ret_decl.type.simple_type = std::move(new_type.simple_type);
                            // Append modifiers to the end, after the "function" modifier.
                            ret_decl.type.modifiers.emplace_back(std::move(func));
                            ret_decl.type.modifiers.insert(ret_decl.type.modifiers.end(), std::make_move_iterator(new_type.modifiers.begin()), std::make_move_iterator(new_type.modifiers.end()));

                            continue;
                        }
                        else
                        {
                            ret_decl.type.modifiers.emplace_back(std::move(func));
                            continue;
                        }
                    }

                    break; // End of string or unknown syntax, nothing more to do.
                }
            }

            // If we're dealing with an empty return type, make sure this is actually a function.
            // Or a kind of name that's known to need no return type.
            if (ret_decl.type.simple_type.IsEmpty() && !ret_decl.type.Is<Function>())
            {
                // If we wanted to add support for things like `A` constructors without parens,
                // or `A::B` constructors (with different name components), that would be here.
                auto kind = ret_decl.name.IsFunctionNameRequiringEmptyReturnType();
                if (kind != QualifiedName::EmptyReturnType::yes)
                {
                    return ParseError{.message = "Expected a parameter list here."};
                }
            }

            // Comsume the rest of the declarator stack.
            while (declarator_stack_pos != 0)
            {
                std::optional<ParseError> error;
                bool found_open_paren = PopDeclaratorFromStack(error);
                if (error)
                    return *error;

                if (found_open_paren)
                    return ParseError{.message = "Expected `)` to match this one."};
            }

            return ret_decl;
        };


        // If we only accept named declarations, don't bother with reparse candidates, do everything in one go.
        // Parses after the first one are never named anyway.
        // But there's another source of ambiguities, which is empty vs non-empty return type. Don't stop yet if that's a possibility.
        if (!bool(flags & ParseDeclFlags::accept_unnamed) && (!allow_empty_simple_type || force_empty_return_type))
            return ParseRemainingDecl();


        struct CandidateResult
        {
            ParseDeclResult ret;
            std::string_view input; // The state of input after parsing.
        };

        std::vector<CandidateResult> candidates;

        // If we haven't tries with an empty return type yet, try now.
        // This does before even the first primary candidate, because we prefer later candidates in the loop below,
        //   so this gives this less priority.
        if (allow_empty_simple_type && !force_empty_return_type && !ret_decl.type.simple_type.IsEmpty())
        {
            std::string_view input_copy = input_before_decl;
            auto decl_result = ParseDecl(input_copy, flags | ParseDeclFlags::force_empty_return_type);

            if (auto error = std::get_if<ParseError>(&decl_result))
            {
                candidates.emplace_back().ret = *error;
                candidates.back().input = input_copy;
            }
            else
            {
                // Unpack the ambiguous results back into a flat list, in reverse order (because the top-level candidate is the most probable one,
                //   so it should be last, because again, the loop below gives the later candidates more priority.
                auto lambda = [&](auto &lambda, MaybeAmbiguousDecl &decl) -> void
                {
                    if (decl.ambiguous_alternative)
                        lambda(lambda, *decl.ambiguous_alternative);

                    // Don't want this stuff to propagate to the candidate.
                    decl.ambiguous_alternative = {};

                    candidates.emplace_back().ret = std::move(decl);
                    candidates.back().input = input_copy;
                };
                lambda(lambda, std::get<MaybeAmbiguousDecl>(decl_result));
            }
        }


        // Now the main remaining parsing branch.
        auto ret_backup = ret;
        candidates.emplace_back().ret = ParseRemainingDecl();
        candidates.back().input = input;
        ret = std::move(ret_backup);
        candidate_decl_name = {}; // Reset the name. It's only meaningful during the initial parse. All retries will always be unnamed.

        // If we do accept unnamed declarations, check every preceding `(` as a possible function parameter list.
        if (bool(flags & ParseDeclFlags::accept_unnamed))
        {
            while (!declarator_stack.empty())
            {
                auto paren = std::get_if<OpenParen>(&declarator_stack.back().var);
                if (paren)
                {
                    input = paren->input;
                    ret_decl = std::move(paren->ret_backup);
                }
                declarator_stack.pop_back();
                if (paren)
                {
                    ret_backup = ret;
                    candidates.emplace_back().ret = ParseRemainingDecl();
                    candidates.back().input = input;
                    ret = std::move(ret_backup);
                }
            }
        }

        std::size_t candidate_index = 0;

        // If there's more than one candidate, pick the best one.
        // We sort by successful parse, then by the amount of characters consumed (more is better, even on failure),
        //   then pick the latter candidate (normally there are only two ambiguous candidates, with the first one having redundant parentheses,
        //   so picking the second one makes more sense).
        // We always prefer successful parse, even if it consumed less characters than a failed one.
        // Other than that, both successful and failed parses are compared by the number of consumed characters, the more the better.
        bool ambiguous = false;
        if (candidates.size() > 1)
        {
            std::size_t min_unparsed_len = std::size_t(-1);
            bool have_successful_parse = false;

            for (std::size_t i = 0; const CandidateResult &c : candidates)
            {
                // Assert that `.ambiguous_alternative` is null. It shouldn't be set at this point.
                assert([&]{
                    auto *c = std::get_if<MaybeAmbiguousDecl>(&candidates[i].ret);
                    return !c || !c->ambiguous_alternative;
                }());

                // Prefer candidates that don't have errors in them and have less unparsed junk at the end.
                if (
                    // First successful parse is always taken.
                    (!have_successful_parse && std::holds_alternative<MaybeAmbiguousDecl>(c.ret)) ||
                    // Otherwise sort by the number of characters consumed.
                    (have_successful_parse == std::holds_alternative<MaybeAmbiguousDecl>(c.ret) && c.input.size() <= min_unparsed_len)
                )
                {
                    if (have_successful_parse)
                        ambiguous = c.input.size() == min_unparsed_len;

                    // We update the info even if the input size matches, see above for rationale.
                    min_unparsed_len = c.input.size();
                    candidate_index = i;
                    have_successful_parse = std::holds_alternative<MaybeAmbiguousDecl>(c.ret);
                }

                i++;
            }

            // If we have ambiguities, stack them into a linked list.
            if (ambiguous)
            {
                MaybeAmbiguousDecl *cur_candidate = &std::get<MaybeAmbiguousDecl>(candidates[candidate_index].ret);
                for (std::size_t i = candidate_index; i-- > 0;)
                {
                    if (auto decl = std::get_if<MaybeAmbiguousDecl>(&candidates[i].ret); decl && candidates[i].input.size() == min_unparsed_len)
                    {
                        cur_candidate->ambiguous_alternative = std::move(*decl);
                        cur_candidate = cur_candidate->ambiguous_alternative.get();
                    }
                }
            }
        }

        // Return the selected candidate.
        ret = std::move(candidates[candidate_index].ret);
        // Restore the parse state for that candidate too.
        input = candidates[candidate_index].input;
        return ret;
    }

    // A subset of `ParseDecl()` that rejects named declarations.
    // My current understanding is that rejecting names makes this never ambiguous, so we return only one type. There's an assert for that.
    [[nodiscard]] CPPDECL_CONSTEXPR ParseTypeResult ParseType(std::string_view &input, ParseTypeFlags flags)
    {
        ParseTypeResult ret;

        ParseDeclFlags decl_flags = ParseDeclFlags::accept_unnamed;
        if (bool(flags & ParseTypeFlags::only_left_side_declarators_without_parens))
            decl_flags |= ParseDeclFlags::accept_unnamed_only_left_side_declarators_without_parens;

        ParseDeclResult decl_result = ParseDecl(input, decl_flags);
        if (auto error = std::get_if<ParseError>(&decl_result))
            return ret = *error, ret;

        auto &decl = std::get<MaybeAmbiguousDecl>(decl_result);

        // If this fires, go make a `using MaybeAmbiguousType = MaybeAmbiguous<Type>;` typedef and start replacing most uses of `Type` with it.
        assert(!decl.ambiguous_alternative && "I thought type parsing can't be ambiguous.");

        std::get<Type>(ret) = std::move(decl.type);

        return ret;
    }

    // Parses a template argument list.
    // Returns null only if `input` (after skipping whitespace) doesn't start with `<`.
    [[nodiscard]] CPPDECL_CONSTEXPR ParseTemplateArgumentListResult ParseTemplateArgumentList(std::string_view &input)
    {
        ParseTemplateArgumentListResult ret;

        TrimLeadingWhitespace(input);
        const std::string_view input_before_list = input;
        if (!ConsumePunctuation(input, "<"))
            return ret; // No argument list here, return nullopt.

        TemplateArgumentList &ret_list = std::get<std::optional<TemplateArgumentList>>(ret).emplace();

        TrimLeadingWhitespace(input);

        if (!ConsumePunctuation(input, ">"))
        {
            while (true)
            {
                TemplateArgument new_arg;

                // Try a declaration (unnamed).
                bool decl_ok = false;
                const std::string_view input_before_arg = input;
                auto type_result = ParseType(input);
                if (auto type = std::get_if<Type>(&type_result))
                {
                    TrimLeadingWhitespace(input);
                    if (input.starts_with('>') || input.starts_with(','))
                    {
                        new_arg.var = std::move(*type);
                        decl_ok = true;
                    }
                }
                // Ignore any parse errors in `decl_result`, and ignore it completely if it's not followed by `>` or `,`.

                if (!decl_ok)
                {
                    input = input_before_arg;
                    auto expr_result = ParsePseudoExpr(input, ParsePseudoExprFlags::stop_on_gt_sign);
                    if (auto error = std::get_if<ParseError>(&expr_result))
                        return ret = *error, ret; // This is fatal.

                    auto &expr = std::get<PseudoExpr>(expr_result);
                    if (expr.IsEmpty())
                        return ret = ParseError{.message = "Expected template argument."}, ret;

                    new_arg.var = std::move(expr);
                }

                ret_list.args.push_back(std::move(new_arg));

                TrimLeadingWhitespace(input);
                if (ConsumePunctuation(input, ">"))
                    break;
                if (ConsumePunctuation(input, ","))
                    continue;

                if (input.empty())
                    return input = input_before_list, ret = ParseError{.message = "Unterminated template argument list."}, ret;

                // How did we get here?
                return ret = ParseError{.message = "Internal error: unparsed junk at the end of a template argument."}, ret;
            }
        }

        return ret;
    }
}
