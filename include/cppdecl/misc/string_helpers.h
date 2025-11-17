#pragma once

#include "cppdecl/misc/enum_flags.h"
#include "cppdecl/misc/platform.h"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <string_view>
#include <string>
#include <type_traits>

namespace cppdecl
{
    // --- Character classification:

    [[nodiscard]] constexpr bool IsWhitespace(char ch)
    {
        // Intentionally not adding `\v` here. Who uses that?
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    }
    // Remove any prefix whitespace from `str`.
    // Returns true if at least one removed.
    constexpr bool TrimLeadingWhitespace(std::string_view &str)
    {
        bool ret = false;
        while (!str.empty() && IsWhitespace(str.front()))
        {
            str.remove_prefix(1);
            ret = true;
        }
        return ret;
    }
    constexpr bool TrimTrailingWhitespace(std::string_view &str)
    {
        bool ret = false;
        while (!str.empty() && IsWhitespace(str.back()))
        {
            str.remove_suffix(1);
            ret = true;
        }
        return ret;
    }

    // A constexpr replacement for `std::to_string()`.
    [[nodiscard]] CPPDECL_CONSTEXPR std::string NumberToString(std::integral auto n)
    {
        std::string ret;

        bool negative = false;
        int digit = n % 10;
        n /= 10;
        if (digit < 0)
        {
            digit = -digit;
            n = -n; // Negate `n` after `/= 10` to handle the smallest possible number correctly.
            negative = true;
        }
        ret += char('0' + digit);

        while (n)
        {
            ret = char('0' + n % 10) + ret;
            n /= 10;
        }

        if (negative)
            ret = '-' + ret;

        return ret;
    }

    [[nodiscard]] constexpr bool IsAlphaLowercase(char ch)
    {
        return ch >= 'a' && ch <= 'z';
    }
    [[nodiscard]] constexpr bool IsAlphaUppercase(char ch)
    {
        return ch >= 'A' && ch <= 'Z';
    }
    [[nodiscard]] constexpr bool IsAlpha(char ch)
    {
        return IsAlphaLowercase(ch) || IsAlphaUppercase(ch);
    }
    // Whether `ch` is a letter or an other non-digit identifier character.
    [[nodiscard]] constexpr bool IsNonDigitIdentifierChar(char ch)
    {
        // `$` is non-standard, but seems to work on all compilers (sometimes you need to disable some warnings).
        return ch == '_' || ch == '$' || IsAlpha(ch);
    }
    [[nodiscard]] constexpr bool IsDigit(char ch)
    {
        return ch >= '0' && ch <= '9';
    }
    // Whether `ch` can be a part of an identifier.
    [[nodiscard]] constexpr bool IsIdentifierChar(char ch)
    {
        return IsNonDigitIdentifierChar(ch) || IsDigit(ch);
    }

    // Checks if `name` is a valid non-empty identifier, using `Is[NonDigit]IdentifierChar()`.
    [[nodiscard]] constexpr bool IsValidIdentifier(std::string_view name)
    {
        return !name.empty() && IsNonDigitIdentifierChar(name.front()) && std::all_of(name.begin() + 1, name.end(), IsIdentifierChar);
    }

    [[nodiscard]] constexpr bool IsBinDigit(char ch)
    {
        return ch == '0' || ch == '1';
    }
    [[nodiscard]] constexpr bool IsOctalDigit(char ch)
    {
        return ch >= '0' && ch <= '7';
    }
    [[nodiscard]] constexpr bool IsHexDigit(char ch)
    {
        return IsDigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
    }

    [[nodiscard]] constexpr char ToLower(char ch)
    {
        return IsAlphaUppercase(ch) ? ch + ('a' - 'A') : ch;
    }
    [[nodiscard]] constexpr char ToUpper(char ch)
    {
        return IsAlphaLowercase(ch) ? ch - ('a' - 'A') : ch;
    }
    constexpr void ToLower(std::string &s)
    {
        for (char &ch : s)
            ch = ToLower(ch);
    }
    constexpr void ToUpper(std::string &s)
    {
        for (char &ch : s)
            ch = ToUpper(ch);
    }

