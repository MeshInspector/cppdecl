#include "cppdecl/declarations/parse.h"
#include "cppdecl/declarations/simplify.h"
#include "cppdecl/declarations/simplify_modules/phmap.h"
#include "cppdecl/declarations/to_string.h"
#include "cppdecl/type_name.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>


void Fail(std::string_view message)
{
    throw std::runtime_error("Test failed: " + std::string(message));
}

void CheckActualEqualsExpected(std::string_view message, std::string_view a, std::string_view b)
{
    if (a != b)
    {
        std::cout << "Equality test failed: " << message << "\nActual:   `" << a << "`\nExpected: `" << b << "`\n";
        throw std::runtime_error("Test failed!");
    }
}

template <typename T = cppdecl::DefaultSimplifyTraits>
CPPDECL_CONSTEXPR std::string ParseDeclToString(std::string_view view, cppdecl::ParseDeclFlags mode, std::size_t expected_junk_suffix_size, cppdecl::ToStringFlags strmode = cppdecl::ToStringFlags::debug, cppdecl::SimplifyFlags simplify_flags = {}, T &&simplify_traits = {})
{
    const auto orig_view = view;
    auto ret = cppdecl::ParseDecl(view, mode);
    if (auto error = std::get_if<cppdecl::ParseError>(&ret))
    {
        std::cout << "Parse error at " << (view.data() - orig_view.data()) << ": " << error->message << '\n';
        std::cout << orig_view << '\n';
        std::cout << std::string(std::size_t(view.data() - orig_view.data()), ' ') << "^\n";
        Fail("Parse error.");
        return "";
    }

    if (expected_junk_suffix_size == 0)
    {
        // This branch just provides a better error message.
        if (!view.empty())
        {
            std::cout << "Unparsed junk after input: `" << view << "`\n";
            Fail("Unparsed junk after input.");
        }
    }
    else
    {
        if (view.size() != expected_junk_suffix_size)
        {
            std::cout << "Expected " << expected_junk_suffix_size << " junk characters at the end, but got " << view.size() << ".\n";
            Fail("Wrong amount of unparsed junk after input.");
        }
    }

    auto &decl = std::get<cppdecl::MaybeAmbiguousDecl>(ret);
    cppdecl::Simplify(simplify_flags, decl, simplify_traits);

    return cppdecl::ToString(decl, strmode);
}

void CheckParseSuccess(std::string_view view, cppdecl::ParseDeclFlags mode, std::string_view result, cppdecl::ToStringFlags strmode = cppdecl::ToStringFlags::debug)
{
    CheckActualEqualsExpected("Wrong result of parsing.", ParseDeclToString(view, mode, 0, strmode), result);
}
void CheckParseSuccessWithJunk(std::string_view view, cppdecl::ParseDeclFlags mode, std::size_t expected_junk_suffix_size, std::string_view result, cppdecl::ToStringFlags strmode = cppdecl::ToStringFlags::debug)
{
    CheckActualEqualsExpected("Wrong result of parsing.", ParseDeclToString(view, mode, expected_junk_suffix_size, strmode), result);
}

void CheckParseFail(std::string_view view, cppdecl::ParseDeclFlags mode, std::size_t pos, std::string_view error_message)
{
    const auto orig_view = view;
    auto ret = cppdecl::ParseDecl(view, mode);
    if (auto error = std::get_if<cppdecl::ParseError>(&ret))
    {
        CheckActualEqualsExpected("Wrong error message from parsing.", error->message, error_message);
        CheckActualEqualsExpected("Wrong error offset", std::to_string(view.data() - orig_view.data()), std::to_string(pos));
        return;
    }

    Fail("Expected this parse to fail, but it parsed successfully to: " + cppdecl::ToString(std::get<cppdecl::MaybeAmbiguousDecl>(ret), cppdecl::ToStringFlags::debug));
}

template <typename T = cppdecl::DefaultSimplifyTraits>
void CheckRoundtrip(std::string_view view, cppdecl::ParseDeclFlags flags, std::string_view result, cppdecl::ToCodeFlags style_flags = {}, cppdecl::SimplifyFlags simplify_flags = {}, T &&simplify_traits = {})
{
    const auto orig_view = view;
    auto ret = cppdecl::ParseDecl(view, flags);
    if (auto error = std::get_if<cppdecl::ParseError>(&ret))
    {
        std::cout << "Parse error at " << (view.data() - orig_view.data()) << ": " << error->message << '\n';
        std::cout << orig_view << '\n';
        std::cout << std::string(std::size_t(view.data() - orig_view.data()), ' ') << "^\n";
        Fail("Parse error.");
        return;
    }
    if (!view.empty())
    {
        std::cout << "Unparsed junk after input: `" << view << "`\n";
        Fail("Unparsed junk after input.");
    }

    auto &decl = std::get<cppdecl::MaybeAmbiguousDecl>(ret);
    cppdecl::Simplify(simplify_flags, decl, simplify_traits);

    CheckActualEqualsExpected("Wrong result of a roundtrip.", cppdecl::ToCode(decl, style_flags), result);
}

template <typename T = cppdecl::DefaultSimplifyTraits>
void CheckTypeRoundtrip(std::string_view view, std::string_view result, cppdecl::ToCodeFlags style_flags = {}, cppdecl::SimplifyFlags simplify_flags = {}, std::size_t skip_modifiers = 0, cppdecl::CvQualifiers ignore_top_level_cv_quals = {}, T &&simplify_traits = {})
{
    const auto orig_view = view;
    auto ret = cppdecl::ParseType(view);
    if (auto error = std::get_if<cppdecl::ParseError>(&ret))
    {
        std::cout << "Parse error at " << (view.data() - orig_view.data()) << ": " << error->message << '\n';
        std::cout << orig_view << '\n';
        std::cout << std::string(std::size_t(view.data() - orig_view.data()), ' ') << "^\n";
        Fail("Parse error.");
        return;
    }
    if (!view.empty())
    {
        std::cout << "Unparsed junk after input: `" << view << "`\n";
        Fail("Unparsed junk after input.");
    }

    auto &type = std::get<cppdecl::Type>(ret);
    cppdecl::Simplify(simplify_flags, type, simplify_traits);

    CheckActualEqualsExpected("Wrong result of a roundtrip.", cppdecl::ToCode(type, style_flags, skip_modifiers, ignore_top_level_cv_quals), result);
}

struct IntegerValue {};

void CheckNumericLiteralLow(std::string_view input, std::string_view result, std::variant<cppdecl::ToCodeFlags, cppdecl::ToStringFlags, IntegerValue> flags, std::size_t trailing_junk = 0)
{
    const auto orig_input = input;
    auto ret = cppdecl::ParseNumericLiteral(input);
    if (auto error = std::get_if<cppdecl::ParseError>(&ret))
    {
        std::cout << "Parse error at " << (input.data() - orig_input.data()) << ": " << error->message << '\n';
        std::cout << orig_input << '\n';
        std::cout << std::string(std::size_t(input.data() - orig_input.data()), ' ') << "^\n";
        Fail("Parse error.");
        return;
    }

    if (trailing_junk == 0)
    {
        if (!input.empty())
        {
            std::cout << "Unparsed junk after input: `" << input << "`\n";
            Fail("Unparsed junk after input.");
        }
    }
    else
    {
        CheckActualEqualsExpected("Amount of trailing junk.", std::to_string(input.size()), std::to_string(trailing_junk));
    }

    auto &lit = std::get<std::optional<cppdecl::NumericLiteral>>(ret);
    if (!lit)
    {
        Fail("Parsing numeric literal unexpectedly returned null.");
    }

    std::string str = std::visit(cppdecl::Overload{
        [&](cppdecl::ToCodeFlags flags) {return cppdecl::ToCode(*lit, flags);},
        [&](cppdecl::ToStringFlags flags) {return cppdecl::ToString(*lit, flags);},
        [&](IntegerValue) -> std::string
        {
            if (auto opt = lit->ToInteger())
                return std::to_string(*opt);
            else
                return "";
        },
    }, flags);

    CheckActualEqualsExpected("Wrong result of a roundtrip.", str, result);
}

void CheckNumericLiteral(std::string_view input, std::string_view as_code, std::string_view integer, std::string_view identifier, std::string_view debug, std::string_view pretty, std::size_t trailing_junk = 0)
{
    CheckNumericLiteralLow(input, as_code, cppdecl::ToCodeFlags{}, trailing_junk);
    CheckNumericLiteralLow(input, integer, IntegerValue{}, trailing_junk);
    CheckNumericLiteralLow(input, identifier, cppdecl::ToStringFlags::identifier, trailing_junk);
    CheckNumericLiteralLow(input, debug, cppdecl::ToStringFlags::debug, trailing_junk);
    CheckNumericLiteralLow(input, pretty, cppdecl::ToStringFlags{}, trailing_junk);
}

void CheckNumericLiteralFail(std::string_view input, std::size_t pos, std::string_view error_message)
{
    const auto orig_input = input;
    auto ret = cppdecl::ParseNumericLiteral(input);
    if (auto opt = std::get_if<std::optional<cppdecl::NumericLiteral>>(&ret); opt && !*opt)
        ret = cppdecl::ParseError{.message = "<no literal to parse>"};

    if (auto error = std::get_if<cppdecl::ParseError>(&ret))
    {
        CheckActualEqualsExpected("Wrong error message from parsing.", error->message, error_message);
        CheckActualEqualsExpected("Wrong error offset", std::to_string(input.data() - orig_input.data()), std::to_string(pos));
        return;
    }

    Fail("Expected this parse to fail, but it parsed successfully to: " + cppdecl::ToString(std::get<std::optional<cppdecl::NumericLiteral>>(ret).value(), cppdecl::ToStringFlags::debug));
}

