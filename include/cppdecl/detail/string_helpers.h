#pragma once

#include <string_view>
#include <type_traits>

namespace cppdecl
{
    // --- Character classification:

    [[nodiscard]] constexpr bool IsWhitespace(char ch)
    {
        return ch == ' ' || ch == '\t';
    }
    // Remove any prefix whitespace from `str`.
    constexpr void TrimLeadingWhitespace(std::string_view &str)
    {
        while (!str.empty() && IsWhitespace(str.front()))
            str.remove_prefix(1);
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

    // Is `name` a type or a keyword related to types?
    // We use this to detect clearly invalid variable names that were parsed from types.
    [[nodiscard]] constexpr bool IsTypeRelatedKeyword(std::string_view name)
    {
        return
            name == "char" ||
            name == "short" ||
            name == "int" ||
            name == "long" ||
            name == "float" ||
            name == "double" ||
            name == "signed" ||
            name == "unsigned" ||
            name == "const" ||
            name == "volatile" ||
            name == "__restrict" ||
            name == "__restrict__";
        // Not adding the C/nonstandard spelling `restrict` here, since this list is just for better error messages, and the lack of it isn't going
        //   to break its parsing or anything.
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

    // Returns true if `input` starts with `word` followed by end of string or whitespace or
    //   a punctuation character (which is anything other `IsIdentifierChar()`).
    [[nodiscard]] constexpr bool StartsWithWord(std::string_view input, std::string_view word)
    {
        return (input.size() <= word.size() || !IsIdentifierChar(input[word.size()])) && input.starts_with(word);
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
}