    // Is this a name of a built-in integral type?
    // `long long` (a multi-word name) isn't handled here.
    // `bool` also isn't handled here. We check it separately, because unlike those it can't have its signedness set explicitly.
    // `signed` and `unsigned` are also not here because in our system they are flags,
    //   they don't appear in type names (including standalone, we add `int` ourselves then).
    [[nodiscard]] constexpr bool IsTypeNameKeywordIntegral(std::string_view name)
    {
        return
            name == "char" ||
            name == "short" ||
            name == "int" ||
            name == "long" ||
            name == "__int128"; // Non-standard stuff. This must exist to allow us to parse `unsigned __int128` as a single type rather than as a variable declaration.
    }

    // This is a boolean type?
    // For simplicity we allow both the normal `bool` and the old C-style `_Bool`.
    [[nodiscard]] constexpr bool IsTypeNameKeywordBool(std::string_view name)
    {
        return
            name == "bool" ||
            name == "_Bool";
    }

    // Is this a name of a built-in floating-point type?
    // `long double` (a multi-word name) isn't handled here.
    [[nodiscard]] constexpr bool IsTypeNameKeywordFloatingPoint(std::string_view name)
    {
        return
            name == "float" ||
            name == "double" ||
            name == "__float128"; // Non-standard stuff. This must exist to allow us to parse `_Complex __int128` as a single type rather than as a variable declaration.
    }

    // Yeah. For consistency.
    [[nodiscard]] constexpr bool IsTypeNameKeywordVoid(std::string_view name)
    {
        return name == "void";
    }

    // Is `name` a type or a keyword related to types?
    // We use this to detect clearly invalid variable names that were parsed from types.
    [[nodiscard]] constexpr bool IsTypeRelatedKeyword(std::string_view name)
    {
        return
            IsTypeNameKeywordIntegral(name) ||
            IsTypeNameKeywordBool(name) ||
            IsTypeNameKeywordFloatingPoint(name) ||
            IsTypeNameKeywordVoid(name) ||
            name == "signed" ||
            name == "unsigned" ||
            name == "const" ||
            name == "volatile" ||
            name == "auto" ||
            // Not adding the C/nonstandard spelling `restrict` here, since this list is just for better error messages, and the lack of it isn't going
            //   to break its parsing or anything.
            name == "__restrict" ||
            name == "__restrict__" ||
            // Not adding the `complex`/`imaginary` spellings here from `complex.h`, as those would be too prone to name conflicts.
            name == "_Complex" ||
            name == "_Imaginary";
    }


    // Is `name` a keyword that is a literal?
    [[nodiscard]] constexpr bool IsLiteralConstantKeyword(std::string_view name)
    {
        return
            name == "true" ||
            name == "false" ||
            name == "nullptr";
    }

    // If `input` starts with word `word` (as per `.starts_with()`), removes that prefix and returns true.
    // Otherwise leaves it unchanged and returns false.
    [[nodiscard]] constexpr bool ConsumePunctuation(std::string_view &input, std::string_view word)
    {
        bool ret = input.starts_with(word);
        if (ret)
            input.remove_prefix(word.size());
        return ret;
    }
    // Same, but for trailing punctuation.
    [[nodiscard]] constexpr bool ConsumeTrailingPunctuation(std::string_view &input, std::string_view word)
    {
        bool ret = input.ends_with(word);
        if (ret)
            input.remove_suffix(word.size());
        return ret;
    }

    // Returns true if `input` starts with `word` followed by end of string or whitespace or
    //   a punctuation character (which is anything other `IsIdentifierChar()`).
    [[nodiscard]] constexpr bool StartsWithWord(std::string_view input, std::string_view word)
    {
        return (input.size() <= word.size() || !IsIdentifierChar(input[word.size()])) && input.starts_with(word);
    }
    [[nodiscard]] constexpr bool EndsWithWord(std::string_view input, std::string_view word)
    {
        return (input.size() <= word.size() || !IsIdentifierChar(input[input.size() - word.size() - 1])) && input.ends_with(word);
    }

    // If `input` starts with word `word` (as per `StartsWithWord`), removes that prefix and returns true.
    // Otherwise leaves it unchanged and returns false.
    [[nodiscard]] constexpr bool ConsumeWord(std::string_view &input, std::string_view word)
    {
        bool ret = StartsWithWord(input, word);
        if (ret)
            input.remove_prefix(word.size());
        return ret;
    }


    enum class ConsumeOperatorTokenFlags
    {
        // Mostly for internal use. Don't allow operators with a single character name.
        reject_single_character_operators = 1 << 0,
    };
    CPPDECL_FLAG_OPERATORS(ConsumeOperatorTokenFlags)