int main()
{
    static constexpr auto m_type = cppdecl::ParseDeclFlags::accept_unnamed;
    static constexpr auto m_any = cppdecl::ParseDeclFlags::accept_everything;
    static constexpr auto m_named = cppdecl::ParseDeclFlags::accept_all_named;

    // Basic named variables.
    CheckParseSuccess("int foo",                               m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="foo"}]}"})");
    CheckParseSuccess("  int  foo  ",                          m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="foo"}]}"})");
    CheckParseSuccess("int*foo",                               m_any, R"({type="pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="foo"}]}"})");
    CheckParseSuccess("  int  *  foo  ",                       m_any, R"({type="pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="foo"}]}"})");
    CheckParseSuccessWithJunk("int*foo",                       m_type, 3, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccessWithJunk("  int  *  foo  ",               m_type, 5, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");

    // Basic templates and pointers.
    CheckParseSuccess("std::basic_string<char>",               m_type, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="std"},{name="basic_string",targs=[type:{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="char"}]}}]}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  std  ::  basic_string  <  char  >  ", m_type, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="std"},{name="basic_string",targs=[type:{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="char"}]}}]}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int*",                                  m_type, R"({type="pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  *  ",                            m_type, R"({type="pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int*const",                             m_type, R"({type="const pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  *  const  ",                     m_type, R"({type="const pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int*const volatile",                    m_type, R"({type="const volatile pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  *  const  volatile  ",           m_type, R"({type="const volatile pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int*__restrict",                        m_type, R"({type="restrict pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  *  __restrict  ",                m_type, R"({type="restrict pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int&",                                  m_type, R"({type="lvalue reference to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  &  ",                            m_type, R"({type="lvalue reference to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int&__restrict",                        m_type, R"({type="restrict lvalue reference to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  &  __restrict  ",                m_type, R"({type="restrict lvalue reference to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseFail("int&const",                                m_type, 4, "References can't be const-qualified.");
    CheckParseFail("  int  &  const  ",                        m_type, 10, "References can't be const-qualified.");

    // Ban [member-]pointers to references and arrays of references.
    CheckParseSuccess("int*&",                                 m_type, R"({type="lvalue reference to pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  *  &  ",                         m_type, R"({type="lvalue reference to pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseFail("int&*",                                    m_type, 3, "Pointers to references are not allowed.");
    CheckParseFail("  int  &  *  ",                            m_type, 7, "Pointers to references are not allowed.");
    CheckParseFail("int&T::*",                                 m_type, 3, "Member pointers to references are not allowed.");
    CheckParseFail("  int  &  T  ::  *  ",                     m_type, 7, "Member pointers to references are not allowed.");
    CheckParseFail("int&[]",                                   m_type, 3, "Arrays of references are not allowed.");
    CheckParseFail("  int  &  [  ]  ",                         m_type, 7, "Arrays of references are not allowed.");
    // Ban arrays of functions.
    CheckParseFail("int[]()",                                  m_type, 5, "Arrays of functions are not allowed.");
    CheckParseFail("  int  [  ]  (  )  ",                      m_type, 13, "Arrays of functions are not allowed.");
    CheckParseFail("int(*)[]()",                               m_type, 8, "Arrays of functions are not allowed.");
    // Ban functions returning arrays and other functions.
    CheckParseFail("int()[]",                                  m_type, 5, "Function return type can't be an array.");
    CheckParseFail("  int  (  )  [  ]  ",                      m_type, 13, "Function return type can't be an array.");
    CheckParseFail("int(*)()[]",                               m_type, 8, "Function return type can't be an array.");
    CheckParseFail("int()()",                                  m_type, 5, "Function return type can't be a function.");
    CheckParseFail("  int  (  )  (  )  ",                      m_type, 13, "Function return type can't be a function.");
    CheckParseFail("int(*)()()",                               m_type, 8, "Function return type can't be a function.");
    // Now with trailing return types:
    CheckParseFail("auto() -> int[]",                          m_type, 10, "Function return type can't be an array.");
    CheckParseFail("auto() -> int()",                          m_type, 10, "Function return type can't be a function.");
    CheckParseFail("auto(*)() -> int[]",                       m_type, 13, "Function return type can't be an array.");
    CheckParseFail("auto(*)() -> int()",                       m_type, 13, "Function return type can't be a function.");

    // Parentheses and pointers.
    CheckParseSuccess("int(*foo)",                             m_any, R"({type="pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="foo"}]}"})");
    CheckParseSuccess("  int  (  *  foo  )  ",                 m_any, R"({type="pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="foo"}]}"})");
    CheckParseSuccess("int(*)",                                m_any, R"({type="pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  (  *  )  ",                      m_any, R"({type="pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int(*)",                                m_type, R"({type="pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");

    // Arrays, arrays of pointers, pointers to arrays.
    CheckParseSuccess("int[]",                                 m_type, R"({type="array of unknown bound of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  [  ]  ",                         m_type, R"({type="array of unknown bound of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int[42]",                               m_type, R"({type="array of size [int{base=10,value=`42`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  [  42  ]  ",                     m_type, R"({type="array of size [int{base=10,value=`42`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int foo[42]",                           m_any, R"({type="array of size [int{base=10,value=`42`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="foo"}]}"})");
    CheckParseSuccess("  int  foo  [  42  ]  ",                m_any, R"({type="array of size [int{base=10,value=`42`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="foo"}]}"})");
    CheckParseSuccess("int (*)[42]",                           m_any, R"({type="pointer to array of size [int{base=10,value=`42`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  (  *  )  [  42  ]  ",            m_any, R"({type="pointer to array of size [int{base=10,value=`42`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int (*foo)[42]",                        m_any, R"({type="pointer to array of size [int{base=10,value=`42`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="foo"}]}"})");
    CheckParseSuccess("  int  (  *  foo  )  [  42  ]  ",       m_any, R"({type="pointer to array of size [int{base=10,value=`42`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="foo"}]}"})");

    // Multidimensional arrays.
    CheckParseSuccess("int foo[1][2]",                         m_any, R"({type="array of size [int{base=10,value=`1`,suffix=none}] of array of size [int{base=10,value=`2`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="foo"}]}"})");
    CheckParseSuccess("  int  foo  [  1  ]  [  2  ]  ",        m_any, R"({type="array of size [int{base=10,value=`1`,suffix=none}] of array of size [int{base=10,value=`2`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="foo"}]}"})");

    // Combining weird qualifiers.
    CheckParseSuccess("long long",                             m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="long long"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseFail("long long long",                           m_type, 10, "Can't add this keyword to the preceding type.");
    CheckParseFail("long long long",                           m_any, 10, "Can't add this keyword to the preceding type.");
    CheckParseFail("const const",                              m_any, 6, "Repeated `const`.");
    CheckParseFail("volatile volatile",                        m_any, 9, "Repeated `volatile`.");
    // A different error message here, for no particular reason.
    CheckParseFail("void *const const",                        m_any, 12, "Duplicate cv-qualifier.");
    CheckParseFail("void *volatile volatile",                  m_any, 15, "Duplicate cv-qualifier.");
    CheckParseFail("void *__restrict __restrict__",            m_any, 17, "Duplicate cv-qualifier.");
    // This parses as a type name right now, which is weird but whatever.
    CheckParseSuccess("__restrict",                            m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="__restrict"}]}}",name="{global_scope=false,parts=[]}"})");
    // Signedness.
    CheckParseSuccess("unsigned long",                         m_any, R"({type="{attrs=[],flags=[unsigned],quals=[],name={global_scope=false,parts=[{name="long"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("signed long",                           m_any, R"({type="{attrs=[],flags=[explicitly_signed],quals=[],name={global_scope=false,parts=[{name="long"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("long unsigned",                         m_any, R"({type="{attrs=[],flags=[unsigned],quals=[],name={global_scope=false,parts=[{name="long"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("long signed",                           m_any, R"({type="{attrs=[],flags=[explicitly_signed],quals=[],name={global_scope=false,parts=[{name="long"}]}}",name="{global_scope=false,parts=[]}"})");
    // Reject combined signed/unsigned qualifiers.
    CheckParseFail("unsigned signed long",                     m_any, 9, "Both `unsigned` and `signed` on the same type.");
    CheckParseFail("signed unsigned long",                     m_any, 7, "Both `signed` and `unsigned` on the same type.");
    // Reject duplicate qualifiers.
    CheckParseFail("unsigned unsigned long",                   m_any, 9, "Repeated `unsigned`.");
    CheckParseFail("signed signed long",                       m_any, 7, "Repeated `signed`.");
    // Implicit `int` when signedness is specified.
    CheckParseSuccess("unsigned",                              m_any, R"({type="{attrs=[],flags=[unsigned,implied_int],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("signed",                                m_any, R"({type="{attrs=[],flags=[explicitly_signed,implied_int],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    // This isn't legal C++, but our parser used to assert on this, which isn't good.
    // Now it treats the template argument as a pseudo-expr, which is intended. It's not our job to diagnose those.
    CheckParseSuccess("A<unsigned B>",                         m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="A",targs=[expr[{attrs=[],flags=[unsigned,implied_int],quals=[],name={global_scope=false,parts=[{name="int"}]}},{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="B"}]}}]]}]}}",name="{global_scope=false,parts=[]}"})");
    // This is valid though, and used to cause a similar issue.
    CheckParseSuccess("A<unsigned{}>",                         m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="A",targs=[expr[{attrs=[],flags=[unsigned,implied_int],quals=[],name={global_scope=false,parts=[{name="int"}]}},list{}]]}]}}",name="{global_scope=false,parts=[]}"})");

    // More moist signedness tests.
    CheckParseSuccess("signed A",                              m_any, "`A` of type explicitly signed implied `int`", cppdecl::ToStringFlags{});
    CheckParseSuccessWithJunk("signed A",                      m_type, 1, "unnamed of type explicitly signed implied `int`", cppdecl::ToStringFlags{});
    CheckParseSuccess("unsigned A",                            m_any, "`A` of type unsigned implied `int`", cppdecl::ToStringFlags{});
    CheckParseSuccessWithJunk("unsigned A",                    m_type, 1, "unnamed of type unsigned implied `int`", cppdecl::ToStringFlags{});
    CheckParseFail("A signed",                                 m_any, 2, "Can only apply `signed` directly to built-in arithmetic types.");
    CheckParseFail("A unsigned",                               m_any, 2, "Can only apply `unsigned` directly to built-in arithmetic types.");

    CheckParseFail("void signed",                              m_any, 5, "Can only apply `signed` directly to built-in arithmetic types.");
    CheckParseFail("void unsigned",                            m_any, 5, "Can only apply `unsigned` directly to built-in arithmetic types.");
    CheckParseFail("float signed",                             m_any, 6, "Can only apply `signed` directly to built-in arithmetic types.");
    CheckParseFail("float unsigned",                           m_any, 6, "Can only apply `unsigned` directly to built-in arithmetic types.");
    // A different error but whatever.
    CheckParseFail("signed void",                              m_any, 7, "Can't add this keyword to the preceding type.");
    CheckParseFail("unsigned void",                            m_any, 9, "Can't add this keyword to the preceding type.");
    CheckParseFail("signed float",                             m_any, 7, "Can't add this keyword to the preceding type.");
    CheckParseFail("unsigned float",                           m_any, 9, "Can't add this keyword to the preceding type.");

    // Some non-standard types.
    CheckParseSuccess("__int128",                              m_type, "unnamed of type `__int128`", {});
    CheckParseSuccess("signed __int128",                       m_type, "unnamed of type explicitly signed `__int128`", {});
    CheckParseSuccess("unsigned __int128",                     m_type, "unnamed of type unsigned `__int128`", {});

    // Empty decl-specifier-seq is an error.
    CheckParseFail("",                                         m_any, 0, "Expected a type or a name.");
    CheckParseFail("  ",                                       m_any, 2, "Expected a type or a name.");
    CheckParseFail("",                                         m_any | cppdecl::ParseDeclFlags::force_non_empty_return_type, 0, "Expected a type.");
    CheckParseFail("  ",                                       m_any | cppdecl::ParseDeclFlags::force_non_empty_return_type, 2, "Expected a type.");
    // Explicit `int` flag.
    CheckParseSuccess("long int",                              m_any, R"({type="{attrs=[],flags=[redundant_int],quals=[],name={global_scope=false,parts=[{name="long"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int long",                              m_any, R"({type="{attrs=[],flags=[redundant_int],quals=[],name={global_scope=false,parts=[{name="long"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("long long int",                         m_any, R"({type="{attrs=[],flags=[redundant_int],quals=[],name={global_scope=false,parts=[{name="long long"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("long int long",                         m_any, R"({type="{attrs=[],flags=[redundant_int],quals=[],name={global_scope=false,parts=[{name="long long"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int long long",                         m_any, R"({type="{attrs=[],flags=[redundant_int],quals=[],name={global_scope=false,parts=[{name="long long"}]}}",name="{global_scope=false,parts=[]}"})");
    // long double
    CheckParseSuccess("long double",                           m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="long double"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("double long",                           m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="long double"}]}}",name="{global_scope=false,parts=[]}"})");

    // Function types.
    CheckParseSuccess("int()",                                 m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  (  )  ",                         m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int()const",                            m_any, R"({type="a function (const-qualified) taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  (  )  const  ",                  m_any, R"({type="a function (const-qualified) taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int()const",                            m_any, R"({type="a function (const-qualified) taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int()volatile const __restrict noexcept", m_any, R"({type="a function (const-volatile-restrict-qualified, noexcept) taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  (  )  volatile  const  __restrict  noexcept  ", m_any, R"({type="a function (const-volatile-restrict-qualified, noexcept) taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int()volatile const __restrict&noexcept", m_any, R"({type="a function (const-volatile-restrict-qualified, lvalue-ref-qualified, noexcept) taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  (  )  volatile  const  __restrict  &  noexcept  ", m_any, R"({type="a function (const-volatile-restrict-qualified, lvalue-ref-qualified, noexcept) taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int()volatile const __restrict&&noexcept", m_any, R"({type="a function (const-volatile-restrict-qualified, rvalue-ref-qualified, noexcept) taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  (  )  volatile  const  __restrict  &&  noexcept  ", m_any, R"({type="a function (const-volatile-restrict-qualified, rvalue-ref-qualified, noexcept) taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccessWithJunk("  int  (  )  volatile  const  __restrict  &  &  noexcept  ", m_any, 13, R"({type="a function (const-volatile-restrict-qualified, lvalue-ref-qualified) taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int&()",                                m_any, R"({type="a function taking no parameters, returning lvalue reference to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  &  (  )  ",                      m_any, R"({type="a function taking no parameters, returning lvalue reference to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");

    CheckParseSuccess("int(foo x, float(*y)[4])",              m_any, R"({type="a function taking 2 parameters: [1. {type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="foo"}]}}",name="{global_scope=false,parts=[{name="x"}]}"}, 2. {type="pointer to array of size [int{base=10,value=`4`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="float"}]}}",name="{global_scope=false,parts=[{name="y"}]}"}], returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  (  foo  x  ,  float  (  *  y  )  [  4  ]  )  ", m_any, R"({type="a function taking 2 parameters: [1. {type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="foo"}]}}",name="{global_scope=false,parts=[{name="x"}]}"}, 2. {type="pointer to array of size [int{base=10,value=`4`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="float"}]}}",name="{global_scope=false,parts=[{name="y"}]}"}], returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");

    CheckParseFail("foo(x,)",                                  m_any, 6, "Expected a type.");

    // We used to choke here, assuming `(void` was a start of a C-style empty parameter list.
    CheckParseSuccess("int(void *)",  m_any, "unnamed function taking 1 parameter: [unnamed pointer to `void`], returning `int`", cppdecl::ToStringFlags{});


    // Trailing return type.

    CheckParseSuccess("auto(*&)()->int(*)[42]",                m_any, R"({type="lvalue reference to pointer to a function taking no parameters, returning (via trailing return type) pointer to array of size [int{base=10,value=`42`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  auto  (  *  &  )  (  )  ->  int  (  *  )  [  42  ]  ", m_any, R"({type="lvalue reference to pointer to a function taking no parameters, returning (via trailing return type) pointer to array of size [int{base=10,value=`42`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseFail("int(*&)()->int(*)[42]",                    m_any, 9, "A trailing return type is specified, but the previousy specified return type wasn't `auto`.");
    CheckParseFail("  int  (  *  &  )  (  )  ->  int  (  *  )  [  42  ]  ", m_any, 25, "A trailing return type is specified, but the previousy specified return type wasn't `auto`.");
    CheckParseFail("auto*(*&)()->int(*)[42]",                  m_any, 11, "A trailing return type is specified, but the previousy specified return type wasn't `auto`.");
    CheckParseFail("  auto  *  (  *  &  )  (  )  ->  int  (  *  )  [  42  ]  ", m_any, 29, "A trailing return type is specified, but the previousy specified return type wasn't `auto`.");


    // Parameter names:
    // Unnamed.
    CheckParseSuccess("int(int)",                              m_any, R"({type="a function taking 1 parameter: [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"}], returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    // Named.
    CheckParseSuccess("int(int x)",                            m_any, R"({type="a function taking 1 parameter: [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"}], returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    // Named and qualified = error. This isn't exactly the best error message, but it's good enough.
    // We COULD make it better by having a hard error on the qualified name, but should we?
    CheckParseFail("int(int x::y)",                            m_any, 8, "Expected `)` or `,` or `...` in function parameter list.");


    // Member pointers.
    CheckParseFail("A::B::*x",                                 m_any, 0, R"(Expected the pointee type before the member pointer.)");
    CheckParseFail("  A  ::  B  ::  *  x  ",                   m_any, 2, R"(Expected the pointee type before the member pointer.)");
    CheckParseSuccess("int A::B::*x",                          m_any, R"({type="pointer-to-member of class {global_scope=false,parts=[{name="A"},{name="B"}]} of type {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"})");
    CheckParseSuccess("  int  A  ::  B  ::  *  x  ",           m_any, R"({type="pointer-to-member of class {global_scope=false,parts=[{name="A"},{name="B"}]} of type {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"})");
    CheckParseSuccess("int A::B::*C::D::*x",                   m_any, R"({type="pointer-to-member of class {global_scope=false,parts=[{name="C"},{name="D"}]} of type pointer-to-member of class {global_scope=false,parts=[{name="A"},{name="B"}]} of type {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"})");
    CheckParseSuccess("  int  A  ::  B  ::  *  C  ::  D  ::  *  x  ", m_any, R"({type="pointer-to-member of class {global_scope=false,parts=[{name="C"},{name="D"}]} of type pointer-to-member of class {global_scope=false,parts=[{name="A"},{name="B"}]} of type {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"})");
    CheckParseSuccess("int A::B::*C::D::*E::F::*x",            m_any, R"({type="pointer-to-member of class {global_scope=false,parts=[{name="E"},{name="F"}]} of type pointer-to-member of class {global_scope=false,parts=[{name="C"},{name="D"}]} of type pointer-to-member of class {global_scope=false,parts=[{name="A"},{name="B"}]} of type {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"})");
    CheckParseSuccess("  int  A  ::  B  ::  *  C  ::  D  ::  *  E  ::  F  ::  *  x  ", m_any, R"({type="pointer-to-member of class {global_scope=false,parts=[{name="E"},{name="F"}]} of type pointer-to-member of class {global_scope=false,parts=[{name="C"},{name="D"}]} of type pointer-to-member of class {global_scope=false,parts=[{name="A"},{name="B"}]} of type {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"})");
    CheckParseSuccess("int A::B::**x",                         m_any, R"({type="pointer to pointer-to-member of class {global_scope=false,parts=[{name="A"},{name="B"}]} of type {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"})");
    CheckParseSuccess("  int  A  ::  B  ::  *  *  x  ",        m_any, R"({type="pointer to pointer-to-member of class {global_scope=false,parts=[{name="A"},{name="B"}]} of type {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"})");
    // Some ambiguities related to `::` forcing the global scope.
    CheckParseSuccess("int::A::*", m_any, "unnamed pointer-to-member of class ::`A` of type `int`", cppdecl::ToStringFlags{});
    CheckParseFail("A::B::*", m_any, 0, "Expected the pointee type before the member pointer.");

    // Resolving ambiguities based on the parser flags.
    CheckParseSuccess("int(x)",                                m_any, R"(either [{type="a function taking 1 parameter: [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="x"}]}}",name="{global_scope=false,parts=[]}"}], returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"}] or [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"}])");
    CheckParseSuccess("int(x)",                                cppdecl::ParseDeclFlags::accept_unnamed, R"({type="a function taking 1 parameter: [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="x"}]}}",name="{global_scope=false,parts=[]}"}], returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int(x)",                                cppdecl::ParseDeclFlags::accept_unqualified_named, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"})");
    CheckParseSuccess("int(x)",                                cppdecl::ParseDeclFlags::accept_all_named        , R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"})");
    // Triple ambiguity (two alternatives on the top level, then another two in one of the function parameters).
    CheckParseSuccess("x(y(z))",                               m_any | cppdecl::ParseDeclFlags::force_non_empty_return_type, R"(either [{type="a function taking 1 parameter: [either [{type="a function taking 1 parameter: [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="z"}]}}",name="{global_scope=false,parts=[]}"}], returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="y"}]}}",name="{global_scope=false,parts=[]}"}] or [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="y"}]}}",name="{global_scope=false,parts=[{name="z"}]}"}]], returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="x"}]}}",name="{global_scope=false,parts=[]}"}] or [{type="a function taking 1 parameter: [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="z"}]}}",name="{global_scope=false,parts=[]}"}], returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="x"}]}}",name="{global_scope=false,parts=[{name="y"}]}"}])");

    // C-style variadics.
    CheckParseSuccess("int(...)",                              m_any, R"({type="a function taking no parameters and a C-style variadic parameter, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  (  ...  )  ",                    m_any, R"({type="a function taking no parameters and a C-style variadic parameter, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int(x...)",                             m_any, R"({type="a function taking 1 parameter: [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="x"}]}}",name="{global_scope=false,parts=[]}"}] and a C-style variadic parameter (with a missing comma before it), returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  (  x  ...  )  ",                 m_any, R"({type="a function taking 1 parameter: [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="x"}]}}",name="{global_scope=false,parts=[]}"}] and a C-style variadic parameter (with a missing comma before it), returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int(x,...)",                            m_any, R"({type="a function taking 1 parameter: [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="x"}]}}",name="{global_scope=false,parts=[]}"}] and a C-style variadic parameter, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("  int  (  x  ,  ...  )  ",              m_any, R"({type="a function taking 1 parameter: [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="x"}]}}",name="{global_scope=false,parts=[]}"}] and a C-style variadic parameter, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    // Bad variadics.
    CheckParseFail("int(...x)",                                m_any, 7, "Expected `)` after a C-style variadic parameter.");
    CheckParseFail("int(x...x)",                               m_any, 8, "Expected `)` after a C-style variadic parameter.");
    CheckParseFail("int(x,...x)",                              m_any, 9, "Expected `)` after a C-style variadic parameter.");


    // Some templates.
    CheckParseSuccess("std::map<std::vector<int>, float>",     m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="std"},{name="map",targs=[type:{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="std"},{name="vector",targs=[type:{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}]}]}},type:{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="float"}]}}]}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("std::map<std::vector<int>, float>::iterator", m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="std"},{name="map",targs=[type:{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="std"},{name="vector",targs=[type:{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}]}]}},type:{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="float"}]}}]},{name="iterator"}]}}",name="{global_scope=false,parts=[]}"})");

    // Expressions.
    CheckParseSuccess("int[4+4]",                              m_any, R"({type="array of size [int{base=10,value=`4`,suffix=none},punct`+`,int{base=10,value=`4`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("int[a::b---c]",                         m_any, R"({type="array of size [{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="a"},{name="b"}]}},punct`--`,punct`-`,{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="c"}]}}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("T<foo(1,2,3), bar[a], std::vector{1,2,3,}>", m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="T",targs=[expr[{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="foo"}]}},list([int{base=10,value=`1`,suffix=none}],[int{base=10,value=`2`,suffix=none}],[int{base=10,value=`3`,suffix=none}])],type:array of size [{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="a"}]}}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}},expr[{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="std"},{name="vector"}]}},list{[int{base=10,value=`1`,suffix=none}],[int{base=10,value=`2`,suffix=none}],[int{base=10,value=`3`,suffix=none}]}(has trailing comma)]]}]}}",name="{global_scope=false,parts=[]}"})");
    // In array size `>` is a punctuation, it doesn't end any template argument list.
    CheckParseSuccess("bar[2 > 1]",                            m_any, R"({type="array of size [int{base=10,value=`2`,suffix=none},punct`>`,int{base=10,value=`1`,suffix=none}] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}}",name="{global_scope=false,parts=[]}"})");


    // String and character literals.

    CheckParseSuccess("bar[\"foo\"]",                          m_any, R"({type="array of size [str`foo`] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}}",name="{global_scope=false,parts=[]}"})");
    // We don't validate character literal length.
    CheckParseSuccess("bar['foo']",                            m_any, R"({type="array of size [char`foo`] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("bar[\"foo\"lit_123]",                   m_any, R"({type="array of size [str`foo`(suffix`lit_123`)] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("bar['foo'lit_123]",                     m_any, R"({type="array of size [char`foo`(suffix`lit_123`)] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}}",name="{global_scope=false,parts=[]}"})");

    CheckParseSuccess("bar[\"foo'bar\"]",                      m_any, R"({type="array of size [str`foo'bar`] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("bar['foo\"bar']",                       m_any, R"({type="array of size [char`foo"bar`] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("bar[\"foo\\\"bar\"]",                   m_any, R"({type="array of size [str`foo\"bar`] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("bar['foo\\'bar']",                      m_any, R"({type="array of size [char`foo\'bar`] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}}",name="{global_scope=false,parts=[]}"})");

    CheckParseFail("bar[ \"foo]",                              m_any, 5, "Unterminated string literal.");
    CheckParseFail("bar[ \'foo]",                              m_any, 5, "Unterminated character literal.");
    CheckParseFail("bar[ \"foo\\\"]",                          m_any, 5, "Unterminated string literal.");
    CheckParseFail("bar[ \'foo\\']",                           m_any, 5, "Unterminated character literal.");
    CheckParseFail("bar[ \"\\",                                m_any, 6, "Unterminated escape sequence.");
    CheckParseFail("bar[ '\\",                                 m_any, 6, "Unterminated escape sequence.");

    // Raw string literals.
    CheckParseFail("bar[ R\"",                                 m_any, 5, "Unterminated opening delimiter of a raw string literal.");
    CheckParseFail("bar[ R\"1234567890123456",                 m_any, 5, "Unterminated opening delimiter of a raw string literal.");
    CheckParseFail("bar[ R\"12345678901234567",                m_any, 24, "Raw string literal delimiter is too long.");
    CheckParseFail("bar[ R\" ",                                m_any, 7, "Invalid character in a raw string literal delimiter.");
    CheckParseFail("bar[ R\"\t",                               m_any, 7, "Invalid character in a raw string literal delimiter.");
    CheckParseFail("bar[ R\"\v",                               m_any, 7, "Invalid character in a raw string literal delimiter.");
    CheckParseFail("bar[ R\"\\",                               m_any, 7, "Invalid character in a raw string literal delimiter.");
    CheckParseSuccess("bar[ R\"(foo)\"]",                      m_any, R"({type="array of size [rawstr`foo`] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseFail("bar[ R\"(foo)x\"]",                        m_any, 5, "Unterminated raw string literal.");
    CheckParseSuccess("bar[ R\"(foo\\)\"]",                    m_any, R"({type="array of size [rawstr`foo\`] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("bar[ R\"(foo)x\"bar)\"]",               m_any, R"({type="array of size [rawstr`foo)x"bar`] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("bar[ R\"abc(foo)cab\"bar)abc\"]",       m_any, R"({type="array of size [rawstr`foo)cab"bar`(delim`abc`)] of {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="bar"}]}}",name="{global_scope=false,parts=[]}"})");



    // Type prefixes:

    CheckParseSuccess("class A",    m_any, R"({type="{attrs=[],flags=[],prefix=class,quals=[],name={global_scope=false,parts=[{name="A"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("struct A",   m_any, R"({type="{attrs=[],flags=[],prefix=struct,quals=[],name={global_scope=false,parts=[{name="A"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("union A",    m_any, R"({type="{attrs=[],flags=[],prefix=union,quals=[],name={global_scope=false,parts=[{name="A"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("enum A",     m_any, R"({type="{attrs=[],flags=[],prefix=enum,quals=[],name={global_scope=false,parts=[{name="A"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseSuccess("typename A", m_any, R"({type="{attrs=[],flags=[],prefix=typename,quals=[],name={global_scope=false,parts=[{name="A"}]}}",name="{global_scope=false,parts=[]}"})");
    CheckParseFail("A class", m_any, 2, "The type prefix can't appear after the type.");
    CheckParseFail("class int", m_any, 6, "Elaborated type specifier applied to a built-in type.");
    // Illegal, but MSVC sometimes allows this, so we do too:
    CheckParseSuccess("typename int", m_any, R"({type="{attrs=[],flags=[],prefix=typename,quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[]}"})");

    CheckParseSuccess("class A",    m_any, "unnamed of type `A`, explicitly a class", cppdecl::ToStringFlags{});
    CheckParseSuccess("struct A",   m_any, "unnamed of type `A`, explicitly a struct", cppdecl::ToStringFlags{});
    CheckParseSuccess("union A",    m_any, "unnamed of type `A`, explicitly a union", cppdecl::ToStringFlags{});
    CheckParseSuccess("enum A",     m_any, "unnamed of type `A`, explicitly a enum", cppdecl::ToStringFlags{});
    CheckParseSuccess("typename A", m_any, "unnamed of type `A`, explicitly a typename", cppdecl::ToStringFlags{});

    // Don't emit them in identifier strings for now. Who needs them anyway?
    CheckParseSuccess("class A",    m_any, "A", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("typename A", m_any, "A", cppdecl::ToStringFlags::identifier);


    // Unusual qualified name components:

    // UDL.
    CheckParseSuccess("void A::operator\"\"_blah()",           m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="void"}]}}",name="{global_scope=false,parts=[{name="A"},{udl=`_blah`}]}"})");
    CheckParseSuccess("  void  A  ::  operator  \"\"_blah  (  )  ", m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="void"}]}}",name="{global_scope=false,parts=[{name="A"},{udl=`_blah`}]}"})");
    CheckParseSuccess("  void  A  ::  operator  \"\" _blah  (  )  ", m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="void"}]}}",name="{global_scope=false,parts=[{name="A"},{udl=`_blah`(with space before suffix)}]}"})");
    CheckParseSuccess("  void  A  ::  operator  \"\"  _blah  (  )  ", m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="void"}]}}",name="{global_scope=false,parts=[{name="A"},{udl=`_blah`(with space before suffix)}]}"})");

    // Overloaded operator.
    CheckParseSuccess("void A::operator++()",                  m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="void"}]}}",name="{global_scope=false,parts=[{name="A"},{op=`++`}]}"})");
    CheckParseSuccess("  void  A  ::  operator  ++  (  )  ",   m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="void"}]}}",name="{global_scope=false,parts=[{name="A"},{op=`++`}]}"})");
    // Overloaded operator `()`. This is special, it can have whitespace between the `()`.
    CheckParseSuccess("void A::operator()()",                  m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="void"}]}}",name="{global_scope=false,parts=[{name="A"},{op=`()`}]}"})");
    CheckParseSuccess("  void  A  ::  operator  (  )  (  )  ", m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="void"}]}}",name="{global_scope=false,parts=[{name="A"},{op=`()`}]}"})");
    // Overloaded operator `()`. This is special, it can have whitespace between the `[]`.
    CheckParseSuccess("void A::operator[]()",                  m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="void"}]}}",name="{global_scope=false,parts=[{name="A"},{op=`[]`}]}"})");
    CheckParseSuccess("  void  A  ::  operator  [  ]  (  )  ", m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="void"}]}}",name="{global_scope=false,parts=[{name="A"},{op=`[]`}]}"})");
    // Test all overloaded operators, to make their the tokens parse correclty.
    CheckParseSuccess("int operator+(A)",   m_any, "overloaded operator `+`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator-(A)",   m_any, "overloaded operator `-`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator*(A)",   m_any, "overloaded operator `*`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator/(A)",   m_any, "overloaded operator `/`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator%(A)",   m_any, "overloaded operator `%`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator^(A)",   m_any, "overloaded operator `^`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator&(A)",   m_any, "overloaded operator `&`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator|(A)",   m_any, "overloaded operator `|`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator~(A)",   m_any, "overloaded operator `~`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator!(A)",   m_any, "overloaded operator `!`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator=(A)",   m_any, "overloaded operator `=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator<(A)",   m_any, "overloaded operator `<`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator>(A)",   m_any, "overloaded operator `>`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator+=(A)",  m_any, "overloaded operator `+=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator-=(A)",  m_any, "overloaded operator `-=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator*=(A)",  m_any, "overloaded operator `*=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator/=(A)",  m_any, "overloaded operator `/=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator%=(A)",  m_any, "overloaded operator `%=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator^=(A)",  m_any, "overloaded operator `^=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator&=(A)",  m_any, "overloaded operator `&=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator|=(A)",  m_any, "overloaded operator `|=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator<<(A)",  m_any, "overloaded operator `<<`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator>>(A)",  m_any, "overloaded operator `>>`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator<<=(A)", m_any, "overloaded operator `<<=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator>>=(A)", m_any, "overloaded operator `>>=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator==(A)",  m_any, "overloaded operator `==`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator!=(A)",  m_any, "overloaded operator `!=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator<=(A)",  m_any, "overloaded operator `<=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator>=(A)",  m_any, "overloaded operator `>=`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator<=>(A)", m_any, "overloaded operator `<=>`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator&&(A)",  m_any, "overloaded operator `&&`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator||(A)",  m_any, "overloaded operator `||`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator++(A)",  m_any, "overloaded operator `++`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator--(A)",  m_any, "overloaded operator `--`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator,(A)",   m_any, "overloaded operator `,`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator->*(A)", m_any, "overloaded operator `->*`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator->()",   m_any, "overloaded operator `->`, a function taking no parameters, returning `int`", {});
    CheckParseSuccess("int operator()(A)",  m_any, "overloaded operator `()`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator[](A)",  m_any, "overloaded operator `[]`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator  (  )  (A)",  m_any, "overloaded operator `()`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});
    CheckParseSuccess("int operator  [  ]  (A)",  m_any, "overloaded operator `[]`, a function taking 1 parameter: [unnamed of type `A`], returning `int`", {});


    // `operator` with nothing after it.
    CheckParseFail("void A::operator",                         m_any, 16, "Expected a type.");
    CheckParseFail("  void  A  ::  operator  ",                m_any, 25, "Expected a type.");
    // `operator` with junk after it. Weird error but ok.
    CheckParseFail("void A::operator@",                        m_any, 16, "Expected a type.");
    CheckParseFail("  void  A  ::  operator  @",               m_any, 25, "Expected a type.");

    // Conversion operator.
    CheckParseSuccess("A::operator int",                       m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="A"},{conv=`{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}`}]}"})");
    CheckParseSuccess("A::operator int*&",                     m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="A"},{conv=`lvalue reference to pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}`}]}"})");
    CheckParseSuccess("A::operator int A::*B::*",              m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="A"},{conv=`pointer-to-member of class {global_scope=false,parts=[{name="B"}]} of type pointer-to-member of class {global_scope=false,parts=[{name="A"}]} of type {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}`}]}"})");
    CheckParseSuccess("A::operator int()",                     m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="A"},{conv=`{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}`}]}"})");
    CheckParseSuccess("A::operator int*&()",                   m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="A"},{conv=`lvalue reference to pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}`}]}"})");
    CheckParseSuccess("A::operator int A::*B::*()",            m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="A"},{conv=`pointer-to-member of class {global_scope=false,parts=[{name="B"}]} of type pointer-to-member of class {global_scope=false,parts=[{name="A"}]} of type {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}`}]}"})");
    CheckParseSuccess("A::operator int  ()",                   m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="A"},{conv=`{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}`}]}"})");
    CheckParseSuccess("A::operator int*&  ()",                 m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="A"},{conv=`lvalue reference to pointer to {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}`}]}"})");
    CheckParseSuccess("A::operator int A::*B::*  ()",          m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="A"},{conv=`pointer-to-member of class {global_scope=false,parts=[{name="B"}]} of type pointer-to-member of class {global_scope=false,parts=[{name="A"}]} of type {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}`}]}"})");
    // Conversion operators don't allow any `(` in the type.
    CheckParseFail("A::operator int(*)",                       m_any, 16, "Expected a type.");
    // Conversion operators don't allow any right-side declarators.
    // Because of that, this parses to a variable named `A::operator int` of type `int [42]`. Weird, but not our job to police?
    CheckParseFail("A::operator int[42]",                      m_any, 15, "Assumed this was a function declaration with an empty return type, but found an array.");

    // Destructors.
    CheckParseSuccess("~A()",                                  m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{dtor=`{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="A"}]}}`}]}"})");
    CheckParseSuccess("~A()",                                  m_any, "destructor for type [`A`], a destructor taking no parameters", cppdecl::ToStringFlags{});
    CheckParseSuccess("~A",                                    m_any, "destructor for type [`A`]", cppdecl::ToStringFlags{});
    CheckParseSuccess("A::~B()",                               m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="A"},{dtor=`{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="B"}]}}`}]}"})");
    // Notably the destructor types can't contain `::` (after `~`), so here the destructor component is only `~A` itself.
    CheckParseSuccess("int ~A::B()",                           m_any, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{dtor=`{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="A"}]}}`},{name="B"}]}"})");
    // And this explodes because there's no return type.
    CheckParseFail("  ~A::B()",                                m_any, 2, "Expected a type."); // Not the best error, but whatever?
    CheckParseFail("  ~struct A()",                            m_any, 3, "Destructor type can't include type prefixes.");
    CheckParseSuccess("A::B()",                                m_any, "unnamed function taking no parameters, returning `A`::`B`", cppdecl::ToStringFlags{});
    CheckParseSuccess("A::B()",                                m_type, "unnamed function taking no parameters, returning `A`::`B`", cppdecl::ToStringFlags{});
    // The error points at the `)`, since we expect a variable name here.
    CheckParseFail("  A  ::  B  (  )  ",                       m_named, 15, "Expected a name.");
    // In template parameters, `~` without `::` before it is parsed as punctuation, for simplicity.
    // `A::~A` is not a valid type, so it's an expression, but in it `A::~A` is a `SimpleType`, despite not being a valid type.
    // `A::~A()` is also an expression, where `A::~A` is a `SimpleType` and `()` is an expression list.
    // `A::~A::B` is a valid type, so the entire template argument is a type.
    CheckParseSuccess("foo<~A, A::~A, A::~A(), A::~A::B>",     m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="foo",targs=[expr[punct`~`,{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="A"}]}}],expr[{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="A"},{dtor=`{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="A"}]}}`}]}}],expr[{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="A"},{dtor=`{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="A"}]}}`}]}},list()],type:{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="A"},{dtor=`{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="A"}]}}`},{name="B"}]}}]}]}}",name="{global_scope=false,parts=[]}"})");


    // Empty return types.

    // Triple ambiguity: function type `x(y)`, variable `x y`, constructor without return type `x(y)`.
    CheckParseSuccess("x(y)",                                  m_any, R"(either [{type="a function taking 1 parameter: [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="y"}]}}",name="{global_scope=false,parts=[]}"}], returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="x"}]}}",name="{global_scope=false,parts=[]}"}] or [either [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="x"}]}}",name="{global_scope=false,parts=[{name="y"}]}"}] or [{type="a function taking 1 parameter: [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="y"}]}}",name="{global_scope=false,parts=[]}"}], returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="x"}]}"}]])");
    // If we require the name, this reduces to two: `variable `x y` or constructor without return type `x(y)`.
    CheckParseSuccess("x(y)",                                  m_any & ~cppdecl::ParseDeclFlags::accept_unnamed, R"(either [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="x"}]}}",name="{global_scope=false,parts=[{name="y"}]}"}] or [{type="a function taking 1 parameter: [{type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="y"}]}}",name="{global_scope=false,parts=[]}"}], returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="x"}]}"}])");


    // Checking various heuristics.
    CheckParseFail("A operator B",                             m_any, 2, "A conversion operator must have no return type.");
    CheckParseFail("A operator B()",                           m_any, 2, "A conversion operator must have no return type.");
    // This is treated as an `operator~` with a missing return type and a missing parameter list. The error points to the missing return type.
    CheckParseFail("  operator ~B()",                          m_any, 2, "Expected a type.");
    // This is a conversion operator with a non-type after `operator`.
    CheckParseFail("operator A::~B()",                         m_any, 9, "Expected a type.");
    // We consider `A::~B::C` to be a valid type. It's not standard C++, but I expect compilers to report function-local types in this fashion.
    CheckParseSuccess("operator A::~B::C()",                   m_any, "conversion operator to [`A`::destructor for type [`B`]::`C`], a function taking no parameters, returning nothing", cppdecl::ToStringFlags{});
    // Right now we allow constructor name `A::A` as a type here, despite compilers not allowing it.
    // Should we not? And if we do that, are there any contexts where we should still allow it to be a type?
    CheckParseSuccess("operator A::A()",                       m_any, "conversion operator to [`A`::`A`], a function taking no parameters, returning nothing", cppdecl::ToStringFlags{});

    // More conversion operators with non-types after `operator`.
    CheckParseFail("operator A::operator+()",                  m_any, 9, "Expected a type.");
    CheckParseFail("operator A::operator\"\"_blah()",          m_any, 9, "Expected a type.");

    CheckParseSuccess("A::~A",                                 m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="A"},{dtor=`{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="A"}]}}`}]}"})");
    CheckParseSuccess("A::~A",                                 m_any, "`A`::destructor for type [`A`]", cppdecl::ToStringFlags{});
    CheckParseSuccess("A::~A()",                               m_any, "`A`::destructor for type [`A`], a destructor taking no parameters", cppdecl::ToStringFlags{});
    CheckParseFail("X A::~A",                                  m_any, 2, "A destructor must have no return type.");
    CheckParseFail("X A::~A()",                                m_any, 2, "A destructor must have no return type.");

    CheckParseFail("  A  ",                                    m_named, 5, "Expected a name.");
    CheckParseFail("  int  (  )  ",                            m_named, 10, "Expected a name.");
    CheckParseSuccess("  A()  ",                               m_named, R"({type="a function taking no parameters, returning {attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{name="A"}]}"})");
    CheckParseSuccess("  A()  ",                               m_named, "`A`, a constructor taking no parameters", cppdecl::ToStringFlags{});
    CheckParseSuccess("  A()  ",                               m_any, "ambiguous, either [unnamed function taking no parameters, returning `A`] or [`A`, a constructor taking no parameters]", cppdecl::ToStringFlags{});
    CheckParseSuccess("  A::A  ",                              m_named, "`A`::`A`, a constructor without a parameter list", cppdecl::ToStringFlags{});
    CheckParseSuccess("  A::A()  ",                            m_named, "`A`::`A`, a constructor taking no parameters", cppdecl::ToStringFlags{});
    CheckParseSuccess("  A::A()  ",                            m_any, "ambiguous, either [unnamed function taking no parameters, returning `A`::`A`] or [`A`::`A`, a constructor taking no parameters]", cppdecl::ToStringFlags{});

    CheckParseFail("int A::A",                                 m_any, 4, "A constructor must have no return type.");
    CheckParseFail("int A::A()",                               m_any, 4, "A constructor must have no return type.");

    CheckParseFail("  long()",                                 m_any | cppdecl::ParseDeclFlags::force_empty_return_type, 2, "Expected a name.");

    CheckParseSuccess("operator int",                          m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[]}}",name="{global_scope=false,parts=[{conv=`{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}`}]}"})");
    CheckParseSuccess("operator int",                          m_any, "conversion operator to [`int`]", cppdecl::ToStringFlags{});

    CheckParseSuccess("A()",                                   m_any, "ambiguous, either [unnamed function taking no parameters, returning `A`] or [`A`, a constructor taking no parameters]", cppdecl::ToStringFlags{});
    CheckParseSuccess("A()",                                   m_any | cppdecl::ParseDeclFlags::force_non_empty_return_type, "unnamed function taking no parameters, returning `A`", cppdecl::ToStringFlags{});
    CheckParseSuccess("A()",                                   m_any | cppdecl::ParseDeclFlags::force_empty_return_type, "`A`, a constructor taking no parameters", cppdecl::ToStringFlags{});
    CheckParseSuccess("int()",                                 m_any, "unnamed function taking no parameters, returning `int`", cppdecl::ToStringFlags{});
    CheckParseSuccess("int()",                                 m_any | cppdecl::ParseDeclFlags::force_non_empty_return_type, "unnamed function taking no parameters, returning `int`", cppdecl::ToStringFlags{});
    // Not the best error, but whatever.
    CheckParseFail("int()",                                    m_any | cppdecl::ParseDeclFlags::force_empty_return_type, 0, "Expected a name.");


    // Some empty return type errors.
    CheckParseFail("  *  x  ",                                 m_any | cppdecl::ParseDeclFlags::force_empty_return_type, 2, "Expected a name.");
    CheckParseFail("  *  x  ",                                 m_any | cppdecl::ParseDeclFlags::force_non_empty_return_type, 2, "Expected a type.");
    CheckParseFail("  *  x  ",                                 m_any, 2, "Expected a type or a name.");

    CheckParseFail("  &  x  ",                                 m_any | cppdecl::ParseDeclFlags::force_empty_return_type, 2, "Expected a name.");
    CheckParseFail("  &  x  ",                                 m_any | cppdecl::ParseDeclFlags::force_non_empty_return_type, 2, "Expected a type.");
    CheckParseFail("  &  x  ",                                 m_any, 2, "Expected a type or a name.");

    CheckParseFail("  A::*  x  ",                              m_any | cppdecl::ParseDeclFlags::force_empty_return_type, 2, "Expected a name, but found a member pointer.");
    CheckParseFail("  A::*  x  ",                              m_any | cppdecl::ParseDeclFlags::force_non_empty_return_type, 2, "Expected the pointee type before the member pointer."); // Hmm, a different message! Whatever.
    CheckParseFail("  A::*  x  ",                              m_any, 2, "Expected the pointee type before the member pointer."); // Same.

    CheckParseFail("  x  [42]  ",                              m_named | cppdecl::ParseDeclFlags::force_empty_return_type, 5, "Assumed this was a function declaration with an empty return type, but found an array.");
    CheckParseFail("  x  [42]  ",                              m_named | cppdecl::ParseDeclFlags::force_non_empty_return_type, 5, "Expected a name.");
    CheckParseFail("  x  [42]  ",                              m_named, 5, "Expected a name.");
    // A bit inconsistent, but whatever.
    CheckParseFail("  operator+  [42]  ",                      m_named | cppdecl::ParseDeclFlags::force_empty_return_type, 2, "Expected a type.");
    CheckParseFail("  operator+  [42]  ",                      m_named | cppdecl::ParseDeclFlags::force_non_empty_return_type, 2, "Expected a type.");
    CheckParseFail("  operator+  [42]  ",                      m_named, 2, "Expected a type.");

    CheckParseFail("  operator int  [42]  ",                   m_named | cppdecl::ParseDeclFlags::force_empty_return_type, 16, "Assumed this was a function declaration with an empty return type, but found an array.");
    CheckParseFail("  operator int  [42]  ",                   m_named | cppdecl::ParseDeclFlags::force_non_empty_return_type, 2, "Expected a type.");
    CheckParseFail("  operator int  [42]  ",                   m_named, 16, "Assumed this was a function declaration with an empty return type, but found an array.");


    // Banning literal keywords from qualified names.
    CheckParseFail("  A::true",                                  m_any, 5, "This keyword can't be a part of a qualified name.");
    CheckParseFail("  true::A",                                  m_any, 2, "Expected a type or a name.");
    CheckParseFail("  A::false",                                 m_any, 5, "This keyword can't be a part of a qualified name.");
    CheckParseFail("  false::A",                                 m_any, 2, "Expected a type or a name.");
    CheckParseFail("  A::nullptr",                               m_any, 5, "This keyword can't be a part of a qualified name.");
    CheckParseFail("  nullptr::A",                               m_any, 2, "Expected a type or a name.");
    // But in template arguments they ARE parsed as `PseudoExpr -> SimpleType`, like all other identifiers.
    // Note that they aren't parsed as type template parameters, which is why we made this special logic in the first place.
    CheckParseSuccess("foo<true, false, nullptr>",               m_any, R"({type="{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="foo",targs=[expr[{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="true"}]}}],expr[{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="false"}]}}],expr[{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="nullptr"}]}}]]}]}}",name="{global_scope=false,parts=[]}"})");


    // Pretty printing.
    CheckParseSuccess("x(y)", m_any, "ambiguous, either [unnamed function taking 1 parameter: [unnamed of type `y`], returning `x`] or [`y` of type `x`] or [`x`, a constructor taking 1 parameter: [unnamed of type `y`]]", cppdecl::ToStringFlags{});


    // Converting to code.
    CheckRoundtrip("int",                                      m_any, "int");
    CheckRoundtrip("int *",                                    m_any, "int *");
    CheckRoundtrip("int **",                                   m_any, "int **");
    CheckRoundtrip("int *const",                               m_any, "int *const");
    CheckRoundtrip("int *const *",                             m_any, "int *const *");
    CheckRoundtrip("int *volatile const",                      m_any, "int *const volatile");
    CheckRoundtrip("int [2]",                                  m_any, "int[2]");
    CheckRoundtrip("int (*(*foo)(const void *))[3]",           m_any, "int (*(*foo)(const void *))[3]");
    CheckRoundtrip("int &__restrict__",                        m_any, "int &__restrict");
    CheckRoundtrip("int (int, int)",                           m_any, "int(int, int)");
    CheckRoundtrip("long long",                                m_any, "long long");
    CheckRoundtrip("long long int",                            m_any, "long long int");
    CheckRoundtrip("unsigned long long",                       m_any, "unsigned long long");
    CheckRoundtrip("unsigned long long int",                   m_any, "unsigned long long int");
    CheckRoundtrip("signed",                                   m_any, "signed");
    CheckRoundtrip("unsigned",                                 m_any, "unsigned");
    CheckRoundtrip("void foo() const volatile __restrict && noexcept", m_any, "void foo() const volatile __restrict && noexcept");
    CheckRoundtrip("auto foo() const volatile __restrict & noexcept -> int", m_any, "auto foo() const volatile __restrict & noexcept -> int");
    CheckRoundtrip("auto() -> auto(*)(int) -> void",           m_any, "auto() -> auto (*)(int) -> void");
    CheckRoundtrip("auto() -> auto(*)(int) -> void",           m_any, "void (*())(int)", cppdecl::ToCodeFlags::force_no_trailing_return_type);
    CheckRoundtrip("int::A::*",                                m_any, "int (::A::*)"); // We could serialize this to `int ::A::*`, but it's easier to always add the `(...)` when the class name starts with `::`.
    CheckRoundtrip("A(::B::*)",                                m_any, "A (::B::*)"); // This one actually requires `(...)` to disambiguate.

    CheckRoundtrip("void foo(int, ...)",                       m_any, "void foo(int, ...)");
    CheckRoundtrip("void foo(int, ...)",                       m_any, "void foo(int,...)", cppdecl::ToCodeFlags::no_space_after_comma);
    CheckRoundtrip("void foo(int...)",                         m_any, "void foo(int...)");
    CheckRoundtrip("void foo(int...)",                         m_any, "void foo(int, ...)", cppdecl::ToCodeFlags::force_comma_before_c_style_variadic);

    CheckRoundtrip("void foo()",                               m_any, "void foo()");
    CheckRoundtrip("void foo(void)",                           m_any, "void foo(void)");
    CheckRoundtrip("void foo()",                               m_any, "void foo(void)", cppdecl::ToCodeFlags::force_c_style_empty_params);
    CheckRoundtrip("void foo(void)",                           m_any, "void foo(void)", cppdecl::ToCodeFlags::force_c_style_empty_params);
    CheckRoundtrip("void foo()",                               m_any, "void foo()", cppdecl::ToCodeFlags::force_cpp_style_empty_params);
    CheckRoundtrip("void foo(void)",                           m_any, "void foo()", cppdecl::ToCodeFlags::force_cpp_style_empty_params);

    CheckRoundtrip("void foo(...)",                            m_any, "void foo(...)");
    CheckRoundtrip("void foo(...)",                            m_any, "void foo(...)", cppdecl::ToCodeFlags::force_c_style_empty_params | cppdecl::ToCodeFlags::force_comma_before_c_style_variadic);

    CheckRoundtrip("std::array<int(*)(int) const, (10 + 20) * 2>", m_any, "std::array<int (*)(int) const, (10+20)*2>");

    CheckRoundtrip("signed int", m_any, "signed int");
    CheckRoundtrip("signed char", m_any, "signed char");
    CheckRoundtrip("signed int", m_any, "int", cppdecl::ToCodeFlags::force_no_redundant_signed);
    CheckRoundtrip("signed char", m_any, "signed char", cppdecl::ToCodeFlags::force_no_redundant_signed);
    CheckRoundtrip("signed int", m_any, "int", {}, cppdecl::SimplifyFlags::bit_common_remove_redundant_signed);
    CheckRoundtrip("signed char", m_any, "signed char", {}, cppdecl::SimplifyFlags::bit_common_remove_redundant_signed);

    // Type prefixes.
    CheckRoundtrip("class A",    m_any, "class A");
    CheckRoundtrip("struct A",   m_any, "struct A");
    CheckRoundtrip("union A",    m_any, "union A");
    CheckRoundtrip("enum A",     m_any, "enum A");
    CheckRoundtrip("typename A", m_any, "typename A");

    CheckRoundtrip("class A",    m_any, "A", cppdecl::ToCodeFlags::force_no_type_prefix);
    CheckRoundtrip("struct A",   m_any, "A", cppdecl::ToCodeFlags::force_no_type_prefix);
    CheckRoundtrip("union A",    m_any, "A", cppdecl::ToCodeFlags::force_no_type_prefix);
    CheckRoundtrip("enum A",     m_any, "A", cppdecl::ToCodeFlags::force_no_type_prefix);
    CheckRoundtrip("typename A", m_any, "A", cppdecl::ToCodeFlags::force_no_type_prefix);

    CheckRoundtrip("class A",    m_any, "A", {}, cppdecl::SimplifyFlags::bit_common_remove_type_prefix);
    CheckRoundtrip("struct A",   m_any, "A", {}, cppdecl::SimplifyFlags::bit_common_remove_type_prefix);
    CheckRoundtrip("union A",    m_any, "A", {}, cppdecl::SimplifyFlags::bit_common_remove_type_prefix);
    CheckRoundtrip("enum A",     m_any, "A", {}, cppdecl::SimplifyFlags::bit_common_remove_type_prefix);
    CheckRoundtrip("typename A", m_any, "A", {}, cppdecl::SimplifyFlags::bit_common_remove_type_prefix);

    // Avoid maximum munch traps.
    CheckRoundtrip("foo<&A::operator> >",                      m_any, "foo<&A::operator> >");
    CheckRoundtrip("std::array<int, 1+ +1>",                   m_any, "std::array<int, 1+ +1>");
    CheckRoundtrip("std::array<int, 1+ -1>",                   m_any, "std::array<int, 1+-1>");
    CheckRoundtrip("std::array<int, 1- +1>",                   m_any, "std::array<int, 1-+1>");
    CheckRoundtrip("std::array<int, 1- -1>",                   m_any, "std::array<int, 1- -1>");
    // Those don't need whitespace:
    CheckRoundtrip("foo<&A::operator+ >",                      m_any, "foo<&A::operator+>");
    CheckRoundtrip("foo<bar<42> >",                            m_any, "foo<bar<42>>");
    CheckRoundtrip("std::array<int, 1* -1>",                   m_any, "std::array<int, 1*-1>");

    // Alternative pointer alignment:
    CheckRoundtrip("int **x",                                  m_any, "int **x");
    CheckRoundtrip("int *&x",                                  m_any, "int *&x");
    CheckRoundtrip("int A::*x",                                m_any, "int A::* x");
    CheckRoundtrip("int (*x)[42]",                             m_any, "int (*x)[42]");
    CheckRoundtrip("int A::*B::*x",                            m_any, "int A::* B::* x");
    CheckRoundtrip("int A::*B::*&x",                           m_any, "int A::* B::* &x");
    CheckRoundtrip("A<int> &",                                 m_any, "A<int> &");
    CheckRoundtrip("auto() -> auto(*)(int) -> void",           m_any, "auto() -> auto (*)(int) -> void");

    CheckRoundtrip("int **x",                                  m_any, "int**x", cppdecl::ToCodeFlags::no_space_before_pointer);
    CheckRoundtrip("int *&x",                                  m_any, "int*&x", cppdecl::ToCodeFlags::no_space_before_pointer);
    CheckRoundtrip("int A::*x",                                m_any, "int A::* x", cppdecl::ToCodeFlags::no_space_before_pointer);
    CheckRoundtrip("int (*x)[42]",                             m_any, "int (*x)[42]", cppdecl::ToCodeFlags::no_space_before_pointer);
    CheckRoundtrip("int A::*B::*x",                            m_any, "int A::* B::* x", cppdecl::ToCodeFlags::no_space_before_pointer);
    CheckRoundtrip("int A::*B::*&x",                           m_any, "int A::* B::* &x", cppdecl::ToCodeFlags::no_space_before_pointer);
    CheckRoundtrip("A<int> &",                                 m_any, "A<int>&", cppdecl::ToCodeFlags::no_space_before_pointer);
    CheckRoundtrip("auto() -> auto(*)(int) -> void",           m_any, "auto() -> auto (*)(int) -> void", cppdecl::ToCodeFlags::no_space_before_pointer);

    CheckRoundtrip("int **x",                                  m_any, "int ** x", cppdecl::ToCodeFlags::add_space_after_pointer);
    CheckRoundtrip("int *&x",                                  m_any, "int *& x", cppdecl::ToCodeFlags::add_space_after_pointer);
    CheckRoundtrip("int A::*x",                                m_any, "int A::* x", cppdecl::ToCodeFlags::add_space_after_pointer);
    CheckRoundtrip("int (*x)[42]",                             m_any, "int (* x)[42]", cppdecl::ToCodeFlags::add_space_after_pointer);
    CheckRoundtrip("int A::*B::*x",                            m_any, "int A::* B::* x", cppdecl::ToCodeFlags::add_space_after_pointer);
    CheckRoundtrip("int A::*B::*&x",                           m_any, "int A::* B::* & x", cppdecl::ToCodeFlags::add_space_after_pointer);
    CheckRoundtrip("A<int> &",                                 m_any, "A<int> &", cppdecl::ToCodeFlags::add_space_after_pointer);
    CheckRoundtrip("auto() -> auto(*)(int) -> void",           m_any, "auto() -> auto (*)(int) -> void", cppdecl::ToCodeFlags::add_space_after_pointer);

    CheckRoundtrip("int **x",                                  m_any, "int** x", cppdecl::ToCodeFlags::left_align_pointer);
    CheckRoundtrip("int *&x",                                  m_any, "int*& x", cppdecl::ToCodeFlags::left_align_pointer);
    CheckRoundtrip("int A::*x",                                m_any, "int A::* x", cppdecl::ToCodeFlags::left_align_pointer);
    CheckRoundtrip("int (*x)[42]",                             m_any, "int (* x)[42]", cppdecl::ToCodeFlags::left_align_pointer);
    CheckRoundtrip("int A::*B::*x",                            m_any, "int A::* B::* x", cppdecl::ToCodeFlags::left_align_pointer);
    CheckRoundtrip("int A::*B::*&x",                           m_any, "int A::* B::* & x", cppdecl::ToCodeFlags::left_align_pointer);
    CheckRoundtrip("A<int> &",                                 m_any, "A<int>&", cppdecl::ToCodeFlags::left_align_pointer);
    CheckRoundtrip("auto() -> auto(*)(int) -> void",           m_any, "auto() -> auto (*)(int) -> void", cppdecl::ToCodeFlags::left_align_pointer);

    // Removing space after commas.
    CheckRoundtrip("int(int, int)",                            m_any, "int(int, int)");
    CheckRoundtrip("int(int, int)",                            m_any, "int(int,int)", cppdecl::ToCodeFlags::no_space_after_comma);

    CheckRoundtrip("A<int, int>",                              m_any, "A<int, int>");
    CheckRoundtrip("A<int, int>",                              m_any, "A<int,int>", cppdecl::ToCodeFlags::no_space_after_comma);



    // Converting things to identifiers.
    CheckParseSuccess("int",                                   m_any, "int", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("long long",                             m_any, "long_long", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("long long int",                         m_any, "long_long", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("unsigned long",                         m_any, "unsigned_long", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("int blah",                              m_any, "int_blah", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("int *&",                                m_any, "int_ptr_ref", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("int *[]",                               m_any, "int_ptr_array", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("int *[42]",                             m_any, "int_ptr_array_42", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("int (*)[42]",                           m_any, "int_array_42_ptr", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("const int *volatile",                   m_any, "int_const_ptr_volatile", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("const int &__restrict",                 m_any, "int_const_ref_restrict", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("const int A::* volatile",               m_any, "int_const_A_memptr_volatile", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("int *()",                               m_any, "int_ptr_func", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("int *(int, float x)",                   m_any, "int_ptr_func_from_int_float_x", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("int *(int, float x) &",                 m_any, "int_ptr_func_from_int_float_x_lvalue", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("int *(int, float x) &&",                m_any, "int_ptr_func_from_int_float_x_rvalue", cppdecl::ToStringFlags::identifier);
    CheckParseSuccess("int *(int, float x) const &",           m_any, "int_ptr_func_from_int_float_x_const_lvalue", cppdecl::ToStringFlags::identifier);



    // MSVC pointer annotations.
    CheckParseSuccess("int *__ptr32 x", m_any, "`x`, a __ptr32 pointer to `int`", {});
    CheckParseSuccess("int *__ptr64 x", m_any, "`x`, a __ptr64 pointer to `int`", {});
    CheckParseFail("__ptr32 int x", m_any, 8, "Can't add this keyword to the preceding type.");
    CheckParseFail("__ptr64 int x", m_any, 8, "Can't add this keyword to the preceding type.");

    // Now the `__unaligned` annotation:
    CheckParseSuccess("__unaligned int *x", m_any, "`x`, a pointer to __unaligned `int`", {});
    CheckParseSuccess("int *__unaligned *x", m_any, "`x`, a pointer to an __unaligned pointer to `int`", {}); // Clang strips `__unaligned` in this context, I'm not sure why. We keep it for simplicity.
    CheckParseSuccess("int *__unaligned x", m_any, "`x`, an __unaligned pointer to `int`", {}); // Clang strips `__unaligned` in this context too. We keep it for simplicity.

    // Printing lambda parameter lists and return types:
    CheckTypeRoundtrip("A(B, C)",          "(B, C) -> A", cppdecl::ToCodeFlags::lambda);
    CheckTypeRoundtrip("auto (B, C) -> A", "(B, C) -> A", cppdecl::ToCodeFlags::lambda);
    CheckTypeRoundtrip("A(B, C)",          "(B, C) -> A", cppdecl::ToCodeFlags::lambda | cppdecl::ToCodeFlags::force_no_trailing_return_type);
    CheckTypeRoundtrip("auto (B, C) -> A", "(B, C) -> A", cppdecl::ToCodeFlags::lambda | cppdecl::ToCodeFlags::force_no_trailing_return_type);


    // --- Simplification:

    // libstdc++-style version namespace:
    CheckRoundtrip("std::__cxx11::basic_string<char>", m_any, "std::__cxx11::basic_string<char>", {});
    CheckRoundtrip("std::__cxx11::basic_string<char>", m_any, "std::basic_string<char>", {}, cppdecl::SimplifyFlags::bit_libstdcxx_remove_cxx11_namespace_in_std);

    // libc++-style version namespace:
    CheckRoundtrip("std::__1::basic_string<char>", m_any, "std::__1::basic_string<char>", {});
    CheckRoundtrip("std::__1::basic_string<char>", m_any, "std::basic_string<char>", {}, cppdecl::SimplifyFlags::bit_libcpp_remove_1_namespace_in_std);

    // MSVC pointer annotations:
    CheckRoundtrip("int * __ptr32", m_any, "int *__ptr32", {});
    CheckRoundtrip("int * __ptr32", m_any, "int *", {}, cppdecl::SimplifyFlags::bit_msvc_remove_ptr32_ptr64);
    CheckRoundtrip("int * __ptr64", m_any, "int *__ptr64", {});
    CheckRoundtrip("int * __ptr64", m_any, "int *", {}, cppdecl::SimplifyFlags::bit_msvc_remove_ptr32_ptr64);

    // Allocator:
    CheckRoundtrip("std::basic_string<char, std::char_traits<char>, std::allocator<char>>",                                           m_any, "std::basic_string<char, std::char_traits<char>, std::allocator<char>>", {});
    CheckRoundtrip("std::basic_string<char, std::char_traits<char>, std::allocator<char>>",                                           m_any, "std::basic_string<char, std::char_traits<char>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);
    CheckRoundtrip("std::vector<int, std::allocator<int>>",                                                                           m_any, "std::vector<int, std::allocator<int>>", {});
    CheckRoundtrip("std::vector<int, std::allocator<int>>",                                                                           m_any, "std::vector<int>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);
    CheckRoundtrip("std::set<int, std::less<int>, std::allocator<int>>",                                                              m_any, "std::set<int, std::less<int>, std::allocator<int>>", {});
    CheckRoundtrip("std::set<int, std::less<int>, std::allocator<int>>",                                                              m_any, "std::set<int, std::less<int>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);
    CheckRoundtrip("std::map<int, float, std::less<int>, std::allocator<std::pair<const int, float>>>",                               m_any, "std::map<int, float, std::less<int>, std::allocator<std::pair<const int, float>>>", {});
    CheckRoundtrip("std::map<int, float, std::less<int>, std::allocator<std::pair<const int, float>>>",                               m_any, "std::map<int, float, std::less<int>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);
    CheckRoundtrip("std::unordered_set<int, std::hash<int>, std::equal_to<int>, std::allocator<int>>",                                m_any, "std::unordered_set<int, std::hash<int>, std::equal_to<int>, std::allocator<int>>", {});
    CheckRoundtrip("std::unordered_set<int, std::hash<int>, std::equal_to<int>, std::allocator<int>>",                                m_any, "std::unordered_set<int, std::hash<int>, std::equal_to<int>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);
    CheckRoundtrip("std::unordered_map<int, float, std::hash<int>, std::equal_to<int>, std::allocator<std::pair<const int, float>>>", m_any, "std::unordered_map<int, float, std::hash<int>, std::equal_to<int>, std::allocator<std::pair<const int, float>>>", {});
    CheckRoundtrip("std::unordered_map<int, float, std::hash<int>, std::equal_to<int>, std::allocator<std::pair<const int, float>>>", m_any, "std::unordered_map<int, float, std::hash<int>, std::equal_to<int>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);
    // This should still work for nested names:
    CheckRoundtrip("std::vector<int, std::allocator<int>>::iterator",                                                                 m_any, "std::vector<int>::iterator", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);
    // Allow constness on the entire vector:
    CheckRoundtrip("const std::vector<int, std::allocator<int>>",                                                                     m_any, "const std::vector<int>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);
    // Allow constness on the element just in case, even though it doesn't work in practice:
    CheckRoundtrip("std::vector<const int, std::allocator<const int>>",                                                               m_any, "std::vector<const int>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);
    // --- Rejections:
    // Refuse to remove allocator when there are other arguments after it:
    CheckRoundtrip("std::vector<int, std::allocator<int>, hmm>",                                                                      m_any, "std::vector<int, std::allocator<int>, hmm>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);
    // Refuse to remove allocator when the type doesn't match:
    CheckRoundtrip("std::vector<int, std::allocator<float>>",                                                                         m_any, "std::vector<int, std::allocator<float>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);
    CheckRoundtrip("std::map<int, float, std::less<int>, std::allocator<std::pair<const char, float>>>",                              m_any, "std::map<int, float, std::less<int>, std::allocator<std::pair<const char, float>>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);
    CheckRoundtrip("std::map<int, float, std::less<int>, std::allocator<std::pair<const int, double>>>",                              m_any, "std::map<int, float, std::less<int>, std::allocator<std::pair<const int, double>>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);
    // Here in particular the `const` is missing from the allocator:
    CheckRoundtrip("std::map<int, float, std::less<int>, std::allocator<std::pair<int, float>>>",                                     m_any, "std::map<int, float, std::less<int>, std::allocator<std::pair<int, float>>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_allocator);

    // `std::char_traits`:
    CheckRoundtrip("std::basic_string<char, std::char_traits<char>>", m_any, "std::basic_string<char, std::char_traits<char>>", {});
    CheckRoundtrip("std::basic_string<char, std::char_traits<char>>", m_any, "std::basic_string<char>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_char_traits);
    // Allow constness:
    CheckRoundtrip("const std::basic_string<char, std::char_traits<char>>", m_any, "const std::basic_string<char>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_char_traits);
    // Allow nested names:
    CheckRoundtrip("std::basic_string<char, std::char_traits<char>>::iterator", m_any, "std::basic_string<char>::iterator", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_char_traits);
    // --- Rejections:
    // Refuse to remove when have other arguments after it:
    CheckRoundtrip("std::basic_string<char, std::char_traits<char>, int>", m_any, "std::basic_string<char, std::char_traits<char>, int>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_char_traits);
    // Refuse to remove when the type doesn't match:
    CheckRoundtrip("std::basic_string<char, std::char_traits<int>>", m_any, "std::basic_string<char, std::char_traits<int>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_char_traits);
    // Refuse to remove when not a string type:
    CheckRoundtrip("std::bleh<char, std::char_traits<char>>", m_any, "std::bleh<char, std::char_traits<char>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_char_traits);

    // Comparators:
    CheckRoundtrip("std::set<int, std::less<int>>", m_any, "std::set<int, std::less<int>>", {});
    CheckRoundtrip("std::set<int, std::less<int>>", m_any, "std::set<int>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_comparator);
    CheckRoundtrip("std::map<int, float, std::less<int>>", m_any, "std::map<int, float, std::less<int>>", {});
    CheckRoundtrip("std::map<int, float, std::less<int>>", m_any, "std::map<int, float>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_comparator);
    CheckRoundtrip("std::unordered_set<int, std::hash<int>, std::equal_to<int>>", m_any, "std::unordered_set<int, std::hash<int>, std::equal_to<int>>", {});
    CheckRoundtrip("std::unordered_set<int, std::hash<int>, std::equal_to<int>>", m_any, "std::unordered_set<int, std::hash<int>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_comparator);
    CheckRoundtrip("std::unordered_map<int, float, std::hash<int>, std::equal_to<int>>", m_any, "std::unordered_map<int, float, std::hash<int>, std::equal_to<int>>", {});
    CheckRoundtrip("std::unordered_map<int, float, std::hash<int>, std::equal_to<int>>", m_any, "std::unordered_map<int, float, std::hash<int>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_comparator);
    // Allow nested names:
    CheckRoundtrip("std::set<int, std::less<int>>::iterator", m_any, "std::set<int>::iterator", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_comparator);
    // Allow constness:
    CheckRoundtrip("const std::set<int, std::less<int>>", m_any, "const std::set<int>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_comparator);
    // --- Rejections:
    CheckRoundtrip("std::set<int, std::less<float>>", m_any, "std::set<int, std::less<float>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_comparator);
    CheckRoundtrip("std::unordered_set<int, std::hash<int>, std::equal_to<float>>", m_any, "std::unordered_set<int, std::hash<int>, std::equal_to<float>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_comparator);

    // Hash:
    CheckRoundtrip("std::unordered_set<int, std::hash<int>>", m_any, "std::unordered_set<int, std::hash<int>>", {});
    CheckRoundtrip("std::unordered_set<int, std::hash<int>>", m_any, "std::unordered_set<int>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_hash_functor);
    CheckRoundtrip("std::unordered_map<int, float, std::hash<int>>", m_any, "std::unordered_map<int, float, std::hash<int>>", {});
    CheckRoundtrip("std::unordered_map<int, float, std::hash<int>>", m_any, "std::unordered_map<int, float>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_hash_functor);
    // Allow nested names:
    CheckRoundtrip("std::unordered_set<int, std::hash<int>>::iterator", m_any, "std::unordered_set<int>::iterator", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_hash_functor);
    // Allow constness:
    CheckRoundtrip("const std::unordered_set<int, std::hash<int>>", m_any, "const std::unordered_set<int>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_hash_functor);
    // --- Rejections:
    CheckRoundtrip("std::unordered_set<int, std::hash<float>>", m_any, "std::unordered_set<int, std::hash<float>>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_hash_functor);

    // Rewriting specializations to typedefs:
    CheckRoundtrip("std::basic_string<char>",     m_any, "std::string", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    CheckRoundtrip("std::basic_string<wchar_t>",  m_any, "std::wstring", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    CheckRoundtrip("std::basic_string<char8_t>",  m_any, "std::u8string", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    CheckRoundtrip("std::basic_string<char16_t>", m_any, "std::u16string", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    CheckRoundtrip("std::basic_string<char32_t>", m_any, "std::u32string", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    CheckRoundtrip("std::basic_string_view<char>",     m_any, "std::string_view", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    CheckRoundtrip("std::basic_string_view<wchar_t>",  m_any, "std::wstring_view", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    CheckRoundtrip("std::basic_string_view<char8_t>",  m_any, "std::u8string_view", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    CheckRoundtrip("std::basic_string_view<char16_t>", m_any, "std::u16string_view", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    CheckRoundtrip("std::basic_string_view<char32_t>", m_any, "std::u32string_view", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    CheckRoundtrip("std::basic_ostream<char>",     m_any, "std::ostream", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    CheckRoundtrip("std::basic_ostream<wchar_t>",  m_any, "std::wostream", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    // Those three don't have typedefs:
    CheckRoundtrip("std::basic_ostream<char8_t>",  m_any, "std::basic_ostream<char8_t>", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    CheckRoundtrip("std::basic_ostream<char16_t>", m_any, "std::basic_ostream<char16_t>", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);
    CheckRoundtrip("std::basic_ostream<char32_t>", m_any, "std::basic_ostream<char32_t>", {}, cppdecl::SimplifyFlags::bit_common_rewrite_template_specializations_as_typedefs);

    // C bools:
    CheckRoundtrip("_Bool", m_any, "_Bool", {});
    CheckRoundtrip("_Bool", m_any, "bool", {}, cppdecl::SimplifyFlags::bit_c_normalize_bool);

    // `std::default_delete`:
    // On libstdc++:
    CheckRoundtrip("std::unique_ptr<int, std::default_delete<int>>", m_any, "std::unique_ptr<int>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_default_delete);
    CheckRoundtrip("std::unique_ptr<int, std::default_delete<int>>", m_any, "std::unique_ptr<int>", {}, cppdecl::SimplifyFlags::all);
    // On libc++:
    CheckRoundtrip("std::__1::unique_ptr<int, std::__1::default_delete<int>>", m_any, "std::__1::unique_ptr<int>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_default_delete);
    CheckRoundtrip("std::__1::unique_ptr<int, std::__1::default_delete<int>>", m_any, "std::unique_ptr<int>", {}, cppdecl::SimplifyFlags::all);
    // On MSVC STL:
    CheckRoundtrip("class std::unique_ptr<int,struct std::default_delete<int> >", m_any, "class std::unique_ptr<int>", {}, cppdecl::SimplifyFlags::bit_common_remove_defarg_default_delete);
    CheckRoundtrip("class std::unique_ptr<int,struct std::default_delete<int> >", m_any, "std::unique_ptr<int>", {}, cppdecl::SimplifyFlags::all);


    // Recursive rewrites:

    // This works regardless of preorder and postorder traversal, because all template arguments are spelled the same way.
    CheckRoundtrip(
        "std::vector<std::vector<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>>, std::allocator<std::vector<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>>>>",
        m_any,
        "std::vector<std::vector<std::string>>",
        {},
        cppdecl::SimplifyFlags::all
    );
    // This works only with postorder traversal, because different template arguments use different spellings.
    CheckRoundtrip(
        "std::vector<std::vector<std::string, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>>, std::allocator<std::vector<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>>>>",
        m_any,
        "std::vector<std::vector<std::string>>",
        {},
        cppdecl::SimplifyFlags::all
    );


    // Removing elaborated type specifiers that MSVC produces.
    CheckRoundtrip("class std::vector<int,class std::allocator<int> >", m_any, "std::vector<int>", {}, cppdecl::SimplifyFlags::all);


    // Removing numeric literal suffixes in certain known good situations.
    CheckRoundtrip("std::array<int, 42ul>", m_any, "std::array<int, 42>", {}, cppdecl::SimplifyFlags::bit_common_remove_numeric_literal_suffixes_from_known_good_template_params);
    CheckRoundtrip("std::__1::array<int, 42ull>", m_any, "std::__1::array<int, 42>", {}, cppdecl::SimplifyFlags::bit_common_remove_numeric_literal_suffixes_from_known_good_template_params);
    CheckRoundtrip("std::__1::array<int, 42ull>", m_any, "std::array<int, 42>", {}, cppdecl::SimplifyFlags::all);
    // Only applies to a single token, not to expressions:
    CheckRoundtrip("std::__1::array<int, 42ull + 43ull>", m_any, "std::array<int, 42ull+43ull>", {}, cppdecl::SimplifyFlags::all);


    // Attributes!
    CheckRoundtrip("[[hello]] int x", m_any, "[[hello]] int x");
    CheckParseSuccess("[[hello]] int x", m_any, R"({type="{attrs=[{style=cpp,attr=[{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="hello"}]}}]}],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"})");
    CheckParseSuccess("[[hello]] int x", m_any, "`x` of type `int`, with attribute [`hello`]", {});
    CheckParseSuccess("[[hello]] int x", m_any, "int_x", cppdecl::ToStringFlags::identifier); // Intentionally no attributes in identifiers.
    CheckRoundtrip("__attribute__((hello)) int x", m_any, "__attribute__((hello)) int x");
    CheckParseSuccess("__attribute__((hello)) int x", m_any, R"({type="{attrs=[{style=gnu,attr=[{attrs=[],flags=[],quals=[],name={global_scope=false,parts=[{name="hello"}]}}]}],flags=[],quals=[],name={global_scope=false,parts=[{name="int"}]}}",name="{global_scope=false,parts=[{name="x"}]}"})");
    CheckParseSuccess("__attribute__((hello)) int x", m_any, "`x` of type `int`, with GNU-style attribute [`hello`]", {});
    CheckParseSuccess("__attribute__((hello)) int x", m_any, "int_x", cppdecl::ToStringFlags::identifier); // Intentionally no attributes in identifiers.
    // Pretty-printing of multiple attributes:
    CheckParseSuccess("[[hello]] [[world]] int x", m_any, "`x` of type `int`, with attributes [[`hello`], [`world`]]", {});
    CheckParseSuccess("[[hello,world,blah]] int x", m_any, "`x` of type `int`, with attributes [[`hello`], [`world`], [`blah`]]", {});
    CheckParseSuccess("__attribute__((hello)) __attribute__((world)) int x", m_any, "`x` of type `int`, with attributes [GNU-style [`hello`], GNU-style [`world`]]", {});
    CheckParseSuccess("__attribute__((hello,world,blah)) int x", m_any, "`x` of type `int`, with attributes [GNU-style [`hello`], GNU-style [`world`], GNU-style [`blah`]]", {});
    // GNU-style attributes can be between and after the specifiers:
    CheckParseSuccess("int __attribute__((hello)) x", m_any, "`x` of type `int`, with GNU-style attribute [`hello`]", {});
    CheckParseSuccess("long __attribute__((hello)) long x", m_any, "`x` of type `long long`, with GNU-style attribute [`hello`]", {});
    // But C++-style attributes can't:
    CheckParseSuccessWithJunk("int [[hello]] x", m_any, 1, "unnamed array of size [list [[`hello`]]] of `int`", {}); // Parses to some junk, oh well.

    // Lone attribute in place of a declaration is a parse error:
    CheckParseFail("[[hello]]", m_any, 9, "Expected a type or a name.");

    // A bunch of mixed attributes:
    CheckParseSuccess("__attribute__((a, b)) [[c]] __attribute__((d)) long __attribute__((e)) long __attribute__((f, g)) x", m_any, "`x` of type `long long`, with attributes [GNU-style [`a`], GNU-style [`b`], [`c`], GNU-style [`d`], GNU-style [`e`], GNU-style [`f`], GNU-style [`g`]]", {});

    // Trailing attributes.
    // Those (when GNU-style) apply to the entire declaration, so we move them to the left.
    // Interestingly, libclang seems to print them on the right (this testcase comes straight from libclang). Maybe we should too? (only for GNU-style attributes, keep C++-style on the left)
    CheckRoundtrip("int (*)(int, char *, int *) __attribute__((cdecl))", m_any, "__attribute__((cdecl)) int (*)(int, char *, int *)");
    CheckRoundtrip("__attribute__((cdecl)) int (*)(int, char *, int *)", m_any, "__attribute__((cdecl)) int (*)(int, char *, int *)");
    // As I said above, GNU-style attributes can't be parenthesized.
    CheckParseFail("int ((*)() __attribute__((cdecl)))", m_any, 11, "Expected `)`.");

    CheckParseSuccess("[[]] int x", m_any, "`x` of type `int`", {});
    CheckParseSuccess("  [  [  ]  ]  int x", m_any, "`x` of type `int`", {});
    CheckParseFail("[[]", m_any, 3, "Expected a second `]` to close the attribute list.");
    CheckParseFail("[[] int x", m_any, 4, "Expected a second `]` to close the attribute list.");
    CheckParseFail("[[", m_any, 2, "Expected `]]` or `,` in the attribute list.");
    CheckParseSuccess("[[,]] int x", m_any, "`x` of type `int`", {});
    CheckParseSuccess("[[,,]] int x", m_any, "`x` of type `int`", {});
    CheckParseSuccess("[[a,b]] int x", m_any, "`x` of type `int`, with attributes [[`a`], [`b`]]", {});
    CheckParseSuccess("[  [  a  ,  b  ]  ] int x", m_any, "`x` of type `int`, with attributes [[`a`], [`b`]]", {});
    CheckParseSuccess("[[a,b,]] int x", m_any, "`x` of type `int`, with attributes [[`a`], [`b`]]", {});
    CheckParseSuccess("  [  [  a  ,  b  ,  ]  ]  int x", m_any, "`x` of type `int`, with attributes [[`a`], [`b`]]", {});
    CheckParseSuccess("[[,,,a,,,,,,b,,,,]] [[,,,,]] int x", m_any, "`x` of type `int`, with attributes [[`a`], [`b`]]", {});
    CheckParseFail("[[using", m_any, 7, "Expected attribute namespace after `using` in an attribute list.");
    CheckParseFail("[[using 1", m_any, 8, "Expected attribute namespace after `using` in an attribute list.");
    CheckParseFail("[[using N1", m_any, 10, "Expected `:` after `using <namespace>` at the beginning of an attribute list.");
    CheckParseSuccess("[[using N1:]] int x", m_any, "`x` of type `int`", {});
    CheckParseFail("[[using N1::]] int x", m_any, 10, "In attribute list, `using` only accepts unqualified names.");
    CheckParseFail("[[using N1]]", m_any, 10, "Expected `:` after `using <namespace>` at the beginning of an attribute list.");
    CheckParseSuccess("[[using N1: a]] int x", m_any, "`x` of type `int`, with attribute [`N1`::`a`]", {});
    CheckParseSuccess("[[using N1:a,b]] int x", m_any, "`x` of type `int`, with attributes [[`N1`::`a`], [`N1`::`b`]]", {});
    CheckParseSuccess("  [  [  using  N1  :  a  ,  b  ]  ]  int x", m_any, "`x` of type `int`, with attributes [[`N1`::`a`], [`N1`::`b`]]", {});
    CheckParseSuccess("[[using N1:,,a,,,,b,,,,]] int x", m_any, "`x` of type `int`, with attributes [[`N1`::`a`], [`N1`::`b`]]", {});
    CheckParseSuccess("  [  [  using  N1  :  ,  ,  a  ,  ,  ,  ,  b  ,  ,  ,  ,  ]  ] int x", m_any, "`x` of type `int`, with attributes [[`N1`::`a`], [`N1`::`b`]]", {});
    CheckParseFail("[[using N1: =a]] int x", m_any, 12, "The attribute list starts with `using <namespace>:`, but this attribute in the list doesn't start with a name that we can apply the qualifier to.");
    // GNU-style attributes can appear even in template parameters:
    CheckParseSuccess("A<__attribute__((noreturn)) int()> x", m_any, "`x` of type `A` with 1 template argument: [possibly type: a function taking no parameters, returning `int`, with GNU-style attribute [`noreturn`]]", {});
    CheckParseSuccess("A<int __attribute__((noreturn))()> x", m_any, "`x` of type `A` with 1 template argument: [possibly type: a function taking no parameters, returning `int`, with GNU-style attribute [`noreturn`]]", {});
    // No C++-style attributes in template argument lists. This still parses, but as a non-type template parameter.
    CheckParseSuccess("A<[[nodiscard]] int()> x", m_any, "`x` of type `A` with 1 template argument: [non-type: [list [[list [[`nodiscard`]]]], `int`, list ()]]", {});
    // No C++-style attributes in function parameters. This straight up fails to parse.
    CheckParseFail("void foo([[nodiscard]] int a())", m_any, 9, "C++-style attributes can't appear here.");
    CheckParseFail("void foo(  [[nodiscard]] int a())", m_any, 11, "C++-style attributes can't appear here.");
    // No C++-style attributes on lone types:
    CheckParseFail("[[nodiscard]] int", m_any, 14, "Expected a name."); // A weird-ass error. This ends up preferring the parse with no return type (because the error is later), and complains that `int` isn't a name.
    CheckParseFail("[[nodiscard]] int", m_named, 14, "Expected a name."); // Same.
    CheckParseFail("[[nodiscard]] int", m_type, 0, "Type names can't have leading C++-style attributes."); // Look, a proper error!
    CheckParseFail("  [[nodiscard]] int", m_type, 2, "Type names can't have leading C++-style attributes.");

    // Forgot the second `(` on an `__attribute__`, so this parses as a function. I guess it could be made into a hard error?
    CheckParseFail("__attribute__(using N1: a) int x", m_any, 22, "Expected `)` or `,` or `...` in function parameter list.");
    // No `using` in GNU-style attributes. This ends up being parsed as a pseudo-expression.
    CheckParseSuccess("__attribute__((using N1: a)) int x", m_any, "`x` of type `int`, with GNU-style attribute [`using`, `N1`, punctuation `:`, `a`]", {});

    // Some checks for GNU-style lists:
    CheckParseSuccess("__attribute__(()) int x", m_any, "`x` of type `int`", {});
    CheckParseSuccess("  __attribute__  (  (  )  )  int x", m_any, "`x` of type `int`", {});
    CheckParseFail("__attribute__(()", m_any, 16, "Expected a second `)` to close the GNU-style attribute list.");
    CheckParseFail("__attribute__(() int x", m_any, 17, "Expected a second `)` to close the GNU-style attribute list.");
    CheckParseFail("__attribute__((", m_any, 15, "Expected `))` or `,` in the GNU-style attribute list.");
    CheckParseSuccess("__attribute__((,)) int x", m_any, "`x` of type `int`", {});
    CheckParseSuccess("__attribute__((,,)) int x", m_any, "`x` of type `int`", {});
    CheckParseSuccess("__attribute__((a,b)) int x", m_any, "`x` of type `int`, with attributes [GNU-style [`a`], GNU-style [`b`]]", {});
    CheckParseSuccess("  __attribute__  (  (  a  ,  b  )  ) int x", m_any, "`x` of type `int`, with attributes [GNU-style [`a`], GNU-style [`b`]]", {});
    CheckParseSuccess("__attribute__((a,b,)) int x", m_any, "`x` of type `int`, with attributes [GNU-style [`a`], GNU-style [`b`]]", {});
    CheckParseSuccess("  __attribute__  (  (  a  ,  b  ,  )  )  int x", m_any, "`x` of type `int`, with attributes [GNU-style [`a`], GNU-style [`b`]]", {});
    CheckParseSuccess("__attribute__((,,,a,,,,,,b,,,,)) __attribute__((,,,,)) int x", m_any, "`x` of type `int`, with attributes [GNU-style [`a`], GNU-style [`b`]]", {});



    // Iterator type names on various platforms:
    #if 0
    cppdecl::Demangler d;
    //                                                                                           |    ## libstdc++ ##                                                                      |    ## libstdc++ with _GLIBCXX_DEBUG ##                                                                                                                                                                                                                                        |    ## libc++ ##                                                                                                                                                                    |    ## MSVC STL ##
    std::cout << d(typeid(std::array             <int, 42>   ::iterator      ).name()) << '\n'; //    int*                                                                                 |    int*                                                                                                                                                                                                                                                                       |    int*                                                                                                                                                                            |    class std::_Array_iterator<int,42>
    std::cout << d(typeid(std::array             <int, 42>   ::const_iterator).name()) << '\n'; //    int const*                                                                           |    int const*                                                                                                                                                                                                                                                                 |    int const*                                                                                                                                                                      |    class std::_Array_const_iterator<int,42>
    std::cout << d(typeid(std::vector            <int>       ::iterator      ).name()) << '\n'; //    __gnu_cxx::__normal_iterator<int*, std::vector<int, std::allocator<int> > >          |    __gnu_debug::_Safe_iterator<__gnu_cxx::__normal_iterator<int*, std::__cxx1998::vector<int, std::allocator<int> > >, std::__debug::vector<int, std::allocator<int> >, std::random_access_iterator_tag>                                                                      |    std::__1::__wrap_iter<int*>                                                                                                                                                     |    class std::_Vector_iterator<class std::_Vector_val<struct std::_Simple_types<int> > >
    std::cout << d(typeid(std::vector            <int>       ::const_iterator).name()) << '\n'; //    __gnu_cxx::__normal_iterator<int const*, std::vector<int, std::allocator<int> > >    |    __gnu_debug::_Safe_iterator<__gnu_cxx::__normal_iterator<int const*, std::__cxx1998::vector<int, std::allocator<int> > >, std::__debug::vector<int, std::allocator<int> >, std::random_access_iterator_tag>                                                                |    std::__1::__wrap_iter<int const*>                                                                                                                                               |    class std::_Vector_const_iterator<class std::_Vector_val<struct std::_Simple_types<int> > >
    std::cout << d(typeid(std::deque             <int>       ::iterator      ).name()) << '\n'; //    std::_Deque_iterator<int, int&, int*>                                                |    __gnu_debug::_Safe_iterator<std::__cxx1998::_Deque_iterator<int, int&, int*>, std::__debug::deque<int, std::allocator<int> >, std::random_access_iterator_tag>                                                                                                             |    std::__1::__deque_iterator<int, int*, int&, int**, long, 1024l>                                                                                                                 |    class std::_Deque_iterator<class std::_Deque_val<struct std::_Deque_simple_types<int> > >
    std::cout << d(typeid(std::deque             <int>       ::const_iterator).name()) << '\n'; //    std::_Deque_iterator<int, int const&, int const*>                                    |    __gnu_debug::_Safe_iterator<std::__cxx1998::_Deque_iterator<int, int const&, int const*>, std::__debug::deque<int, std::allocator<int> >, std::random_access_iterator_tag>                                                                                                 |    std::__1::__deque_iterator<int, int const*, int const&, int const* const*, long, 1024l>                                                                                         |    class std::_Deque_const_iterator<class std::_Deque_val<struct std::_Deque_simple_types<int> > >
    std::cout << d(typeid(std::forward_list      <int>       ::iterator      ).name()) << '\n'; //    std::_Fwd_list_iterator<int>                                                         |    __gnu_debug::_Safe_iterator<std::__cxx1998::_Fwd_list_iterator<int>, std::__debug::forward_list<int, std::allocator<int> >, std::forward_iterator_tag>                                                                                                                     |    std::__1::__forward_list_iterator<std::__1::__forward_list_node<int, void*>*>                                                                                                   |    class std::_Flist_iterator<class std::_Flist_val<struct std::_Flist_simple_types<int> > >
    std::cout << d(typeid(std::forward_list      <int>       ::const_iterator).name()) << '\n'; //    std::_Fwd_list_const_iterator<int>                                                   |    __gnu_debug::_Safe_iterator<std::__cxx1998::_Fwd_list_const_iterator<int>, std::__debug::forward_list<int, std::allocator<int> >, std::forward_iterator_tag>                                                                                                               |    std::__1::__forward_list_const_iterator<std::__1::__forward_list_node<int, void*>*>                                                                                             |    class std::_Flist_const_iterator<class std::_Flist_val<struct std::_Flist_simple_types<int> > >
    std::cout << d(typeid(std::list              <int>       ::iterator      ).name()) << '\n'; //    std::_List_iterator<int>                                                             |    __gnu_debug::_Safe_iterator<std::__cxx1998::_List_iterator<int>, std::__debug::list<int, std::allocator<int> >, std::bidirectional_iterator_tag>                                                                                                                           |    std::__1::__list_iterator<int, void*>                                                                                                                                           |    class std::_List_iterator<class std::_List_val<struct std::_List_simple_types<int> > >
    std::cout << d(typeid(std::list              <int>       ::const_iterator).name()) << '\n'; //    std::_List_const_iterator<int>                                                       |    __gnu_debug::_Safe_iterator<std::__cxx1998::_List_const_iterator<int>, std::__debug::list<int, std::allocator<int> >, std::bidirectional_iterator_tag>                                                                                                                     |    std::__1::__list_const_iterator<int, void*>                                                                                                                                     |    class std::_List_const_iterator<class std::_List_val<struct std::_List_simple_types<int> > >
    std::cout << d(typeid(std::set               <int>       ::iterator      ).name()) << '\n'; //    std::_Rb_tree_const_iterator<int>                                                    |    __gnu_debug::_Safe_iterator<std::_Rb_tree_const_iterator<int>, std::__debug::set<int, std::less<int>, std::allocator<int> >, std::bidirectional_iterator_tag>                                                                                                              |    std::__1::__tree_const_iterator<int, std::__1::__tree_node<int, void*>*, long>                                                                                                  |    class std::_Tree_const_iterator<class std::_Tree_val<struct std::_Tree_simple_types<int> > >
    std::cout << d(typeid(std::multiset          <int>       ::iterator      ).name()) << '\n'; //    std::_Rb_tree_const_iterator<int>                                                    |    __gnu_debug::_Safe_iterator<std::_Rb_tree_const_iterator<int>, std::__debug::multiset<int, std::less<int>, std::allocator<int> >, std::bidirectional_iterator_tag>                                                                                                         |    std::__1::__tree_const_iterator<int, std::__1::__tree_node<int, void*>*, long>                                                                                                  |    class std::_Tree_const_iterator<class std::_Tree_val<struct std::_Tree_simple_types<int> > >
    std::cout << d(typeid(std::set               <int>       ::const_iterator).name()) << '\n'; //    std::_Rb_tree_const_iterator<int>                                                    |    __gnu_debug::_Safe_iterator<std::_Rb_tree_const_iterator<int>, std::__debug::set<int, std::less<int>, std::allocator<int> >, std::bidirectional_iterator_tag>                                                                                                              |    std::__1::__tree_const_iterator<int, std::__1::__tree_node<int, void*>*, long>                                                                                                  |    class std::_Tree_const_iterator<class std::_Tree_val<struct std::_Tree_simple_types<int> > >
    std::cout << d(typeid(std::multiset          <int>       ::const_iterator).name()) << '\n'; //    std::_Rb_tree_const_iterator<int>                                                    |    __gnu_debug::_Safe_iterator<std::_Rb_tree_const_iterator<int>, std::__debug::multiset<int, std::less<int>, std::allocator<int> >, std::bidirectional_iterator_tag>                                                                                                         |    std::__1::__tree_const_iterator<int, std::__1::__tree_node<int, void*>*, long>                                                                                                  |    class std::_Tree_const_iterator<class std::_Tree_val<struct std::_Tree_simple_types<int> > >
    std::cout << d(typeid(std::map               <int, float>::iterator      ).name()) << '\n'; //    std::_Rb_tree_iterator<std::pair<int const, float> >                                 |    __gnu_debug::_Safe_iterator<std::_Rb_tree_iterator<std::pair<int const, float> >, std::__debug::map<int, float, std::less<int>, std::allocator<std::pair<int const, float> > >, std::bidirectional_iterator_tag>                                                           |    std::__1::__map_iterator<std::__1::__tree_iterator<std::__1::__value_type<int, float>, std::__1::__tree_node<std::__1::__value_type<int, float>, void*>*, long>>                |    class std::_Tree_iterator<class std::_Tree_val<struct std::_Tree_simple_types<struct std::pair<int const ,float> > > >
    std::cout << d(typeid(std::multimap          <int, float>::iterator      ).name()) << '\n'; //    std::_Rb_tree_iterator<std::pair<int const, float> >                                 |    __gnu_debug::_Safe_iterator<std::_Rb_tree_iterator<std::pair<int const, float> >, std::__debug::multimap<int, float, std::less<int>, std::allocator<std::pair<int const, float> > >, std::bidirectional_iterator_tag>                                                      |    std::__1::__map_iterator<std::__1::__tree_iterator<std::__1::__value_type<int, float>, std::__1::__tree_node<std::__1::__value_type<int, float>, void*>*, long>>                |    class std::_Tree_iterator<class std::_Tree_val<struct std::_Tree_simple_types<struct std::pair<int const ,float> > > >
    std::cout << d(typeid(std::map               <int, float>::const_iterator).name()) << '\n'; //    std::_Rb_tree_const_iterator<std::pair<int const, float> >                           |    __gnu_debug::_Safe_iterator<std::_Rb_tree_const_iterator<std::pair<int const, float> >, std::__debug::map<int, float, std::less<int>, std::allocator<std::pair<int const, float> > >, std::bidirectional_iterator_tag>                                                     |    std::__1::__map_const_iterator<std::__1::__tree_const_iterator<std::__1::__value_type<int, float>, std::__1::__tree_node<std::__1::__value_type<int, float>, void*>*, long>>    |    class std::_Tree_const_iterator<class std::_Tree_val<struct std::_Tree_simple_types<struct std::pair<int const ,float> > > >
    std::cout << d(typeid(std::multimap          <int, float>::const_iterator).name()) << '\n'; //    std::_Rb_tree_const_iterator<std::pair<int const, float> >                           |    __gnu_debug::_Safe_iterator<std::_Rb_tree_const_iterator<std::pair<int const, float> >, std::__debug::multimap<int, float, std::less<int>, std::allocator<std::pair<int const, float> > >, std::bidirectional_iterator_tag>                                                |    std::__1::__map_const_iterator<std::__1::__tree_const_iterator<std::__1::__value_type<int, float>, std::__1::__tree_node<std::__1::__value_type<int, float>, void*>*, long>>    |    class std::_Tree_const_iterator<class std::_Tree_val<struct std::_Tree_simple_types<struct std::pair<int const ,float> > > >
    std::cout << d(typeid(std::unordered_set     <int>       ::iterator      ).name()) << '\n'; //    std::__detail::_Node_iterator<int, true, false>                                      |    __gnu_debug::_Safe_iterator<std::__detail::_Node_iterator<int, true, false>, std::__debug::unordered_set<int, std::hash<int>, std::equal_to<int>, std::allocator<int> >, std::forward_iterator_tag>                                                                        |    std::__1::__hash_const_iterator<std::__1::__hash_node<int, void*>*>                                                                                                             |    class std::_List_const_iterator<class std::_List_val<struct std::_List_simple_types<int> > >
    std::cout << d(typeid(std::unordered_multiset<int>       ::iterator      ).name()) << '\n'; //    std::__detail::_Node_iterator<int, true, false>                                      |    __gnu_debug::_Safe_iterator<std::__detail::_Node_iterator<int, true, false>, std::__debug::unordered_multiset<int, std::hash<int>, std::equal_to<int>, std::allocator<int> >, std::forward_iterator_tag>                                                                   |    std::__1::__hash_const_iterator<std::__1::__hash_node<int, void*>*>                                                                                                             |    class std::_List_const_iterator<class std::_List_val<struct std::_List_simple_types<int> > >
    std::cout << d(typeid(std::unordered_set     <int>       ::const_iterator).name()) << '\n'; //    std::__detail::_Node_const_iterator<int, true, false>                                |    __gnu_debug::_Safe_iterator<std::__detail::_Node_const_iterator<int, true, false>, std::__debug::unordered_set<int, std::hash<int>, std::equal_to<int>, std::allocator<int> >, std::forward_iterator_tag>                                                                  |    std::__1::__hash_const_iterator<std::__1::__hash_node<int, void*>*>                                                                                                             |    class std::_List_const_iterator<class std::_List_val<struct std::_List_simple_types<int> > >
    std::cout << d(typeid(std::unordered_multiset<int>       ::const_iterator).name()) << '\n'; //    std::__detail::_Node_const_iterator<int, true, false>                                |    __gnu_debug::_Safe_iterator<std::__detail::_Node_const_iterator<int, true, false>, std::__debug::unordered_multiset<int, std::hash<int>, std::equal_to<int>, std::allocator<int> >, std::forward_iterator_tag>                                                             |    std::__1::__hash_const_iterator<std::__1::__hash_node<int, void*>*>                                                                                                             |    class std::_List_const_iterator<class std::_List_val<struct std::_List_simple_types<int> > >
    std::cout << d(typeid(std::unordered_map     <int, float>::iterator      ).name()) << '\n'; //    std::__detail::_Node_iterator<std::pair<int const, float>, false, false>             |    __gnu_debug::_Safe_iterator<std::__detail::_Node_iterator<std::pair<int const, float>, false, false>, std::__debug::unordered_map<int, float, std::hash<int>, std::equal_to<int>, std::allocator<std::pair<int const, float> > >, std::forward_iterator_tag>               |    std::__1::__hash_map_iterator<std::__1::__hash_iterator<std::__1::__hash_node<std::__1::__hash_value_type<int, float>, void*>*>>                                                |    class std::_List_iterator<class std::_List_val<struct std::_List_simple_types<struct std::pair<int const ,float> > > >
    std::cout << d(typeid(std::unordered_multimap<int, float>::iterator      ).name()) << '\n'; //    std::__detail::_Node_iterator<std::pair<int const, float>, false, false>             |    __gnu_debug::_Safe_iterator<std::__detail::_Node_iterator<std::pair<int const, float>, false, false>, std::__debug::unordered_multimap<int, float, std::hash<int>, std::equal_to<int>, std::allocator<std::pair<int const, float> > >, std::forward_iterator_tag>          |    std::__1::__hash_map_iterator<std::__1::__hash_iterator<std::__1::__hash_node<std::__1::__hash_value_type<int, float>, void*>*>>                                                |    class std::_List_iterator<class std::_List_val<struct std::_List_simple_types<struct std::pair<int const ,float> > > >
    std::cout << d(typeid(std::unordered_map     <int, float>::const_iterator).name()) << '\n'; //    std::__detail::_Node_const_iterator<std::pair<int const, float>, false, false>       |    __gnu_debug::_Safe_iterator<std::__detail::_Node_const_iterator<std::pair<int const, float>, false, false>, std::__debug::unordered_map<int, float, std::hash<int>, std::equal_to<int>, std::allocator<std::pair<int const, float> > >, std::forward_iterator_tag>         |    std::__1::__hash_map_const_iterator<std::__1::__hash_const_iterator<std::__1::__hash_node<std::__1::__hash_value_type<int, float>, void*>*>>                                    |    class std::_List_const_iterator<class std::_List_val<struct std::_List_simple_types<struct std::pair<int const ,float> > > >
    std::cout << d(typeid(std::unordered_multimap<int, float>::const_iterator).name()) << '\n'; //    std::__detail::_Node_const_iterator<std::pair<int const, float>, false, false>       |    __gnu_debug::_Safe_iterator<std::__detail::_Node_const_iterator<std::pair<int const, float>, false, false>, std::__debug::unordered_multimap<int, float, std::hash<int>, std::equal_to<int>, std::allocator<std::pair<int const, float> > >, std::forward_iterator_tag>    |    std::__1::__hash_map_const_iterator<std::__1::__hash_const_iterator<std::__1::__hash_node<std::__1::__hash_value_type<int, float>, void*>*>>                                    |    class std::_List_const_iterator<class std::_List_val<struct std::_List_simple_types<struct std::pair<int const ,float> > > >
    //                                                                                           |    ## NOTE: In unordered containers, the second-to-last bool is true for sets and       |                                                                                                                                                                                                                                                                               |    ## NOTE: All mentions of `long` here are replaced with `long long` on Windows! (Those are most probably `ptrdiff_t`.) And similarly `1024l` becomes `1024ll`.                   |    ## NOTE: std::[unordered_]set<T>::[const_]iterator are all equal to `std::list<T>::const_iterator.
    //                                                                                           |             false for maps.                                                             |                                                                                                                                                                                                                                                                               |                                                                                                                                                                                    |    ## NOTE: The mention of `simple_types` means that the allocator used doesn't use fancy pointers.
    // ## COMMON NOTES:
    // * On all implementations, `multi...` containers use the same iterators as the non-multi equivalents.
    // * On all implementations, `[multi]set` use the same iterators for const and non-const. The same is true for `unordered_[multi]set` on libc++ and MSVC STL, but not on libstdc++.
    // * MSVC is the only platform that doesn't use pointers for `std::array` iterators.
    #endif

    // --- libstdc++
    // std::vector
    CheckRoundtrip("__gnu_cxx::__normal_iterator<int*, std::vector<int, std::allocator<int> > >                     ", m_any, "std::vector<int, std::allocator<int>>::iterator",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("__gnu_cxx::__normal_iterator<int const*, std::vector<int, std::allocator<int> > >               ", m_any, "std::vector<int, std::allocator<int>>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("__gnu_cxx::__normal_iterator<int*, std::vector<int, std::allocator<int> > >       ::blah const *", m_any, "std::vector<int, std::allocator<int>>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("__gnu_cxx::__normal_iterator<int const*, std::vector<int, std::allocator<int> > > ::blah const *", m_any, "std::vector<int, std::allocator<int>>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("__gnu_cxx::__normal_iterator<int*, std::vector<int, std::allocator<int> > >                     ", m_any, "std::vector<int>::iterator",                                          cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::all);
    CheckRoundtrip("__gnu_cxx::__normal_iterator<int const*, std::vector<int, std::allocator<int> > >               ", m_any, "std::vector<int>::const_iterator",                                    cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::all);
    CheckRoundtrip("__gnu_cxx::__normal_iterator<int*, std::vector<int, std::allocator<int> > >       ::blah const *", m_any, "std::vector<int>::iterator::blah const *",                            cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::all);
    CheckRoundtrip("__gnu_cxx::__normal_iterator<int const*, std::vector<int, std::allocator<int> > > ::blah const *", m_any, "std::vector<int>::const_iterator::blah const *",                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::all);
    // std::deque
    CheckRoundtrip("std::_Deque_iterator<int, int&, int*>                                                           ", m_any, "std::deque<int>::iterator",                                           cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_Deque_iterator<int, int const&, int const*>                                               ", m_any, "std::deque<int>::const_iterator",                                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_Deque_iterator<int, int&, int*>             ::blah const *                                ", m_any, "std::deque<int>::iterator::blah const *",                             cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_Deque_iterator<int, int const&, int const*> ::blah const *                                ", m_any, "std::deque<int>::const_iterator::blah const *",                       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    // std::forward_list
    CheckRoundtrip("std::_Fwd_list_iterator<int>                                                                    ", m_any, "std::forward_list<int>::iterator",                                    cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_Fwd_list_const_iterator<int>                                                              ", m_any, "std::forward_list<int>::const_iterator",                              cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_Fwd_list_iterator<int>       ::blah const *                                               ", m_any, "std::forward_list<int>::iterator::blah const *",                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_Fwd_list_const_iterator<int> ::blah const *                                               ", m_any, "std::forward_list<int>::const_iterator::blah const *",                cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    // std::list
    CheckRoundtrip("std::_List_iterator<int>                                                                        ", m_any, "std::list<int>::iterator",                                            cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_List_const_iterator<int>                                                                  ", m_any, "std::list<int>::const_iterator",                                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_List_iterator<int>       ::blah const *                                                   ", m_any, "std::list<int>::iterator::blah const *",                              cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_List_const_iterator<int> ::blah const *                                                   ", m_any, "std::list<int>::const_iterator::blah const *",                        cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    // std::set, std::multiset, std::map, std::multimap
    CheckRoundtrip("std::_Rb_tree_const_iterator<int>                                                               ", m_any, "std::set<int>::const_iterator",                                       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_Rb_tree_iterator<std::pair<int const, float> >                                            ", m_any, "std::map<int, float>::iterator",                                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_Rb_tree_const_iterator<std::pair<int const, float> >                                      ", m_any, "std::map<int, float>::const_iterator",                                cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_Rb_tree_const_iterator<int>                          ::blah const *                       ", m_any, "std::set<int>::const_iterator::blah const *",                         cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_Rb_tree_iterator<std::pair<int const, float> >       ::blah const *                       ", m_any, "std::map<int, float>::iterator::blah const *",                        cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::_Rb_tree_const_iterator<std::pair<int const, float> > ::blah const *                       ", m_any, "std::map<int, float>::const_iterator::blah const *",                  cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    // std::unordered_set, std::unordered_multiset, std::unordered_map, std::unordered_multimap
    CheckRoundtrip("std::__detail::_Node_iterator<int, true, false>                                                 ", m_any, "std::unordered_set<int>::iterator",                                   cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::__detail::_Node_const_iterator<int, true, false>                                           ", m_any, "std::unordered_set<int>::const_iterator",                             cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::__detail::_Node_iterator<std::pair<int const, float>, false, false>                        ", m_any, "std::unordered_map<int, float>::iterator",                            cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::__detail::_Node_const_iterator<std::pair<int const, float>, false, false>                  ", m_any, "std::unordered_map<int, float>::const_iterator",                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::__detail::_Node_iterator<std::pair<int const, float>, true, false>                         ", m_any, "std::unordered_set<std::pair<int const, float>>::iterator",           cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::__detail::_Node_const_iterator<std::pair<int const, float>, true, false>                   ", m_any, "std::unordered_set<std::pair<int const, float>>::const_iterator",     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::__detail::_Node_iterator<int, true, false>                                ::blah const *   ", m_any, "std::unordered_set<int>::iterator::blah const *",                               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::__detail::_Node_const_iterator<int, true, false>                          ::blah const *   ", m_any, "std::unordered_set<int>::const_iterator::blah const *",                         cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::__detail::_Node_iterator<std::pair<int const, float>, false, false>       ::blah const *   ", m_any, "std::unordered_map<int, float>::iterator::blah const *",                        cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::__detail::_Node_const_iterator<std::pair<int const, float>, false, false> ::blah const *   ", m_any, "std::unordered_map<int, float>::const_iterator::blah const *",                  cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::__detail::_Node_iterator<std::pair<int const, float>, true, false>        ::blah const *   ", m_any, "std::unordered_set<std::pair<int const, float>>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);
    CheckRoundtrip("std::__detail::_Node_const_iterator<std::pair<int const, float>, true, false>  ::blah const *   ", m_any, "std::unordered_set<std::pair<int const, float>>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libstdcxx_normalize_iterators);

    // --- libc++
    // std::vector
    CheckRoundtrip("std::__1::__wrap_iter<int*>      ", m_any, "std::__1::vector<int>::iterator",                                    cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__wrap_iter<int const*>", m_any, "std::__1::vector<int>::const_iterator",                              cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__wrap_iter<int*>       ::blah const *", m_any, "std::__1::vector<int>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__wrap_iter<int const*> ::blah const *", m_any, "std::__1::vector<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__wrap_iter<int*>      ", m_any, "std::vector<int>::iterator",                                    cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__wrap_iter<int const*>", m_any, "std::vector<int>::const_iterator",                              cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__wrap_iter<int*>       ::blah const *", m_any, "std::vector<int>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__wrap_iter<int const*> ::blah const *", m_any, "std::vector<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    // std::deque
    CheckRoundtrip("std::__1::__deque_iterator<int, int*, int&, int**, long, 1024l>                                       ", m_any, "std::__1::deque<int>::iterator",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__deque_iterator<int, int const*, int const&, int const* const*, long, 1024l>               ", m_any, "std::__1::deque<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__deque_iterator<int, int*, int&, int**, long, 1024l>                         ::blah const *", m_any, "std::__1::deque<int>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__deque_iterator<int, int const*, int const&, int const* const*, long, 1024l> ::blah const *", m_any, "std::__1::deque<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__deque_iterator<int, int*, int&, int**, long, 1024l>                                       ", m_any, "std::deque<int>::iterator",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__deque_iterator<int, int const*, int const&, int const* const*, long, 1024l>               ", m_any, "std::deque<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__deque_iterator<int, int*, int&, int**, long, 1024l>                         ::blah const *", m_any, "std::deque<int>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__deque_iterator<int, int const*, int const&, int const* const*, long, 1024l> ::blah const *", m_any, "std::deque<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    // std::deque, now with `long long`, as it would be on Windows.
    CheckRoundtrip("std::__1::__deque_iterator<int, int*, int&, int**, long long, 1024ll>                                       ", m_any, "std::__1::deque<int>::iterator",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__deque_iterator<int, int const*, int const&, int const* const*, long long, 1024ll>               ", m_any, "std::__1::deque<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__deque_iterator<int, int*, int&, int**, long long, 1024ll>                         ::blah const *", m_any, "std::__1::deque<int>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__deque_iterator<int, int const*, int const&, int const* const*, long long, 1024ll> ::blah const *", m_any, "std::__1::deque<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__deque_iterator<int, int*, int&, int**, long long, 1024ll>                                       ", m_any, "std::deque<int>::iterator",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__deque_iterator<int, int const*, int const&, int const* const*, long long, 1024ll>               ", m_any, "std::deque<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__deque_iterator<int, int*, int&, int**, long long, 1024ll>                         ::blah const *", m_any, "std::deque<int>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__deque_iterator<int, int const*, int const&, int const* const*, long long, 1024ll> ::blah const *", m_any, "std::deque<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    // std::forward_list
    CheckRoundtrip("std::__1::__forward_list_iterator<std::__1::__forward_list_node<int, void*>*>                     ", m_any, "std::__1::forward_list<int>::iterator",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__forward_list_const_iterator<std::__1::__forward_list_node<int, void*>*>               ", m_any, "std::__1::forward_list<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__forward_list_iterator<std::__1::__forward_list_node<int, void*>*>       ::blah const *", m_any, "std::__1::forward_list<int>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__forward_list_const_iterator<std::__1::__forward_list_node<int, void*>*> ::blah const *", m_any, "std::__1::forward_list<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__forward_list_iterator<std::__forward_list_node<int, void*>*>                     ", m_any, "std::forward_list<int>::iterator",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__forward_list_const_iterator<std::__forward_list_node<int, void*>*>               ", m_any, "std::forward_list<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__forward_list_iterator<std::__forward_list_node<int, void*>*>       ::blah const *", m_any, "std::forward_list<int>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__forward_list_const_iterator<std::__forward_list_node<int, void*>*> ::blah const *", m_any, "std::forward_list<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    // std::list
    CheckRoundtrip("std::__1::__list_iterator<int, void*>                     ", m_any, "std::__1::list<int>::iterator",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__list_const_iterator<int, void*>               ", m_any, "std::__1::list<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__list_iterator<int, void*>       ::blah const *", m_any, "std::__1::list<int>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__list_const_iterator<int, void*> ::blah const *", m_any, "std::__1::list<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__list_iterator<int, void*>                     ", m_any, "std::list<int>::iterator",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__list_const_iterator<int, void*>               ", m_any, "std::list<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__list_iterator<int, void*>       ::blah const *", m_any, "std::list<int>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__list_const_iterator<int, void*> ::blah const *", m_any, "std::list<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    // std::set and std::multiset
    CheckRoundtrip("std::__1::__tree_const_iterator<int, std::__1::__tree_node<int, void*>*, long>               ", m_any, "std::__1::set<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__tree_const_iterator<int, std::__1::__tree_node<int, void*>*, long> ::blah const *", m_any, "std::__1::set<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__tree_const_iterator<int, std::__tree_node<int, void*>*, long>               ", m_any, "std::set<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__tree_const_iterator<int, std::__tree_node<int, void*>*, long> ::blah const *", m_any, "std::set<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    // std::set and std::multiset, now with `long long`, as it would be on Windows.
    CheckRoundtrip("std::__1::__tree_const_iterator<int, std::__1::__tree_node<int, void*>*, long long>               ", m_any, "std::__1::set<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__tree_const_iterator<int, std::__1::__tree_node<int, void*>*, long long> ::blah const *", m_any, "std::__1::set<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__tree_const_iterator<int, std::__tree_node<int, void*>*, long long>               ", m_any, "std::set<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__tree_const_iterator<int, std::__tree_node<int, void*>*, long long> ::blah const *", m_any, "std::set<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    // std::map and std::multimap
    CheckRoundtrip("std::__1::__map_iterator<std::__1::__tree_iterator<std::__1::__value_type<int, float>, std::__1::__tree_node<std::__1::__value_type<int, float>, void*>*, long>>                           ", m_any, "std::__1::map<int, float>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__map_const_iterator<std::__1::__tree_const_iterator<std::__1::__value_type<int, float>, std::__1::__tree_node<std::__1::__value_type<int, float>, void*>*, long>>               ", m_any, "std::__1::map<int, float>::const_iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__map_iterator<std::__1::__tree_iterator<std::__1::__value_type<int, float>, std::__1::__tree_node<std::__1::__value_type<int, float>, void*>*, long>>             ::blah const *", m_any, "std::__1::map<int, float>::iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__map_const_iterator<std::__1::__tree_const_iterator<std::__1::__value_type<int, float>, std::__1::__tree_node<std::__1::__value_type<int, float>, void*>*, long>> ::blah const *", m_any, "std::__1::map<int, float>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__map_iterator<std::__tree_iterator<std::__value_type<int, float>, std::__tree_node<std::__value_type<int, float>, void*>*, long>>                           ", m_any, "std::map<int, float>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__map_const_iterator<std::__tree_const_iterator<std::__value_type<int, float>, std::__tree_node<std::__value_type<int, float>, void*>*, long>>               ", m_any, "std::map<int, float>::const_iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__map_iterator<std::__tree_iterator<std::__value_type<int, float>, std::__tree_node<std::__value_type<int, float>, void*>*, long>>             ::blah const *", m_any, "std::map<int, float>::iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__map_const_iterator<std::__tree_const_iterator<std::__value_type<int, float>, std::__tree_node<std::__value_type<int, float>, void*>*, long>> ::blah const *", m_any, "std::map<int, float>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    // std::unordered_set and std::unordered_multiset
    CheckRoundtrip("std::__1::__hash_const_iterator<std::__1::__hash_node<int, void*>*>               ", m_any, "std::__1::unordered_set<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__hash_const_iterator<std::__1::__hash_node<int, void*>*> ::blah const *", m_any, "std::__1::unordered_set<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__hash_const_iterator<std::__hash_node<int, void*>*>               ", m_any, "std::unordered_set<int>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__hash_const_iterator<std::__hash_node<int, void*>*> ::blah const *", m_any, "std::unordered_set<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    // std::unordered_map and std::unordered_multimap
    CheckRoundtrip("std::__1::__hash_map_iterator<std::__1::__hash_iterator<std::__1::__hash_node<std::__1::__hash_value_type<int, float>, void*>*>>                           ", m_any, "std::__1::unordered_map<int, float>::iterator",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__hash_map_const_iterator<std::__1::__hash_const_iterator<std::__1::__hash_node<std::__1::__hash_value_type<int, float>, void*>*>>               ", m_any, "std::__1::unordered_map<int, float>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__hash_map_iterator<std::__1::__hash_iterator<std::__1::__hash_node<std::__1::__hash_value_type<int, float>, void*>*>>             ::blah const *", m_any, "std::__1::unordered_map<int, float>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__1::__hash_map_const_iterator<std::__1::__hash_const_iterator<std::__1::__hash_node<std::__1::__hash_value_type<int, float>, void*>*>> ::blah const *", m_any, "std::__1::unordered_map<int, float>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__hash_map_iterator<std::__hash_iterator<std::__hash_node<std::__hash_value_type<int, float>, void*>*>>                           ", m_any, "std::unordered_map<int, float>::iterator",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__hash_map_const_iterator<std::__hash_const_iterator<std::__hash_node<std::__hash_value_type<int, float>, void*>*>>               ", m_any, "std::unordered_map<int, float>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__hash_map_iterator<std::__hash_iterator<std::__hash_node<std::__hash_value_type<int, float>, void*>*>>             ::blah const *", m_any, "std::unordered_map<int, float>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);
    CheckRoundtrip("std::__hash_map_const_iterator<std::__hash_const_iterator<std::__hash_node<std::__hash_value_type<int, float>, void*>*>> ::blah const *", m_any, "std::unordered_map<int, float>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_libcpp_normalize_iterators);

    // --- MSVC STL
    // std::array
    CheckRoundtrip("class std::_Array_iterator<int,42>                     ", m_any, "class std::array<int, 42>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Array_const_iterator<int,42>               ", m_any, "class std::array<int, 42>::const_iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Array_iterator<int,42>       ::blah const *", m_any, "class std::array<int, 42>::iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Array_const_iterator<int,42> ::blah const *", m_any, "class std::array<int, 42>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    // std::vector
    CheckRoundtrip("class std::_Vector_iterator<class std::_Vector_val<struct std::_Simple_types<int> > >                     ", m_any, "class std::vector<int>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Vector_const_iterator<class std::_Vector_val<struct std::_Simple_types<int> > >               ", m_any, "class std::vector<int>::const_iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Vector_iterator<class std::_Vector_val<struct std::_Simple_types<int> > >       ::blah const *", m_any, "class std::vector<int>::iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Vector_const_iterator<class std::_Vector_val<struct std::_Simple_types<int> > > ::blah const *", m_any, "class std::vector<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    // std::deque
    CheckRoundtrip("class std::_Deque_iterator<class std::_Deque_val<struct std::_Deque_simple_types<int> > >                     ", m_any, "class std::deque<int>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Deque_const_iterator<class std::_Deque_val<struct std::_Deque_simple_types<int> > >               ", m_any, "class std::deque<int>::const_iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Deque_iterator<class std::_Deque_val<struct std::_Deque_simple_types<int> > >       ::blah const *", m_any, "class std::deque<int>::iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Deque_const_iterator<class std::_Deque_val<struct std::_Deque_simple_types<int> > > ::blah const *", m_any, "class std::deque<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    // std::forward_list
    CheckRoundtrip("class std::_Flist_iterator<class std::_Flist_val<struct std::_Flist_simple_types<int> > >                     ", m_any, "class std::forward_list<int>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Flist_const_iterator<class std::_Flist_val<struct std::_Flist_simple_types<int> > >               ", m_any, "class std::forward_list<int>::const_iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Flist_iterator<class std::_Flist_val<struct std::_Flist_simple_types<int> > >       ::blah const *", m_any, "class std::forward_list<int>::iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Flist_const_iterator<class std::_Flist_val<struct std::_Flist_simple_types<int> > > ::blah const *", m_any, "class std::forward_list<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    // std::list
    CheckRoundtrip("class std::_List_iterator<class std::_List_val<struct std::_List_simple_types<int> > >                     ", m_any, "class std::list<int>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_List_const_iterator<class std::_List_val<struct std::_List_simple_types<int> > >               ", m_any, "class std::list<int>::const_iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_List_iterator<class std::_List_val<struct std::_List_simple_types<int> > >       ::blah const *", m_any, "class std::list<int>::iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_List_const_iterator<class std::_List_val<struct std::_List_simple_types<int> > > ::blah const *", m_any, "class std::list<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    // std::list, now with all simplification rules enabled (the names overlap with libstdc++, so we have to carefully work around this)
    CheckRoundtrip("class std::_List_iterator<class std::_List_val<struct std::_List_simple_types<int> > >                     ", m_any, "class std::list<int>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bits_normalize_iterators);
    CheckRoundtrip("class std::_List_const_iterator<class std::_List_val<struct std::_List_simple_types<int> > >               ", m_any, "class std::list<int>::const_iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bits_normalize_iterators);
    CheckRoundtrip("class std::_List_iterator<class std::_List_val<struct std::_List_simple_types<int> > >       ::blah const *", m_any, "class std::list<int>::iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bits_normalize_iterators);
    CheckRoundtrip("class std::_List_const_iterator<class std::_List_val<struct std::_List_simple_types<int> > > ::blah const *", m_any, "class std::list<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bits_normalize_iterators);
    // std::set, std::multiset
    CheckRoundtrip("class std::_Tree_const_iterator<class std::_Tree_val<struct std::_Tree_simple_types<int> > >               ", m_any, "class std::set<int>::const_iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Tree_const_iterator<class std::_Tree_val<struct std::_Tree_simple_types<int> > > ::blah const *", m_any, "class std::set<int>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    // std::map
    CheckRoundtrip("class std::_Tree_iterator<class std::_Tree_val<struct std::_Tree_simple_types<struct std::pair<int const ,float> > > >      ", m_any, "class std::map<int, float>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Tree_const_iterator<class std::_Tree_val<struct std::_Tree_simple_types<struct std::pair<int const ,float> > > >", m_any, "class std::map<int, float>::const_iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Tree_iterator<class std::_Tree_val<struct std::_Tree_simple_types<struct std::pair<int const ,float> > > >       ::blah const *", m_any, "class std::map<int, float>::iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_Tree_const_iterator<class std::_Tree_val<struct std::_Tree_simple_types<struct std::pair<int const ,float> > > > ::blah const *", m_any, "class std::map<int, float>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    // std::unordered_map
    CheckRoundtrip("class std::_List_iterator<class std::_List_val<struct std::_List_simple_types<struct std::pair<int const ,float> > > >      ", m_any, "class std::unordered_map<int, float>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_List_const_iterator<class std::_List_val<struct std::_List_simple_types<struct std::pair<int const ,float> > > >", m_any, "class std::unordered_map<int, float>::const_iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_List_iterator<class std::_List_val<struct std::_List_simple_types<struct std::pair<int const ,float> > > >       ::blah const *", m_any, "class std::unordered_map<int, float>::iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);
    CheckRoundtrip("class std::_List_const_iterator<class std::_List_val<struct std::_List_simple_types<struct std::pair<int const ,float> > > > ::blah const *", m_any, "class std::unordered_map<int, float>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_msvcstl_normalize_iterators);


    // Now third-party simplification!

    // --- phmap
    struct TraitsPhmap : cppdecl::SimplifyTraits<TraitsPhmap, cppdecl::SimplifyModules::Phmap> {};
    CheckRoundtrip("phmap::flat_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>",                                                                               m_any, "phmap::flat_hash_set<int>",                               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::flat_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>> ::blah const *",                                                                m_any, "phmap::flat_hash_set<int>::blah const *",                 cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::flat_hash_map<int, float, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>",                                                m_any, "phmap::flat_hash_map<int, float>",                        cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::flat_hash_map<int, float, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>> ::blah const *",                                 m_any, "phmap::flat_hash_map<int, float>::blah const *",          cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::node_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>",                                                                               m_any, "phmap::node_hash_set<int>",                               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::node_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>> ::blah const *",                                                                m_any, "phmap::node_hash_set<int>::blah const *",                 cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::node_hash_map<int, float, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>",                                                m_any, "phmap::node_hash_map<int, float>",                        cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::node_hash_map<int, float, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>> ::blah const *",                                 m_any, "phmap::node_hash_map<int, float>::blah const *",          cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::parallel_flat_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>, 4ul, phmap::NullMutex>",                                               m_any, "phmap::parallel_flat_hash_set<int>",                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::parallel_flat_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>, 4ul, phmap::NullMutex> ::blah const *",                                m_any, "phmap::parallel_flat_hash_set<int>::blah const *",        cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::parallel_flat_hash_map<int, float, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>, 4ul, phmap::NullMutex>",                m_any, "phmap::parallel_flat_hash_map<int, float>",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::parallel_flat_hash_map<int, float, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>, 4ul, phmap::NullMutex> ::blah const *", m_any, "phmap::parallel_flat_hash_map<int, float>::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::parallel_node_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>, 4ul, phmap::NullMutex>",                                               m_any, "phmap::parallel_node_hash_set<int>",                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::parallel_node_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>, 4ul, phmap::NullMutex> ::blah const *",                                m_any, "phmap::parallel_node_hash_set<int>::blah const *",        cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::parallel_node_hash_map<int, float, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>, 4ul, phmap::NullMutex>",                m_any, "phmap::parallel_node_hash_map<int, float>",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::parallel_node_hash_map<int, float, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>, 4ul, phmap::NullMutex> ::blah const *", m_any, "phmap::parallel_node_hash_map<int, float>::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::btree_set<int, phmap::Less<int>, std::allocator<int>>",                                                                                                        m_any, "phmap::btree_set<int>",                                   cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::btree_set<int, phmap::Less<int>, std::allocator<int>> ::blah const *",                                                                                         m_any, "phmap::btree_set<int>::blah const *",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::btree_map<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>>",                                                                         m_any, "phmap::btree_map<int, float>",                            cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::btree_map<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>> ::blah const *",                                                          m_any, "phmap::btree_map<int, float>::blah const *",              cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::btree_multiset<int, phmap::Less<int>, std::allocator<int>>",                                                                                                   m_any, "phmap::btree_multiset<int>",                              cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::btree_multiset<int, phmap::Less<int>, std::allocator<int>> ::blah const *",                                                                                    m_any, "phmap::btree_multiset<int>::blah const *",                cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::btree_multimap<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>>",                                                                    m_any, "phmap::btree_multimap<int, float>",                       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::btree_multimap<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>> ::blah const *",                                                     m_any, "phmap::btree_multimap<int, float>::blah const *",         cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::flat_hash_set<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, phmap::priv::StringHashEqT<char>::Hash, phmap::priv::StringHashEqT<char>::Eq, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>>",                m_any, "phmap::flat_hash_set<std::string>",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::all, TraitsPhmap{});
    CheckRoundtrip("phmap::flat_hash_set<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, phmap::priv::StringHashEqT<char>::Hash, phmap::priv::StringHashEqT<char>::Eq, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>> ::blah const *", m_any, "phmap::flat_hash_set<std::string>::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::all, TraitsPhmap{});
    CheckRoundtrip("phmap::flat_hash_set<int*, phmap::priv::HashEq<int*, void>::Hash, phmap::priv::HashEq<int*, void>::Eq, std::allocator<int*>>",                                        m_any, "phmap::flat_hash_set<int *>",                             cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::flat_hash_set<int*, phmap::priv::HashEq<int*, void>::Hash, phmap::priv::HashEq<int*, void>::Eq, std::allocator<int*>> ::blah const *",                         m_any, "phmap::flat_hash_set<int *>::blah const *",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    // On MSVC:
    CheckRoundtrip("class phmap::flat_hash_set<int,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<int> >",                                                                          m_any, "class phmap::flat_hash_set<int>",                 {}, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::flat_hash_map<int,float,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<struct std::pair<int const ,float> > >",                                    m_any, "class phmap::flat_hash_map<int, float>",          {}, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::node_hash_set<int,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<int> >",                                                                          m_any, "class phmap::node_hash_set<int>",                 {}, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::node_hash_map<int,float,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<struct std::pair<int const ,float> > >",                                    m_any, "class phmap::node_hash_map<int, float>",          {}, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::parallel_flat_hash_set<int,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<int>,4,class phmap::NullMutex>",                                         m_any, "class phmap::parallel_flat_hash_set<int>",        {}, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::parallel_flat_hash_map<int,float,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<struct std::pair<int const ,float> >,4,class phmap::NullMutex>",   m_any, "class phmap::parallel_flat_hash_map<int, float>", {}, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::parallel_node_hash_set<int,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<int>,4,class phmap::NullMutex>",                                         m_any, "class phmap::parallel_node_hash_set<int>",        {}, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::parallel_node_hash_map<int,float,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<struct std::pair<int const ,float> >,4,class phmap::NullMutex>",   m_any, "class phmap::parallel_node_hash_map<int, float>", {}, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::btree_set<int,struct phmap::Less<int>,class std::allocator<int> >",                                                                                                         m_any, "class phmap::btree_set<int>",                     {}, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::btree_map<int,float,struct phmap::Less<int>,class std::allocator<struct std::pair<int const ,float> > >",                                                                   m_any, "class phmap::btree_map<int, float>",              {}, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::btree_multiset<int,struct phmap::Less<int>,class std::allocator<int> >",                                                                                                    m_any, "class phmap::btree_multiset<int>",                {}, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::btree_multimap<int,float,struct phmap::Less<int>,class std::allocator<struct std::pair<int const ,float> > >",                                                              m_any, "class phmap::btree_multimap<int, float>",         {}, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::flat_hash_set<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct phmap::Hash<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > >,struct phmap::EqualTo<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > >,class std::allocator<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > > >", m_any, "phmap::flat_hash_set<std::string>", {}, cppdecl::SimplifyFlags::all, TraitsPhmap{});
    CheckRoundtrip("class phmap::flat_hash_set<int * __ptr64,struct phmap::priv::HashEq<int * __ptr64,void>::Hash,struct phmap::priv::HashEq<int * __ptr64,void>::Eq,class std::allocator<int * __ptr64> >", m_any, "class phmap::flat_hash_set<int *__ptr64>",        {}, cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::flat_hash_set<int * __ptr64,struct phmap::priv::HashEq<int * __ptr64,void>::Hash,struct phmap::priv::HashEq<int * __ptr64,void>::Eq,class std::allocator<int * __ptr64> >", m_any, "phmap::flat_hash_set<int *>",                     {}, cppdecl::SimplifyFlags::all, TraitsPhmap{});
    // Iterators:
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator",                                                                                                                                                                                                                   m_any, "phmap::flat_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators, TraitsPhmap{});
    CheckRoundtrip("class phmap::priv::raw_hash_set<struct phmap::priv::FlatHashSetPolicy<int>,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<int> >::iterator",                                                                                                                                                                                    m_any, "class phmap::flat_hash_set<int>::iterator",                               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator",                                                                                                                                                                                                                   m_any, "phmap::flat_hash_set<int>::iterator",                                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator ::blah const *",                                                                                                                                                                                                    m_any, "phmap::flat_hash_set<int>::iterator::blah const *",                       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::const_iterator",                                                                                                                                                                                                             m_any, "phmap::flat_hash_set<int>::const_iterator",                               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::const_iterator ::blah const *",                                                                                                                                                                                              m_any, "phmap::flat_hash_set<int>::const_iterator::blah const *",                 cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator",                                                                                                                                                                                    m_any, "phmap::flat_hash_map<int, float, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators, TraitsPhmap{});
    CheckRoundtrip("class phmap::priv::raw_hash_set<struct phmap::priv::FlatHashMapPolicy<int,float>,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<struct std::pair<int const ,float> > >::iterator",                                                                                                                                              m_any, "class phmap::flat_hash_map<int, float>::iterator",                        cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator",                                                                                                                                                                                    m_any, "phmap::flat_hash_map<int, float>::iterator",                              cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator ::blah const *",                                                                                                                                                                     m_any, "phmap::flat_hash_map<int, float>::iterator::blah const *",                cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::const_iterator",                                                                                                                                                                              m_any, "phmap::flat_hash_map<int, float>::const_iterator",                        cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::const_iterator ::blah const *",                                                                                                                                                               m_any, "phmap::flat_hash_map<int, float>::const_iterator::blah const *",          cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::NodeHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator",                                                                                                                                                                                                                   m_any, "phmap::node_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators, TraitsPhmap{});
    CheckRoundtrip("class phmap::priv::raw_hash_set<struct phmap::priv::NodeHashSetPolicy<int>,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<int> >::iterator",                                                                                                                                                                                    m_any, "class phmap::node_hash_set<int>::iterator",                               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::NodeHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator",                                                                                                                                                                                                                   m_any, "phmap::node_hash_set<int>::iterator",                                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::NodeHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator ::blah const *",                                                                                                                                                                                                    m_any, "phmap::node_hash_set<int>::iterator::blah const *",                       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::NodeHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::const_iterator",                                                                                                                                                                                                             m_any, "phmap::node_hash_set<int>::const_iterator",                               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::NodeHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::const_iterator ::blah const *",                                                                                                                                                                                              m_any, "phmap::node_hash_set<int>::const_iterator::blah const *",                 cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::NodeHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator",                                                                                                                                                                                    m_any, "phmap::node_hash_map<int, float, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators, TraitsPhmap{});
    CheckRoundtrip("class phmap::priv::raw_hash_set<class phmap::priv::NodeHashMapPolicy<int,float>,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<struct std::pair<int const ,float> > >::iterator",                                                                                                                                               m_any, "class phmap::node_hash_map<int, float>::iterator",                        cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::NodeHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator",                                                                                                                                                                                    m_any, "phmap::node_hash_map<int, float>::iterator",                              cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::NodeHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator ::blah const *",                                                                                                                                                                     m_any, "phmap::node_hash_map<int, float>::iterator::blah const *",                cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::NodeHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::const_iterator",                                                                                                                                                                              m_any, "phmap::node_hash_map<int, float>::const_iterator",                        cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::NodeHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::const_iterator ::blah const *",                                                                                                                                                               m_any, "phmap::node_hash_map<int, float>::const_iterator::blah const *",          cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::FlatHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator",                                                                                                                                                            m_any, "phmap::parallel_flat_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>, 4ul, phmap::NullMutex>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators, TraitsPhmap{});
    CheckRoundtrip("class phmap::priv::parallel_hash_set<4,class phmap::priv::raw_hash_set,class phmap::NullMutex,struct phmap::priv::FlatHashSetPolicy<int>,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<int> >::iterator",                                                                                                                      m_any, "class phmap::parallel_flat_hash_set<int>::iterator",                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::FlatHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator",                                                                                                                                                            m_any, "phmap::parallel_flat_hash_set<int>::iterator",                            cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::FlatHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator ::blah const *",                                                                                                                                             m_any, "phmap::parallel_flat_hash_set<int>::iterator::blah const *",              cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::FlatHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::const_iterator",                                                                                                                                                      m_any, "phmap::parallel_flat_hash_set<int>::const_iterator",                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::FlatHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::const_iterator ::blah const *",                                                                                                                                       m_any, "phmap::parallel_flat_hash_set<int>::const_iterator::blah const *",        cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::FlatHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator",                                                                                                                             m_any, "phmap::parallel_flat_hash_map<int, float, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>, 4ul, phmap::NullMutex>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators, TraitsPhmap{});
    CheckRoundtrip("class phmap::priv::parallel_hash_set<4,class phmap::priv::raw_hash_set,class phmap::NullMutex,struct phmap::priv::FlatHashMapPolicy<int,float>,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<struct std::pair<int const ,float> > >::iterator",                                                                                m_any, "class phmap::parallel_flat_hash_map<int, float>::iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::FlatHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator",                                                                                                                             m_any, "phmap::parallel_flat_hash_map<int, float>::iterator",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::FlatHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator ::blah const *",                                                                                                              m_any, "phmap::parallel_flat_hash_map<int, float>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::FlatHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::const_iterator",                                                                                                                       m_any, "phmap::parallel_flat_hash_map<int, float>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::FlatHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::const_iterator ::blah const *",                                                                                                        m_any, "phmap::parallel_flat_hash_map<int, float>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::NodeHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator",                                                                                                                                                            m_any, "phmap::parallel_node_hash_set<int, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>, 4ul, phmap::NullMutex>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators, TraitsPhmap{});
    CheckRoundtrip("class phmap::priv::parallel_hash_set<4,class phmap::priv::raw_hash_set,class phmap::NullMutex,struct phmap::priv::NodeHashSetPolicy<int>,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<int> >::iterator",                                                                                                                      m_any, "class phmap::parallel_node_hash_set<int>::iterator",                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::NodeHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator",                                                                                                                                                            m_any, "phmap::parallel_node_hash_set<int>::iterator",                            cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::NodeHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::iterator ::blah const *",                                                                                                                                             m_any, "phmap::parallel_node_hash_set<int>::iterator::blah const *",              cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::NodeHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::const_iterator",                                                                                                                                                      m_any, "phmap::parallel_node_hash_set<int>::const_iterator",                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::NodeHashSetPolicy<int>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<int>>::const_iterator ::blah const *",                                                                                                                                       m_any, "phmap::parallel_node_hash_set<int>::const_iterator::blah const *",        cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::NodeHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator",                                                                                                                             m_any, "phmap::parallel_node_hash_map<int, float, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>, 4ul, phmap::NullMutex>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators, TraitsPhmap{});
    CheckRoundtrip("class phmap::priv::parallel_hash_set<4,class phmap::priv::raw_hash_set,class phmap::NullMutex,class phmap::priv::NodeHashMapPolicy<int,float>,struct phmap::Hash<int>,struct phmap::EqualTo<int>,class std::allocator<struct std::pair<int const ,float> > >::iterator",                                                                                 m_any, "class phmap::parallel_node_hash_map<int, float>::iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::NodeHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator",                                                                                                                             m_any, "phmap::parallel_node_hash_map<int, float>::iterator",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::NodeHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::iterator ::blah const *",                                                                                                              m_any, "phmap::parallel_node_hash_map<int, float>::iterator::blah const *",       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::NodeHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::const_iterator",                                                                                                                       m_any, "phmap::parallel_node_hash_map<int, float>::const_iterator",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::parallel_hash_set<4ul, phmap::priv::raw_hash_set, phmap::NullMutex, phmap::priv::NodeHashMapPolicy<int, float>, phmap::Hash<int>, phmap::EqualTo<int>, std::allocator<std::pair<int const, float>>>::const_iterator ::blah const *",                                                                                                        m_any, "phmap::parallel_node_hash_map<int, float>::const_iterator::blah const *", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::set_params<int, phmap::Less<int>, std::allocator<int>, 256, false>>, int&, int*>",                                                                                                                                                                                                      m_any, "phmap::btree_set<int, phmap::Less<int>, std::allocator<int>>::iterator",  cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators, TraitsPhmap{});
    CheckRoundtrip("struct phmap::priv::btree_iterator<class phmap::priv::btree_node<struct phmap::priv::set_params<int,struct phmap::Less<int>,class std::allocator<int>,256,0> >,int & __ptr64,int * __ptr64>",                                                                                                                                                            m_any, "struct phmap::btree_set<int>::iterator",                                  cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::set_params<int, phmap::Less<int>, std::allocator<int>, 256, false>>, int&, int*>",                                                                                                                                                                                                      m_any, "phmap::btree_set<int>::iterator",                                         cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::set_params<int, phmap::Less<int>, std::allocator<int>, 256, false>>, int&, int*> ::blah const *",                                                                                                                                                                                       m_any, "phmap::btree_set<int>::iterator::blah const *",                           cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::set_params<int, phmap::Less<int>, std::allocator<int>, 256, false>> const, int const&, int const*>",                                                                                                                                                                                    m_any, "phmap::btree_set<int>::const_iterator",                                   cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::set_params<int, phmap::Less<int>, std::allocator<int>, 256, false>> const, int const&, int const*> ::blah const *",                                                                                                                                                                     m_any, "phmap::btree_set<int>::const_iterator::blah const *",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::map_params<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>, 256, false>>, std::pair<int const, float>&, std::pair<int const, float>*>",                                                                                                                       m_any, "phmap::btree_map<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators, TraitsPhmap{});
    CheckRoundtrip("struct phmap::priv::btree_iterator<class phmap::priv::btree_node<struct phmap::priv::map_params<int,float,struct phmap::Less<int>,class std::allocator<struct std::pair<int const ,float> >,256,0> >,struct std::pair<int const ,float> & __ptr64,struct std::pair<int const ,float> * __ptr64>",                                                        m_any, "struct phmap::btree_map<int, float>::iterator",                           cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::map_params<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>, 256, false>>, std::pair<int const, float>&, std::pair<int const, float>*>",                                                                                                                       m_any, "phmap::btree_map<int, float>::iterator",                                  cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::map_params<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>, 256, false>>, std::pair<int const, float>&, std::pair<int const, float>*> ::blah const *",                                                                                                        m_any, "phmap::btree_map<int, float>::iterator::blah const *",                    cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::map_params<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>, 256, false>> const, std::pair<int const, float> const&, std::pair<int const, float> const*>",                                                                                                     m_any, "phmap::btree_map<int, float>::const_iterator",                            cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::map_params<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>, 256, false>> const, std::pair<int const, float> const&, std::pair<int const, float> const*> ::blah const *",                                                                                      m_any, "phmap::btree_map<int, float>::const_iterator::blah const *",              cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::set_params<int, phmap::Less<int>, std::allocator<int>, 256, true>>, int&, int*>",                                                                                                                                                                                                       m_any, "phmap::btree_multiset<int, phmap::Less<int>, std::allocator<int>>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators, TraitsPhmap{});
    CheckRoundtrip("struct phmap::priv::btree_iterator<class phmap::priv::btree_node<struct phmap::priv::set_params<int,struct phmap::Less<int>,class std::allocator<int>,256,1> >,int & __ptr64,int * __ptr64>",                                                                                                                                                            m_any, "struct phmap::btree_multiset<int>::iterator",                             cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::set_params<int, phmap::Less<int>, std::allocator<int>, 256, true>>, int&, int*>",                                                                                                                                                                                                       m_any, "phmap::btree_multiset<int>::iterator",                                    cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::set_params<int, phmap::Less<int>, std::allocator<int>, 256, true>>, int&, int*> ::blah const *",                                                                                                                                                                                        m_any, "phmap::btree_multiset<int>::iterator::blah const *",                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::set_params<int, phmap::Less<int>, std::allocator<int>, 256, true>> const, int const&, int const*>",                                                                                                                                                                                     m_any, "phmap::btree_multiset<int>::const_iterator",                              cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::set_params<int, phmap::Less<int>, std::allocator<int>, 256, true>> const, int const&, int const*> ::blah const *",                                                                                                                                                                      m_any, "phmap::btree_multiset<int>::const_iterator::blah const *",                cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::map_params<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>, 256, true>>, std::pair<int const, float>&, std::pair<int const, float>*>",                                                                                                                        m_any, "phmap::btree_multimap<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators, TraitsPhmap{});
    CheckRoundtrip("struct phmap::priv::btree_iterator<class phmap::priv::btree_node<struct phmap::priv::map_params<int,float,struct phmap::Less<int>,class std::allocator<struct std::pair<int const ,float> >,256,1> >,struct std::pair<int const ,float> & __ptr64,struct std::pair<int const ,float> * __ptr64>",                                                        m_any, "struct phmap::btree_multimap<int, float>::iterator",                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::map_params<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>, 256, true>>, std::pair<int const, float>&, std::pair<int const, float>*>",                                                                                                                        m_any, "phmap::btree_multimap<int, float>::iterator",                             cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::map_params<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>, 256, true>>, std::pair<int const, float>&, std::pair<int const, float>*> ::blah const *",                                                                                                         m_any, "phmap::btree_multimap<int, float>::iterator::blah const *",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::map_params<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>, 256, true>> const, std::pair<int const, float> const&, std::pair<int const, float> const*>",                                                                                                      m_any, "phmap::btree_multimap<int, float>::const_iterator",                       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::btree_iterator<phmap::priv::btree_node<phmap::priv::map_params<int, float, phmap::Less<int>, std::allocator<std::pair<int const, float>>, 256, true>> const, std::pair<int const, float> const&, std::pair<int const, float> const*> ::blah const *",                                                                                       m_any, "phmap::btree_multimap<int, float>::const_iterator::blah const *",         cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::priv::raw_hash_set<struct phmap::priv::FlatHashSetPolicy<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > >,struct phmap::Hash<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > >,struct phmap::EqualTo<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > >,class std::allocator<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > > >::iterator", m_any, "phmap::flat_hash_set<std::string>::iterator", cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::all, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashSetPolicy<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>, phmap::priv::StringHashEqT<char>::Hash, phmap::priv::StringHashEqT<char>::Eq, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>>::iterator",                      m_any, "phmap::flat_hash_set<std::string>::iterator",                             cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::all, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashSetPolicy<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>, phmap::priv::StringHashEqT<char>::Hash, phmap::priv::StringHashEqT<char>::Eq, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>>::iterator ::blah const *",       m_any, "phmap::flat_hash_set<std::string>::iterator::blah const *",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::all, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashSetPolicy<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>, phmap::priv::StringHashEqT<char>::Hash, phmap::priv::StringHashEqT<char>::Eq, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>>::const_iterator",                m_any, "phmap::flat_hash_set<std::string>::const_iterator",                       cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::all, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashSetPolicy<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>, phmap::priv::StringHashEqT<char>::Hash, phmap::priv::StringHashEqT<char>::Eq, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>>>::const_iterator ::blah const *", m_any, "phmap::flat_hash_set<std::string>::const_iterator::blah const *",         cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::all, TraitsPhmap{});
    CheckRoundtrip("class phmap::priv::raw_hash_set<struct phmap::priv::FlatHashSetPolicy<int * __ptr64>,struct phmap::priv::HashEq<int * __ptr64,void>::Hash,struct phmap::priv::HashEq<int * __ptr64,void>::Eq,class std::allocator<int * __ptr64> >::iterator",                                                                                                           m_any, "class phmap::flat_hash_set<int *__ptr64>::iterator",                      cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("class phmap::priv::raw_hash_set<struct phmap::priv::FlatHashSetPolicy<int * __ptr64>,struct phmap::priv::HashEq<int * __ptr64,void>::Hash,struct phmap::priv::HashEq<int * __ptr64,void>::Eq,class std::allocator<int * __ptr64> >::iterator",                                                                                                           m_any, "phmap::flat_hash_set<int *>::iterator",                                   cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::all, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashSetPolicy<int*>, phmap::priv::HashEq<int*, void>::Hash, phmap::priv::HashEq<int*, void>::Eq, std::allocator<int*>>::iterator",                                                                                                                                                                            m_any, "phmap::flat_hash_set<int *>::iterator",                                   cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashSetPolicy<int*>, phmap::priv::HashEq<int*, void>::Hash, phmap::priv::HashEq<int*, void>::Eq, std::allocator<int*>>::iterator ::blah const *",                                                                                                                                                             m_any, "phmap::flat_hash_set<int *>::iterator::blah const *",                     cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashSetPolicy<int*>, phmap::priv::HashEq<int*, void>::Hash, phmap::priv::HashEq<int*, void>::Eq, std::allocator<int*>>::const_iterator",                                                                                                                                                                      m_any, "phmap::flat_hash_set<int *>::const_iterator",                             cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});
    CheckRoundtrip("phmap::priv::raw_hash_set<phmap::priv::FlatHashSetPolicy<int*>, phmap::priv::HashEq<int*, void>::Hash, phmap::priv::HashEq<int*, void>::Eq, std::allocator<int*>>::const_iterator ::blah const *",                                                                                                                                                       m_any, "phmap::flat_hash_set<int *>::const_iterator::blah const *",               cppdecl::ToCodeFlags::east_const, cppdecl::SimplifyFlags::bit_common_normalize_iterators | cppdecl::SimplifyFlags::bit_common_remove_defargs_other, TraitsPhmap{});


    // Selective `ToCode()` stuff.
    CheckTypeRoundtrip("int (*&)[42]", "int (*)[42]", {}, {}, 1);
    CheckTypeRoundtrip("int (*&)[42]", "int[42]", {}, {}, 2);
    CheckTypeRoundtrip("const int", "int", {}, {}, 0, cppdecl::CvQualifiers::const_);
    CheckTypeRoundtrip("const int *const *const", "const int *const *", {}, {}, 0, cppdecl::CvQualifiers::const_);
    CheckTypeRoundtrip("const int *const *const", "const int *", {}, {}, 1, cppdecl::CvQualifiers::const_);
    CheckTypeRoundtrip("const int *const *const", "int", {}, {}, 2, cppdecl::CvQualifiers::const_);


    // ### Parsing numeric literals:

    CheckNumericLiteralFail("", 0, "<no literal to parse>"); // This isn't an actual error, but the string reported by the testing helper when the parser returns empty.
    CheckNumericLiteralFail("0x", 2, "Expected at least one digit after the numeric literal prefix.");
    CheckNumericLiteralFail("0b", 2, "Expected at least one digit after the numeric literal prefix.");
    CheckNumericLiteral("0", "0", "0", "0", "int{base=8,value=``,suffix=none}", "integer 0");
    CheckNumericLiteral("123", "123", "123", "123", "int{base=10,value=`123`,suffix=none}", "integer 123");
    CheckNumericLiteral("0123", "0123", "83", "83", "int{base=8,value=`123`,suffix=none}", "octal integer 123 (decimal 83)");
    CheckNumericLiteral("0b1101", "0b1101", "13", "13", "int{base=2,value=`1101`,suffix=none}", "binary integer 1101 (decimal 13)");
    CheckNumericLiteral("0x12a", "0x12a", "298", "298", "int{base=16,value=`12a`,suffix=none}", "hexadecimal integer 12a (decimal 298)");
    CheckNumericLiteral("0x12A", "0x12A", "298", "298", "int{base=16,value=`12A`,suffix=none}", "hexadecimal integer 12A (decimal 298)");
    CheckNumericLiteral("0X12a", "0x12a", "298", "298", "int{base=16,value=`12a`,suffix=none}", "hexadecimal integer 12a (decimal 298)");
    CheckNumericLiteral("0X12A", "0x12A", "298", "298", "int{base=16,value=`12A`,suffix=none}", "hexadecimal integer 12A (decimal 298)");
    CheckNumericLiteral("0xffffffffffffffff", "0xffffffffffffffff", "18446744073709551615", "18446744073709551615", "int{base=16,value=`ffffffffffffffff`,suffix=none}", "hexadecimal integer ffffffffffffffff (decimal 18446744073709551615)");
    // This number is so large that it can't converted to a `std::uint64_t`.
    CheckNumericLiteral("0x10000000000000000", "0x10000000000000000", "", "0x10000000000000000", "int{base=16,value=`10000000000000000`,suffix=none}", "hexadecimal integer 10000000000000000");
    // Bad digits:
    CheckNumericLiteral("123!", "123", "123", "123", "int{base=10,value=`123`,suffix=none}", "integer 123", 1);
    CheckNumericLiteralFail("01238", 4, "Non-octal digit in an octal integer literal.");
    CheckNumericLiteral("0123!", "0123", "83", "83", "int{base=8,value=`123`,suffix=none}", "octal integer 123 (decimal 83)", 1);
    CheckNumericLiteral("0x123!", "0x123", "291", "291", "int{base=16,value=`123`,suffix=none}", "hexadecimal integer 123 (decimal 291)", 1);
    CheckNumericLiteralFail("0b11012", 6, "Non-binary digit in a binary integer literal.");
    // User-defined suffixes:
    CheckNumericLiteral("123a", "123a", "123", "123a", "int{base=10,value=`123`,suffix=udl`a`}", "integer 123 with user-defined suffix `a`");
    CheckNumericLiteral("123a4", "123a4", "123", "123a4", "int{base=10,value=`123`,suffix=udl`a4`}", "integer 123 with user-defined suffix `a4`");
    CheckNumericLiteral("0b1101a", "0b1101a", "13", "13a", "int{base=2,value=`1101`,suffix=udl`a`}", "binary integer 1101 (decimal 13) with user-defined suffix `a`");
    // --- Stock suffixes:
    CheckNumericLiteral("123l", "123l", "123", "123", "int{base=10,value=`123`,suffix=l}", "integer 123 with preferred type `long`");
    CheckNumericLiteral("123L", "123l", "123", "123", "int{base=10,value=`123`,suffix=l}", "integer 123 with preferred type `long`");
    CheckNumericLiteral("123ll", "123ll", "123", "123", "int{base=10,value=`123`,suffix=ll}", "integer 123 with preferred type `long long`");
    CheckNumericLiteral("123LL", "123ll", "123", "123", "int{base=10,value=`123`,suffix=ll}", "integer 123 with preferred type `long long`");
    CheckNumericLiteral("123z", "123z", "123", "123", "int{base=10,value=`123`,suffix=z}", "integer 123 with preferred type `ptrdiff_t`");
    CheckNumericLiteral("123Z", "123z", "123", "123", "int{base=10,value=`123`,suffix=z}", "integer 123 with preferred type `ptrdiff_t`");
    CheckNumericLiteral("123lL", "123lL", "123", "123lL", "int{base=10,value=`123`,suffix=udl`lL`}", "integer 123 with user-defined suffix `lL`"); // Mixed `lL` is a UDL.
    CheckNumericLiteral("123Ll", "123Ll", "123", "123Ll", "int{base=10,value=`123`,suffix=udl`Ll`}", "integer 123 with user-defined suffix `Ll`"); // Same.
    // Now the unsigned ones:
    CheckNumericLiteral("123u", "123u", "123", "123", "int{base=10,value=`123`,suffix=u}", "integer 123 with preferred type `unsigned int`");
    CheckNumericLiteral("123ul", "123ul", "123", "123", "int{base=10,value=`123`,suffix=u+l}", "integer 123 with preferred type `unsigned long`");
    CheckNumericLiteral("123uL", "123ul", "123", "123", "int{base=10,value=`123`,suffix=u+l}", "integer 123 with preferred type `unsigned long`");
    CheckNumericLiteral("123ull", "123ull", "123", "123", "int{base=10,value=`123`,suffix=u+ll}", "integer 123 with preferred type `unsigned long long`");
    CheckNumericLiteral("123uLL", "123ull", "123", "123", "int{base=10,value=`123`,suffix=u+ll}", "integer 123 with preferred type `unsigned long long`");
    CheckNumericLiteral("123uz", "123uz", "123", "123", "int{base=10,value=`123`,suffix=u+z}", "integer 123 with preferred type `size_t`");
    CheckNumericLiteral("123uZ", "123uz", "123", "123", "int{base=10,value=`123`,suffix=u+z}", "integer 123 with preferred type `size_t`");
    // Unsigned with capital `U`:
    CheckNumericLiteral("123U", "123u", "123", "123", "int{base=10,value=`123`,suffix=u}", "integer 123 with preferred type `unsigned int`");
    CheckNumericLiteral("123Ul", "123ul", "123", "123", "int{base=10,value=`123`,suffix=u+l}", "integer 123 with preferred type `unsigned long`");
    CheckNumericLiteral("123UL", "123ul", "123", "123", "int{base=10,value=`123`,suffix=u+l}", "integer 123 with preferred type `unsigned long`");
    CheckNumericLiteral("123Ull", "123ull", "123", "123", "int{base=10,value=`123`,suffix=u+ll}", "integer 123 with preferred type `unsigned long long`");
    CheckNumericLiteral("123ULL", "123ull", "123", "123", "int{base=10,value=`123`,suffix=u+ll}", "integer 123 with preferred type `unsigned long long`");
    CheckNumericLiteral("123Uz", "123uz", "123", "123", "int{base=10,value=`123`,suffix=u+z}", "integer 123 with preferred type `size_t`");
    CheckNumericLiteral("123UZ", "123uz", "123", "123", "int{base=10,value=`123`,suffix=u+z}", "integer 123 with preferred type `size_t`");
    // Unsigned with trailing `u`:
    CheckNumericLiteral("123u", "123u", "123", "123", "int{base=10,value=`123`,suffix=u}", "integer 123 with preferred type `unsigned int`");
    CheckNumericLiteral("123lu", "123ul", "123", "123", "int{base=10,value=`123`,suffix=u+l}", "integer 123 with preferred type `unsigned long`");
    CheckNumericLiteral("123Lu", "123ul", "123", "123", "int{base=10,value=`123`,suffix=u+l}", "integer 123 with preferred type `unsigned long`");
    CheckNumericLiteral("123llu", "123ull", "123", "123", "int{base=10,value=`123`,suffix=u+ll}", "integer 123 with preferred type `unsigned long long`");
    CheckNumericLiteral("123LLu", "123ull", "123", "123", "int{base=10,value=`123`,suffix=u+ll}", "integer 123 with preferred type `unsigned long long`");
    CheckNumericLiteral("123zu", "123uz", "123", "123", "int{base=10,value=`123`,suffix=u+z}", "integer 123 with preferred type `size_t`");
    CheckNumericLiteral("123Zu", "123uz", "123", "123", "int{base=10,value=`123`,suffix=u+z}", "integer 123 with preferred type `size_t`");
    // Unsigned with trailing capital `U`:
    CheckNumericLiteral("123U", "123u", "123", "123", "int{base=10,value=`123`,suffix=u}", "integer 123 with preferred type `unsigned int`");
    CheckNumericLiteral("123lU", "123ul", "123", "123", "int{base=10,value=`123`,suffix=u+l}", "integer 123 with preferred type `unsigned long`");
    CheckNumericLiteral("123LU", "123ul", "123", "123", "int{base=10,value=`123`,suffix=u+l}", "integer 123 with preferred type `unsigned long`");
    CheckNumericLiteral("123llU", "123ull", "123", "123", "int{base=10,value=`123`,suffix=u+ll}", "integer 123 with preferred type `unsigned long long`");
    CheckNumericLiteral("123LLU", "123ull", "123", "123", "int{base=10,value=`123`,suffix=u+ll}", "integer 123 with preferred type `unsigned long long`");
    CheckNumericLiteral("123zU", "123uz", "123", "123", "int{base=10,value=`123`,suffix=u+z}", "integer 123 with preferred type `size_t`");
    CheckNumericLiteral("123ZU", "123uz", "123", "123", "int{base=10,value=`123`,suffix=u+z}", "integer 123 with preferred type `size_t`");

    // Digit separators in integers.
    CheckNumericLiteralFail("'123", 0, "<no literal to parse>"); // Same error as for empty input. This isn't an actual error, but the string reported by the testing helper when the parser returns empty.
    CheckNumericLiteral("1'23", "1'23", "123", "123", "int{base=10,value=`1'23`,suffix=none}", "integer 123");
    CheckNumericLiteral("0x12'a", "0x12'a", "298", "298", "int{base=16,value=`12'a`,suffix=none}", "hexadecimal integer 12a (decimal 298)");
    CheckNumericLiteral("123'", "123", "123", "123", "int{base=10,value=`123`,suffix=none}", "integer 123", 1); // Trailing apostrophe is rejected.
    CheckNumericLiteral("123'+", "123", "123", "123", "int{base=10,value=`123`,suffix=none}", "integer 123", 2); // Same.
    CheckNumericLiteral("0'123", "0'123", "83", "83", "int{base=8,value=`'123`,suffix=none}", "octal integer 123 (decimal 83)"); // Apostrophe immediately after the leading zero is fine.
    CheckNumericLiteralFail("0b'1101", 2, "Expected at least one digit after the numeric literal prefix."); // Not fine in binary and hex though.
    CheckNumericLiteralFail("0x'12a", 2, "Expected at least one digit after the numeric literal prefix.");
    CheckNumericLiteral("0b1101'2", "0b1101", "13", "13", "int{base=2,value=`1101`,suffix=none}", "binary integer 1101 (decimal 13)", 2); // Wrong digit.
    CheckNumericLiteral("0b1101", "0b1101", "13", "13", "int{base=2,value=`1101`,suffix=none}", "binary integer 1101 (decimal 13)");

    // Floating-point numbers:
    CheckNumericLiteral("12.34", "12.34", "", "12point34", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=none}", "floating-point number 12.34");
    CheckNumericLiteral("12.", "12.", "", "12point0", "float{base=10,int=`12`,frac=``,exp=``,suffix=none}", "floating-point number 12");
    CheckNumericLiteral(".34", ".34", "", "0point34", "float{base=10,int=``,frac=`34`,exp=``,suffix=none}", "floating-point number 0.34");
    CheckNumericLiteralFail(".", 0, "<no literal to parse>"); // Same error as for empty input. This isn't an actual error, but the string reported by the testing helper when the parser returns empty.
    CheckNumericLiteral("12.34e56", "12.34e56", "", "12point34e56", "float{base=10,int=`12`,frac=`34`,exp=`56`,suffix=none}", "floating-point number 12.34 * 10^56");
    CheckNumericLiteral("12.34e+56", "12.34e+56", "", "12point34e56", "float{base=10,int=`12`,frac=`34`,exp=`+56`,suffix=none}", "floating-point number 12.34 * 10^+56");
    CheckNumericLiteral("12.34e-56", "12.34e-56", "", "12point34eminus56", "float{base=10,int=`12`,frac=`34`,exp=`-56`,suffix=none}", "floating-point number 12.34 * 10^-56");
    CheckNumericLiteral("12e56", "12e56", "", "12e56", "float{base=10,int=`12`,frac=none,exp=`56`,suffix=none}", "floating-point number 12 * 10^56");
    // Missing stuff after the exponent.
    CheckNumericLiteralFail("12e", 3, "Expected the exponent value.");
    CheckNumericLiteralFail("12e+", 4, "Expected the exponent value.");
    CheckNumericLiteralFail("12e-", 4, "Expected the exponent value.");
    CheckNumericLiteralFail("12e++", 4, "Expected the exponent value.");
    CheckNumericLiteralFail("12e--", 4, "Expected the exponent value.");
    // Apostrophes:
    CheckNumericLiteral("1'2.3'4e5'6", "1'2.3'4e5'6", "", "12point34e56", "float{base=10,int=`1'2`,frac=`3'4`,exp=`5'6`,suffix=none}", "floating-point number 12.34 * 10^56");

    // Encoding fun:
    // Seemingly "octal" floating-point literal, but actually decimal.
    CheckNumericLiteral("02.34", "02.34", "", "02point34", "float{base=10,int=`02`,frac=`34`,exp=``,suffix=none}", "floating-point number 02.34");
    // Can have digits 8,9 in those too.
    CheckNumericLiteral("08.34", "08.34", "", "08point34", "float{base=10,int=`08`,frac=`34`,exp=``,suffix=none}", "floating-point number 08.34");
    CheckNumericLiteral("09.34", "09.34", "", "09point34", "float{base=10,int=`09`,frac=`34`,exp=``,suffix=none}", "floating-point number 09.34");

    // Stray hex digits:
    CheckNumericLiteral("1a.34", "1a", "1", "1a", "int{base=10,value=`1`,suffix=udl`a`}", "integer 1 with user-defined suffix `a`", 3);
    CheckNumericLiteral("12.a4", "12.a4", "", "12point0a4", "float{base=10,int=`12`,frac=``,exp=``,suffix=udl`a4`}", "floating-point number 12 with user-defined suffix `a4`");
    // Suffixes in different scenarios:
    // If there is no fractional part and no exponent, the floating-point suffixes don't resolve:
    CheckNumericLiteral("1f32", "1f32", "1", "1f32", "int{base=10,value=`1`,suffix=udl`f32`}", "integer 1 with user-defined suffix `f32`");
    CheckNumericLiteral("12.34f32", "12.34f32", "", "12point34f32", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=f32}", "floating-point number 12.34 of type `std::float32_t`");
    CheckNumericLiteral("12e56f32", "12e56f32", "", "12e56f32", "float{base=10,int=`12`,frac=none,exp=`56`,suffix=f32}", "floating-point number 12 * 10^56 of type `std::float32_t`");
    CheckNumericLiteral("12.34e56f32", "12.34e56f32", "", "12point34e56f32", "float{base=10,int=`12`,frac=`34`,exp=`56`,suffix=f32}", "floating-point number 12.34 * 10^56 of type `std::float32_t`");
    // All the suffixes:
    CheckNumericLiteral("12.34f", "12.34f", "", "12point34f", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=f}", "floating-point number 12.34 of type `float`");
    CheckNumericLiteral("12.34l", "12.34l", "", "12point34l", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=l}", "floating-point number 12.34 of type `long double`");
    CheckNumericLiteral("12.34f16", "12.34f16", "", "12point34f16", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=f16}", "floating-point number 12.34 of type `std::float16_t`");
    CheckNumericLiteral("12.34f32", "12.34f32", "", "12point34f32", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=f32}", "floating-point number 12.34 of type `std::float32_t`");
    CheckNumericLiteral("12.34f64", "12.34f64", "", "12point34f64", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=f64}", "floating-point number 12.34 of type `std::float64_t`");
    CheckNumericLiteral("12.34f128", "12.34f128", "", "12point34f128", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=f128}", "floating-point number 12.34 of type `std::float128_t`");
    CheckNumericLiteral("12.34bf16", "12.34bf16", "", "12point34bf16", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=bf16}", "floating-point number 12.34 of type `std::bfloat16_t`");
    // All the suffixes in caps:
    CheckNumericLiteral("12.34F", "12.34f", "", "12point34f", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=f}", "floating-point number 12.34 of type `float`");
    CheckNumericLiteral("12.34L", "12.34l", "", "12point34l", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=l}", "floating-point number 12.34 of type `long double`");
    CheckNumericLiteral("12.34F16", "12.34f16", "", "12point34f16", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=f16}", "floating-point number 12.34 of type `std::float16_t`");
    CheckNumericLiteral("12.34F32", "12.34f32", "", "12point34f32", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=f32}", "floating-point number 12.34 of type `std::float32_t`");
    CheckNumericLiteral("12.34F64", "12.34f64", "", "12point34f64", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=f64}", "floating-point number 12.34 of type `std::float64_t`");
    CheckNumericLiteral("12.34F128", "12.34f128", "", "12point34f128", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=f128}", "floating-point number 12.34 of type `std::float128_t`");
    CheckNumericLiteral("12.34BF16", "12.34bf16", "", "12point34bf16", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=bf16}", "floating-point number 12.34 of type `std::bfloat16_t`");
    // Mixed-case suffixes don't resolve:
    CheckNumericLiteral("12.34bF16", "12.34bF16", "", "12point34bF16", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=udl`bF16`}", "floating-point number 12.34 with user-defined suffix `bF16`");
    CheckNumericLiteral("12.34bF16", "12.34bF16", "", "12point34bF16", "float{base=10,int=`12`,frac=`34`,exp=``,suffix=udl`bF16`}", "floating-point number 12.34 with user-defined suffix `bF16`");

    // Hex floats.
    // The `p` exponent doesn't resolve in non-hex floats:
    CheckNumericLiteral("12.p4", "12.p4", "", "12point0p4", "float{base=10,int=`12`,frac=``,exp=``,suffix=udl`p4`}", "floating-point number 12 with user-defined suffix `p4`");
    CheckNumericLiteralFail("0x12.34", 7, "Hexadecimal floating-point literals require a `p...` exponent.");
    CheckNumericLiteral("0x1a.b2p34", "0x1a.b2p34", "", "0x1ApointB2p34", "float{base=16,int=`1a`,frac=`b2`,exp=`34`,suffix=none}", "hexadecimal floating-point number 0x1a.0xb2 * 2^34");
    CheckNumericLiteral("0X1A.B2P34", "0x1A.B2p34", "", "0x1ApointB2p34", "float{base=16,int=`1A`,frac=`B2`,exp=`34`,suffix=none}", "hexadecimal floating-point number 0x1A.0xB2 * 2^34");
    CheckNumericLiteral("0x1ap34", "0x1ap34", "", "0x1Ap34", "float{base=16,int=`1a`,frac=none,exp=`34`,suffix=none}", "hexadecimal floating-point number 0x1a * 2^34");
    CheckNumericLiteral("0X1AP34", "0x1Ap34", "", "0x1Ap34", "float{base=16,int=`1A`,frac=none,exp=`34`,suffix=none}", "hexadecimal floating-point number 0x1A * 2^34");
    CheckNumericLiteral("0x1ap+34", "0x1ap+34", "", "0x1Ap34", "float{base=16,int=`1a`,frac=none,exp=`+34`,suffix=none}", "hexadecimal floating-point number 0x1a * 2^+34");
    CheckNumericLiteral("0x1ap-34", "0x1ap-34", "", "0x1Apminus34", "float{base=16,int=`1a`,frac=none,exp=`-34`,suffix=none}", "hexadecimal floating-point number 0x1a * 2^-34");
    // All the suffixes:
    CheckNumericLiteral("0x1p2f", "0x1p2f", "", "0x1p2f", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=f}", "hexadecimal floating-point number 0x1 * 2^2 of type `float`");
    CheckNumericLiteral("0x1p2l", "0x1p2l", "", "0x1p2l", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=l}", "hexadecimal floating-point number 0x1 * 2^2 of type `long double`");
    CheckNumericLiteral("0x1p2f16", "0x1p2f16", "", "0x1p2f16", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=f16}", "hexadecimal floating-point number 0x1 * 2^2 of type `std::float16_t`");
    CheckNumericLiteral("0x1p2f32", "0x1p2f32", "", "0x1p2f32", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=f32}", "hexadecimal floating-point number 0x1 * 2^2 of type `std::float32_t`");
    CheckNumericLiteral("0x1p2f64", "0x1p2f64", "", "0x1p2f64", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=f64}", "hexadecimal floating-point number 0x1 * 2^2 of type `std::float64_t`");
    CheckNumericLiteral("0x1p2f128", "0x1p2f128", "", "0x1p2f128", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=f128}", "hexadecimal floating-point number 0x1 * 2^2 of type `std::float128_t`");
    CheckNumericLiteral("0x1p2bf16", "0x1p2bf16", "", "0x1p2bf16", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=bf16}", "hexadecimal floating-point number 0x1 * 2^2 of type `std::bfloat16_t`");
    // All the suffixes in caps:
    CheckNumericLiteral("0x1p2F", "0x1p2f", "", "0x1p2f", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=f}", "hexadecimal floating-point number 0x1 * 2^2 of type `float`");
    CheckNumericLiteral("0x1p2L", "0x1p2l", "", "0x1p2l", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=l}", "hexadecimal floating-point number 0x1 * 2^2 of type `long double`");
    CheckNumericLiteral("0x1p2F16", "0x1p2f16", "", "0x1p2f16", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=f16}", "hexadecimal floating-point number 0x1 * 2^2 of type `std::float16_t`");
    CheckNumericLiteral("0x1p2F32", "0x1p2f32", "", "0x1p2f32", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=f32}", "hexadecimal floating-point number 0x1 * 2^2 of type `std::float32_t`");
    CheckNumericLiteral("0x1p2F64", "0x1p2f64", "", "0x1p2f64", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=f64}", "hexadecimal floating-point number 0x1 * 2^2 of type `std::float64_t`");
    CheckNumericLiteral("0x1p2F128", "0x1p2f128", "", "0x1p2f128", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=f128}", "hexadecimal floating-point number 0x1 * 2^2 of type `std::float128_t`");
    CheckNumericLiteral("0x1p2BF16", "0x1p2bf16", "", "0x1p2bf16", "float{base=16,int=`1`,frac=none,exp=`2`,suffix=bf16}", "hexadecimal floating-point number 0x1 * 2^2 of type `std::bfloat16_t`");

    CheckTypeRoundtrip("A<12'34, 1'2.3'4e5'6>", "A<12'34, 1'2.3'4e5'6>");
    CheckTypeRoundtrip("A<12'34, 1'2.3'4e5'6>", "A<1234, 12.34e56>", cppdecl::ToCodeFlags::numeric_literals_strip_apostrophes);

    CheckTypeRoundtrip("A<12.34, 12., .34, 12.0, 0.34, 12.00, 00.34>", "A<12.34, 12., .34, 12.0, 0.34, 12.00, 00.34>");
    CheckTypeRoundtrip("A<12.34, 12., .34, 12.0, 0.34, 12.00, 00.34>", "A<12.34, 12., 0.34, 12.0, 0.34, 12.00, 00.34>", cppdecl::ToCodeFlags::numeric_literals_force_zero_before_point);
    CheckTypeRoundtrip("A<12.34, 12., .34, 12.0, 0.34, 12.00, 00.34>", "A<12.34, 12., .34, 12.0, .34, 12.00, .34>", cppdecl::ToCodeFlags::numeric_literals_no_zero_before_point);

    CheckTypeRoundtrip("A<12.34, 12., .34, 12.0, 0.34, 12.00, 00.34, 12.34e56, 12.e56, .34e56, 12.0e56, 0.34e56, 12.00e56, 00.34e56>", "A<12.34, 12., .34, 12.0, 0.34, 12.00, 00.34, 12.34e56, 12.e56, .34e56, 12.0e56, 0.34e56, 12.00e56, 00.34e56>");
    CheckTypeRoundtrip("A<12.34, 12., .34, 12.0, 0.34, 12.00, 00.34, 12.34e56, 12.e56, .34e56, 12.0e56, 0.34e56, 12.00e56, 00.34e56>", "A<12.34, 12.0, .34, 12.0, 0.34, 12.00, 00.34, 12.34e56, 12.0e56, .34e56, 12.0e56, 0.34e56, 12.00e56, 00.34e56>", cppdecl::ToCodeFlags::numeric_literals_force_zero_after_point);
    CheckTypeRoundtrip("A<12.34, 12., .34, 12.0, 0.34, 12.00, 00.34, 12.34e56, 12.e56, .34e56, 12.0e56, 0.34e56, 12.00e56, 00.34e56>", "A<12.34, 12., .34, 12., 0.34, 12., 00.34, 12.34e56, 12.e56, .34e56, 12.e56, 0.34e56, 12.e56, 00.34e56>", cppdecl::ToCodeFlags::numeric_literals_no_zero_after_point);
    CheckTypeRoundtrip("A<12.34, 12., .34, 12.0, 0.34, 12.00, 00.34, 12.34e56, 12.e56, .34e56, 12.0e56, 0.34e56, 12.00e56, 00.34e56>", "A<12.34, 12, .34, 12, 0.34, 12, 00.34, 12.34e56, 12e56, .34e56, 12e56, 0.34e56, 12e56, 00.34e56>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac);
    CheckTypeRoundtrip("A<12.34, 12., .34, 12.0, 0.34, 12.00, 00.34, 12.34e56, 12.e56, .34e56, 12.0e56, 0.34e56, 12.00e56, 00.34e56>", "A<12.34, 12., .34, 12.0, 0.34, 12.00, 00.34, 12.34e56, 12e56, .34e56, 12e56, 0.34e56, 12e56, 00.34e56>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac_before_exponent);
    CheckTypeRoundtrip("A<12.34, 12., .34, 12.0, 0.34, 12.00, 00.34, 12.34e56, 12.e56, .34e56, 12.0e56, 0.34e56, 12.00e56, 00.34e56>", "A<12.34, 12.0, .34, 12.0, 0.34, 12.00, 00.34, 12.34e56, 12e56, .34e56, 12e56, 0.34e56, 12e56, 00.34e56>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac_before_exponent | cppdecl::ToCodeFlags::numeric_literals_force_zero_after_point);
    CheckTypeRoundtrip("A<12.34, 12., .34, 12.0, 0.34, 12.00, 00.34, 12.34e56, 12.e56, .34e56, 12.0e56, 0.34e56, 12.00e56, 00.34e56>", "A<12.34, 12., .34, 12., 0.34, 12., 00.34, 12.34e56, 12e56, .34e56, 12e56, 0.34e56, 12e56, 00.34e56>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac_before_exponent | cppdecl::ToCodeFlags::numeric_literals_no_zero_after_point);

    CheckTypeRoundtrip("A<12e1, 12e+1, 12e-1, 12e+0, 12e+00, 12e-0, 12e-00, 12e0, 12e00, 12e+0, 12e+00, 12e-0, 12e-00, 12.34e1, 12.34e+1, 12.34e-1, 12.34e+0, 12.34e+00, 12.34e-0, 12.34e-00, 12.34e0, 12.34e00, 12.34e+0, 12.34e+00, 12.34e-0, 12.34e-00>", "A<12e1, 12e+1, 12e-1, 12e+0, 12e+00, 12e-0, 12e-00, 12e0, 12e00, 12e+0, 12e+00, 12e-0, 12e-00, 12.34e1, 12.34e+1, 12.34e-1, 12.34e+0, 12.34e+00, 12.34e-0, 12.34e-00, 12.34e0, 12.34e00, 12.34e+0, 12.34e+00, 12.34e-0, 12.34e-00>");
    CheckTypeRoundtrip("A<12e1, 12e+1, 12e-1, 12e+0, 12e+00, 12e-0, 12e-00, 12e0, 12e00, 12e+0, 12e+00, 12e-0, 12e-00, 12.34e1, 12.34e+1, 12.34e-1, 12.34e+0, 12.34e+00, 12.34e-0, 12.34e-00, 12.34e0, 12.34e00, 12.34e+0, 12.34e+00, 12.34e-0, 12.34e-00>", "A<12e1, 12e+1, 12e-1, 12e+0, 12e+00, 12e-0, 12e-00, 12e0, 12e00, 12e+0, 12e+00, 12e-0, 12e-00, 12.34e1, 12.34e+1, 12.34e-1, 12.34, 12.34, 12.34, 12.34, 12.34, 12.34, 12.34, 12.34, 12.34, 12.34>", cppdecl::ToCodeFlags::numeric_literals_no_zero_exponent_after_frac);
    CheckTypeRoundtrip("A<12e1, 12e+1, 12e-1, 12e+0, 12e+00, 12e-0, 12e-00, 12e0, 12e00, 12e+0, 12e+00, 12e-0, 12e-00, 12.34e1, 12.34e+1, 12.34e-1, 12.34e+0, 12.34e+00, 12.34e-0, 12.34e-00, 12.34e0, 12.34e00, 12.34e+0, 12.34e+00, 12.34e-0, 12.34e-00>", "A<12e1, 12e+1, 12e-1, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12.34e1, 12.34e+1, 12.34e-1, 12.34, 12.34, 12.34, 12.34, 12.34, 12.34, 12.34, 12.34, 12.34, 12.34>", cppdecl::ToCodeFlags::numeric_literals_no_zero_exponent);

    CheckTypeRoundtrip("A<12e1, 12e+1, 12e-1, 12e+0, 12e+00, 12e-0, 12e-00, 12e0, 12e00, 12e+0, 12e+00, 12e-0, 12e-00, 12.34e1, 12.34e+1, 12.34e-1, 12.34e+0, 12.34e+00, 12.34e-0, 12.34e-00, 12.34e0, 12.34e00, 12.34e+0, 12.34e+00, 12.34e-0, 12.34e-00>", "A<12e1, 12e1, 12e-1, 12e0, 12e00, 12e0, 12e00, 12e0, 12e00, 12e0, 12e00, 12e0, 12e00, 12.34e1, 12.34e1, 12.34e-1, 12.34e0, 12.34e00, 12.34e0, 12.34e00, 12.34e0, 12.34e00, 12.34e0, 12.34e00, 12.34e0, 12.34e00>", cppdecl::ToCodeFlags::numeric_literals_no_exponent_useless_sign);
    CheckTypeRoundtrip("A<12e1, 12e+1, 12e-1, 12e+0, 12e+00, 12e-0, 12e-00, 12e0, 12e00, 12e+0, 12e+00, 12e-0, 12e-00, 12.34e1, 12.34e+1, 12.34e-1, 12.34e+0, 12.34e+00, 12.34e-0, 12.34e-00, 12.34e0, 12.34e00, 12.34e+0, 12.34e+00, 12.34e-0, 12.34e-00>", "A<12e+1, 12e+1, 12e-1, 12e+0, 12e+00, 12e+0, 12e+00, 12e+0, 12e+00, 12e+0, 12e+00, 12e+0, 12e+00, 12.34e+1, 12.34e+1, 12.34e-1, 12.34e+0, 12.34e+00, 12.34e+0, 12.34e+00, 12.34e+0, 12.34e+00, 12.34e+0, 12.34e+00, 12.34e+0, 12.34e+00>", cppdecl::ToCodeFlags::numeric_literals_force_exponent_plus_sign);

    // `numeric_literals_no_zero_exponent_after_frac` beats `numeric_literals_no_zero_frac_before_exponent`, but loses to `numeric_literals_no_zero_frac`.
    CheckTypeRoundtrip("A<12.0e0, 12.0e+0, 12.0e-0>", "A<12.0, 12.0, 12.0>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac_before_exponent | cppdecl::ToCodeFlags::numeric_literals_no_zero_exponent_after_frac);
    CheckTypeRoundtrip("A<12.0e0, 12.0e+0, 12.0e-0>", "A<12e0, 12e+0, 12e-0>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac | cppdecl::ToCodeFlags::numeric_literals_no_zero_exponent_after_frac);
    CheckTypeRoundtrip("A<12.0e0, 12.0e+0, 12.0e-0>", "A<12.0, 12.0, 12.0>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac_before_exponent | cppdecl::ToCodeFlags::numeric_literals_no_zero_exponent);

    // Zero exponents in hex floats.
    CheckTypeRoundtrip("A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1.p3, 0x1.p+3, 0x1.p-3, 0x1.p0, 0x1.p+0, 0x1.p-0, 0x1.p00, 0x1.p+00, 0x1.p-00, 0x1.0p3, 0x1.0p+3, 0x1.0p-3, 0x1.0p0, 0x1.0p+0, 0x1.0p-0, 0x1.0p00, 0x1.0p+00, 0x1.0p-00>", "A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1.p3, 0x1.p+3, 0x1.p-3, 0x1.p0, 0x1.p+0, 0x1.p-0, 0x1.p00, 0x1.p+00, 0x1.p-00, 0x1.0p3, 0x1.0p+3, 0x1.0p-3, 0x1.0p0, 0x1.0p+0, 0x1.0p-0, 0x1.0p00, 0x1.0p+00, 0x1.0p-00>");
    CheckTypeRoundtrip("A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1.p3, 0x1.p+3, 0x1.p-3, 0x1.p0, 0x1.p+0, 0x1.p-0, 0x1.p00, 0x1.p+00, 0x1.p-00, 0x1.0p3, 0x1.0p+3, 0x1.0p-3, 0x1.0p0, 0x1.0p+0, 0x1.0p-0, 0x1.0p00, 0x1.0p+00, 0x1.0p-00>", "A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1.p3, 0x1.p+3, 0x1.p-3, 0x1.p0, 0x1.p+0, 0x1.p-0, 0x1.p00, 0x1.p+00, 0x1.p-00, 0x1.0p3, 0x1.0p+3, 0x1.0p-3, 0x1.0p0, 0x1.0p+0, 0x1.0p-0, 0x1.0p00, 0x1.0p+00, 0x1.0p-00>", cppdecl::ToCodeFlags::numeric_literals_no_zero_exponent_after_frac);
    CheckTypeRoundtrip("A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1.p3, 0x1.p+3, 0x1.p-3, 0x1.p0, 0x1.p+0, 0x1.p-0, 0x1.p00, 0x1.p+00, 0x1.p-00, 0x1.0p3, 0x1.0p+3, 0x1.0p-3, 0x1.0p0, 0x1.0p+0, 0x1.0p-0, 0x1.0p00, 0x1.0p+00, 0x1.0p-00>", "A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1.p3, 0x1.p+3, 0x1.p-3, 0x1.p0, 0x1.p+0, 0x1.p-0, 0x1.p00, 0x1.p+00, 0x1.p-00, 0x1.0p3, 0x1.0p+3, 0x1.0p-3, 0x1.0p0, 0x1.0p+0, 0x1.0p-0, 0x1.0p00, 0x1.0p+00, 0x1.0p-00>", cppdecl::ToCodeFlags::numeric_literals_no_zero_exponent);
    CheckTypeRoundtrip("A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1.p3, 0x1.p+3, 0x1.p-3, 0x1.p0, 0x1.p+0, 0x1.p-0, 0x1.p00, 0x1.p+00, 0x1.p-00, 0x1.0p3, 0x1.0p+3, 0x1.0p-3, 0x1.0p0, 0x1.0p+0, 0x1.0p-0, 0x1.0p00, 0x1.0p+00, 0x1.0p-00>", "A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac_before_exponent);
    CheckTypeRoundtrip("A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1.p3, 0x1.p+3, 0x1.p-3, 0x1.p0, 0x1.p+0, 0x1.p-0, 0x1.p00, 0x1.p+00, 0x1.p-00, 0x1.0p3, 0x1.0p+3, 0x1.0p-3, 0x1.0p0, 0x1.0p+0, 0x1.0p-0, 0x1.0p00, 0x1.0p+00, 0x1.0p-00>", "A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac_before_exponent | cppdecl::ToCodeFlags::numeric_literals_no_zero_exponent_after_frac);
    CheckTypeRoundtrip("A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1.p3, 0x1.p+3, 0x1.p-3, 0x1.p0, 0x1.p+0, 0x1.p-0, 0x1.p00, 0x1.p+00, 0x1.p-00, 0x1.0p3, 0x1.0p+3, 0x1.0p-3, 0x1.0p0, 0x1.0p+0, 0x1.0p-0, 0x1.0p00, 0x1.0p+00, 0x1.0p-00>", "A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac_before_exponent | cppdecl::ToCodeFlags::numeric_literals_no_zero_exponent);
    CheckTypeRoundtrip("A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1.p3, 0x1.p+3, 0x1.p-3, 0x1.p0, 0x1.p+0, 0x1.p-0, 0x1.p00, 0x1.p+00, 0x1.p-00, 0x1.0p3, 0x1.0p+3, 0x1.0p-3, 0x1.0p0, 0x1.0p+0, 0x1.0p-0, 0x1.0p00, 0x1.0p+00, 0x1.0p-00>", "A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac);
    CheckTypeRoundtrip("A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1.p3, 0x1.p+3, 0x1.p-3, 0x1.p0, 0x1.p+0, 0x1.p-0, 0x1.p00, 0x1.p+00, 0x1.p-00, 0x1.0p3, 0x1.0p+3, 0x1.0p-3, 0x1.0p0, 0x1.0p+0, 0x1.0p-0, 0x1.0p00, 0x1.0p+00, 0x1.0p-00>", "A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac | cppdecl::ToCodeFlags::numeric_literals_no_zero_exponent_after_frac);
    CheckTypeRoundtrip("A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1.p3, 0x1.p+3, 0x1.p-3, 0x1.p0, 0x1.p+0, 0x1.p-0, 0x1.p00, 0x1.p+00, 0x1.p-00, 0x1.0p3, 0x1.0p+3, 0x1.0p-3, 0x1.0p0, 0x1.0p+0, 0x1.0p-0, 0x1.0p00, 0x1.0p+00, 0x1.0p-00>", "A<0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00, 0x1p3, 0x1p+3, 0x1p-3, 0x1p0, 0x1p+0, 0x1p-0, 0x1p00, 0x1p+00, 0x1p-00>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac | cppdecl::ToCodeFlags::numeric_literals_no_zero_exponent);

    // Ampersands in "no_zero_..." flags.
    CheckTypeRoundtrip("A<12.0'0e0'0>", "A<12>", cppdecl::ToCodeFlags::numeric_literals_no_zero_frac | cppdecl::ToCodeFlags::numeric_literals_no_zero_exponent);

    // Check that simplification handles the numbers correclty.
    CheckTypeRoundtrip("A<4'2, 1'2.e0'0>", "A<42, 12.0>", {}, cppdecl::SimplifyFlags::bit_common_normalize_numbers);




    // Compile-time stuff.

    // Same as the `static_assert` below, but at runtime.
    CheckParseSuccess("std::array<int, 42> &(*func)(int x, int y)", m_any, "`func`, a pointer to a function taking 2 parameters: [1. `x` of type `int`, 2. `y` of type `int`], returning an lvalue reference to `std`::`array` with 2 template arguments: [1. possibly type: `int`, 2. non-type: [integer 42]]", {});

    // Those tests choke on some configurations. I've disabled some known broken configurations, but more conditions might need to be added here.
    #if CPPDECL_IS_CONSTEXPR
    static_assert(
        ParseDeclToString("std::array<int, 42> &(*func)(int x, int y)", m_any, 0, cppdecl::ToStringFlags{})
        ==
        "`func`, a pointer to a function taking 2 parameters: [1. `x` of type `int`, 2. `y` of type `int`], returning an lvalue reference to `std`::`array` with 2 template arguments: [1. possibly type: `int`, 2. non-type: [integer 42]]"
    );

    static_assert(cppdecl::TypeName<std::unordered_map<int, float>::iterator, cppdecl::TypeNameFlags::no_simplify>() != "std::unordered_map<int, float>::iterator");

    static_assert(cppdecl::TypeName<int>() == "int");
    static_assert(cppdecl::TypeName<int, cppdecl::TypeNameFlags::no_simplify>() == "int");

    static_assert(cppdecl::TypeName<const int, {}, cppdecl::ToCodeFlags::east_const>() == "int const");
    static_assert(cppdecl::TypeName<const int, cppdecl::TypeNameFlags::no_process, cppdecl::ToCodeFlags::east_const>() == "const int");
    #endif

    CheckActualEqualsExpected("", cppdecl::TypeName<std::unordered_map<int, float>::iterator, cppdecl::TypeNameFlags::use_typeid>(), "std::unordered_map<int, float>::iterator");
    CheckActualEqualsExpected("", cppdecl::TypeName<int, cppdecl::TypeNameFlags::use_typeid>(), "int");
    CheckActualEqualsExpected("", cppdecl::TypeName<int, cppdecl::TypeNameFlags::use_typeid | cppdecl::TypeNameFlags::no_process>(), "int");
    #ifdef _MSC_VER
    CheckActualEqualsExpected("", cppdecl::TypeName<int, cppdecl::TypeNameFlags::use_typeid | cppdecl::TypeNameFlags::no_demangle>(), "int");
    #else
    CheckActualEqualsExpected("", cppdecl::TypeName<int, cppdecl::TypeNameFlags::use_typeid | cppdecl::TypeNameFlags::no_demangle>(), "i");
    #endif
}
