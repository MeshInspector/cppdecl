#pragma once

#include "cppdecl/declarations/data.h"
#include "cppdecl/declarations/parse.h"
#include "cppdecl/misc/string_helpers.h"

#include <stdexcept>
#include <string_view>
#include <string>
#include <unordered_map>

// Some simple parsing functions that throw on failure, and throw if some part of the string was left unparsed.
// Those also don't support all of the entities, only the most popular ones.
//
// If you need more control than this, use `parse.h` directly. Same if you don't want exceptions.

namespace cppdecl
{
    [[nodiscard]] CPPDECL_CONSTEXPR Type ParseType_Simple(std::string_view input, ParseTypeFlags flags = {})
    {
        const std::string_view input_before_parse = input;
        ParseTypeResult ret = ParseType(input, flags);

        // Handle parse error.
        if (auto error = std::get_if<ParseError>(&ret))
            throw std::runtime_error("cppdecl: Parse error in type `" + std::string(input_before_parse) + "` at position " + NumberToString(input.data() - input_before_parse.data()) + ": " + error->message);

        // Handle unparsed junk at the end.
        assert(input.empty() || !IsWhitespace(input.back())); // `ParseType()` is expected to leave no whitespace.
        TrimLeadingWhitespace(input); // But trim it just in case.
        if (!input.empty())
            throw std::runtime_error("cppdecl: Unparsed junk in type `" + std::string(input_before_parse) + "` starting from position " + NumberToString(input.data() - input_before_parse.data()) + ": `" + std::string(input) + "`.");

        return std::get<Type>(ret);
    }

    // Note that the default value of `flags`, `ParseDeclFlags::accept_everything`, will happily accept unnamed declarations (i.e. just types).
    // If that's not what's desired, pass either `accept_all_named` or `accept_unqualified_named`.
    // Note, the default flags must be synced with `DeclParser` below.
    [[nodiscard]] CPPDECL_CONSTEXPR MaybeAmbiguousDecl ParseDecl_Simple(std::string_view input, ParseDeclFlags flags = ParseDeclFlags::accept_everything)
    {
        const std::string_view input_before_parse = input;
        ParseDeclResult ret = ParseDecl(input, flags);

        // Handle parse error.
        if (auto error = std::get_if<ParseError>(&ret))
            throw std::runtime_error("cppdecl: Parse error in declaration `" + std::string(input_before_parse) + "` at position " + NumberToString(input.data() - input_before_parse.data()) + ": " + error->message);

        // Handle unparsed junk at the end.
        assert(input.empty() || !IsWhitespace(input.back())); // `ParseDecl()` is expected to leave no whitespace.
        TrimLeadingWhitespace(input); // But trim it just in case.
        if (!input.empty())
            throw std::runtime_error("cppdecl: Unparsed junk in declaration `" + std::string(input_before_parse) + "` starting from position " + NumberToString(input.data() - input_before_parse.data()) + ": `" + std::string(input) + "`.");

        return std::get<MaybeAmbiguousDecl>(ret);
    }

    [[nodiscard]] CPPDECL_CONSTEXPR QualifiedName ParseQualifiedName_Simple(std::string_view input, ParseQualifiedNameFlags flags = {})
    {
        const std::string_view input_before_parse = input;
        ParseQualifiedNameResult ret = ParseQualifiedName(input, flags);

        // Handle parse error.
        if (auto error = std::get_if<ParseError>(&ret))
            throw std::runtime_error("cppdecl: Parse error in qualified name `" + std::string(input_before_parse) + "` at position " + NumberToString(input.data() - input_before_parse.data()) + ": " + error->message);

        // Handle unparsed junk at the end.
        // `ParseQualifiedName` is not guaranteed to not leave whitespace.
        if (!input.empty())
            throw std::runtime_error("cppdecl: Unparsed junk in qualified name `" + std::string(input_before_parse) + "` starting from position " + NumberToString(input.data() - input_before_parse.data()) + ": `" + std::string(input) + "`.");

        // Complain if this is a member pointer. Here we only want qualified names.
        if (std::holds_alternative<MemberPointer>(ret))
            throw std::runtime_error("cppdecl: Expected `" + std::string(input_before_parse) + "` to be a qualified name, but it includes a `::*`, which makes it a member pointer.");

        return std::get<QualifiedName>(ret);
    }


    // This is a CRTP base.
    template <typename Derived, typename T>
    class BasicParser
    {
        std::unordered_map<std::string, T> cache;

      public:
        [[nodiscard]] const T &operator()(const std::string &str)
        {
            auto [iter, is_new] = cache.try_emplace(str); // Sadly this doesn't accept `std::string_view` out of the box.
            if (is_new)
            {
                struct Guard
                {
                    BasicParser *self;
                    decltype(iter) iter;

                    ~Guard()
                    {
                        if (self)
                            self->cache.erase(iter);
                    }
                };

                Guard guard{this, iter};

                iter->second = static_cast<Derived &>(*this).Parse(iter->first); // Using `iter->first` because `str` is moved-from at this point.

                guard.self = nullptr; // Disarm the guard.
            }

            return iter->second;
        }
    };

    // This calls `ParseType_Simple()` and memoizes the results.
    class TypeParser : public BasicParser<TypeParser, Type>
    {
        ParseTypeFlags flags;

        friend BasicParser<TypeParser, Type>;
        Type Parse(std::string_view str)
        {
            return ParseType_Simple(str, flags);
        }

      public:
        TypeParser(ParseTypeFlags flags = {}) : flags(flags) {}
    };

    // This calls `ParseDecl_Simple()` and memoizes the results.
    class DeclParser : public BasicParser<DeclParser, MaybeAmbiguousDecl>
    {
        ParseDeclFlags flags;

        friend BasicParser<DeclParser, MaybeAmbiguousDecl>;
        MaybeAmbiguousDecl Parse(std::string_view str)
        {
            return ParseDecl_Simple(str, flags);
        }

      public:
        // Note, the default flags must be synced with `ParseDecl_Simple()` above.
        DeclParser(ParseDeclFlags flags = ParseDeclFlags::accept_everything) : flags(flags) {}
    };

    // This calls `ParseQualifiedName_Simple()` and memoizes the results.
    class QualifiedNameParser : public BasicParser<QualifiedNameParser, QualifiedName>
    {
        ParseQualifiedNameFlags flags;

        friend BasicParser<QualifiedNameParser, QualifiedName>;
        QualifiedName Parse(std::string_view str)
        {
            return ParseQualifiedName_Simple(str, flags);
        }

      public:
        QualifiedNameParser(ParseQualifiedNameFlags flags = {}) : flags(flags) {}
    };
}