    // Tries to read an operator name token from `input`. On success returns true and makes the `out_token` point
    //   to that token (stored statically, so it will remain valid forever).
    // On failure, returns false and sets `out_token` to empty.
    // The initial value of `out_token` is ignored.
    // Only accepts tokens that can be after `operator`.
    // This has special handling for `(  )` and `[  ]` (with any number of spaces), returning the condensed `()` and `[]` for them.
    [[nodiscard]] constexpr bool ConsumeOperatorToken(std::string_view &input, std::string_view &out_token, ConsumeOperatorTokenFlags flags = {})
    {
        using namespace std::string_view_literals;

        for (std::string_view token : {
            // The OVERLOADABLE operators, a subset of https://eel.is/c++draft/lex.operators#nt:operator-or-punctuator
            // Single-character operators are handled below.

            // Some operators are prefixes of others, so the order matters!
            // Those must be first:
            "<<="sv, ">>="sv, "<=>"sv, "->*"sv,
            // Now the rest:
            "->"sv ,
            "+="sv, "-="sv, "*="sv , "/="sv , "%="sv , "^="sv, "&="sv, "|="sv,
            "=="sv, "!="sv, "<="sv , ">="sv , "&&"sv, "||"sv,
            "<<"sv, ">>"sv, "++"sv , "--"sv,
        })
        {
            if (ConsumePunctuation(input, token))
            {
                out_token = token;
                return true;
            }
        }

        if (!bool(flags & ConsumeOperatorTokenFlags::reject_single_character_operators))
        {
            for (std::string_view token : {
                "~"sv, "!"sv, "+"sv, "-"sv, "*"sv, "/"sv, "%"sv, "^"sv, "&"sv, "|"sv, "="sv, "<"sv, ">"sv, ","sv
            })
            {
                if (ConsumePunctuation(input, token))
                {
                    out_token = token;
                    return true;
                }
            }
        }

        { // Try to consume `()` and `[]`. Those can have whitespace in them...
            std::string_view input_copy = input;
            if (ConsumePunctuation(input_copy, "("))
            {
                TrimLeadingWhitespace(input_copy);
                if (ConsumePunctuation(input_copy, ")"))
                {
                    out_token = "()";
                    input = input_copy;
                    return true;
                }
            }

            if (ConsumePunctuation(input_copy, "["))
            {
                TrimLeadingWhitespace(input_copy);
                if (ConsumePunctuation(input_copy, "]"))
                {
                    out_token = "[]";
                    input = input_copy;
                    return true;
                }
            }
        }

        out_token = {};
        return false;
    }

    // Maybe inserts a whitespace into `input` at `.end() - last_token_len` if that is needed to split up tokens to avoid maximum munch.
    // Returns true if the space was inserted, false if it wasn't needed.
    constexpr bool BreakMaximumMunch(std::string &input, std::size_t last_token_len)
    {
        if (input.empty() || input.size() <= last_token_len)
            return false; // Either lhs or rhs is empty.

        // X + Y
        if (last_token_len == 1 && input.size() >= 2)
        {
            std::string_view tmp = std::string_view(input).substr(input.size() - 2);
            std::string_view token;
            if (ConsumeOperatorToken(tmp, token, ConsumeOperatorTokenFlags::reject_single_character_operators) && tmp.empty())
            {
                // Here we need a special case to only break `>>` if we have `operator> >`, because otherwise it appears to be unnecessary.
                bool ok = true;
                if (token == ">>")
                {
                    std::string_view input_copy = input;
                    input_copy.remove_suffix(2);
                    TrimTrailingWhitespace(input_copy);
                    if (!EndsWithWord(input_copy, "operator"))
                        ok = false;
                }

                if (ok)
                {
                    input.insert(input.end() - std::ptrdiff_t(last_token_len), ' ');
                    return true;
                }
            }
        }
        // XY + Z
        if (last_token_len == 1 && input.size() >= 3)
        {
            std::string_view tmp = std::string_view(input).substr(input.size() - 3);
            std::string_view token;
            if (ConsumeOperatorToken(tmp, token, ConsumeOperatorTokenFlags::reject_single_character_operators) && tmp.empty())
            {
                input.insert(input.end() - std::ptrdiff_t(last_token_len), ' ');
                return true;
            }
        }
        // X + YZ
        if (last_token_len == 2 && input.size() >= 3)
        {
            std::string_view tmp = std::string_view(input).substr(input.size() - 3);
            std::string_view token;
            if (ConsumeOperatorToken(tmp, token, ConsumeOperatorTokenFlags::reject_single_character_operators) && tmp.empty())
            {
                input.insert(input.end() - std::ptrdiff_t(last_token_len), ' ');
                return true;
            }
        }

        return false;
    }

    // Tries to convert the token to a sane looking identifier.
    // Has special handling for `[]` and `{}` to help with `operator[]`, `operator()`.
    // If the token is unknown, either returns empty (if `assert_and_fallback_if_unknown == false`),
    //   or returns `"unknown"` and raises an assertion in debug builds (if `assert_and_fallback_if_unknown == true`).
    [[nodiscard]] constexpr std::string_view TokenToIdentifier(std::string_view token, bool assert_and_fallback_if_unknown)
    {
        // Those are the overloaded operators.
        // Some spellings are inspired by the corresponding alternative operator spellings.
        // We intentionally don't process alternative operator spellings in the input. The rest of the library doesn't support them right now,
        //   and if we begin supporting them, it's unclear if this function would need to be changed at all (maybe we'll translate them during parsing?).
        if (token == "[]") return "index"; // Not `at` to avoid confusion with the popular methods with the same name, when used in function names.
        if (token == "()") return "call";
        if (token == "->") return "arrow";
        if (token == "->*") return "arrow_star";
        if (token == "~") return "compl";
        if (token == "!") return "not";
        if (token == "+") return "add"; // Not `plus` to look better in function names.
        if (token == "-") return "sub"; // Not `minus` to look better in function names.
        if (token == "*") return "mul";
        if (token == "/") return "div";
        if (token == "%") return "mod";
        if (token == "^") return "xor";
        if (token == "&") return "bitand";
        if (token == "|") return "bitor";
        if (token == "=") return "assign";
        if (token == "+=") return "add_assign";
        if (token == "-=") return "sub_assign";
        if (token == "*=") return "mul_assign";
        if (token == "/=") return "div_assign";
        if (token == "%=") return "mod_assign";
        if (token == "^=") return "xor_assign";
        if (token == "&=") return "bitand_assign";
        if (token == "|=") return "bitor_assign";
        if (token == "==") return "equal";
        if (token == "!=") return "not_equal";
        if (token == "<") return "less";
        if (token == ">") return "greater";
        if (token == "<=") return "less_equal";
        if (token == ">=") return "greater_equal";
        if (token == "<=>") return "compare_three_way";
        if (token == "&&") return "and";
        if (token == "||") return "or";
        if (token == "<<") return "lshift";
        if (token == ">>") return "rshift";
        if (token == "<<=") return "lshift_assign";
        if (token == ">>=") return "rshift_assign";
        if (token == "++") return "incr";
        if (token == "--") return "decr";
        if (token == ",") return "comma";
        // Some other tokens:
        if (token == "{") return "open_brace";
        if (token == "}") return "close_brace";
        if (token == "[") return "open_bracket";
        if (token == "]") return "close_bracket";
        if (token == "(") return "open_paren";
        if (token == ")") return "close_paren";
        if (token == ";") return "semicolon";
        if (token == ":") return "colon";
        if (token == "...") return "ellipsis";
        if (token == "?") return "question_mark";
        if (token == "::") return "scope";
        if (token == ".") return "dot";
        if (token == ".*") return "dot_star";

        if (assert_and_fallback_if_unknown)
        {
            assert(false && "Unknown token, don't know how to convert it to an identifier.");
            return "unknown";
        }

        return "";
    }

    // Removes non-identifier characters from `input`, replacing sequences of them with a single `_`.
    [[nodiscard]] CPPDECL_CONSTEXPR std::string KeepOnlyIdentifierChars(std::string_view input)
    {
        std::string ret;
        ret.reserve(input.size());
        bool prev_char_is_special = true; // True to not add leading underscores on unknown characters.
        std::size_t last_good_size = 0;
        for (char ch : input)
        {
            if (cppdecl::IsIdentifierChar(ch))
            {
                ret += ch;
                prev_char_is_special = false;
                last_good_size = ret.size();
            }
            else
            {
                if (!prev_char_is_special)
                {
                    ret += '_';
                    prev_char_is_special = true;
                }
            }
        }
        ret.resize(last_good_size); // Trim trailing special characters.
        return ret;
    }
}
