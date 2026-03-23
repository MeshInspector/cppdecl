# cppdecl

All-in-one library to parse and manipulate C/C++ type names and declarations.

Includes a type name parser that gives sensible results without context information, and a bunch of [normalization rules](#normalizing--simplifying-types) to produce consistent type names across compilers.

Some usecases:

* [**Getting type names as strings**](#getting-type-names), either at runtime (from `typeid`) or at compile-time, optionally normalized to be more consistent across platforms (e.g. `"std::__cxx11::basic_string<char, ...>"` → `"std::string"`).

  Formatting knobs are included (optional east const, `T *x` vs `T* x`, etc).

* [**Explaining type names**](#explaining-type-names--declarations), like [cdecl.org](https://cdecl.org/) but more capable. (For one, we are not limited to built-in type names, and [can handle ambiguities](#how-do-you-handle-ambiguities) that can arise from not knowing what is or isn't a type.)

* [Constructing type names / declarations](#how-do-i-construct-a-type-object-from-scratch) (such as for code generation), or [manipulating](#manipulating-types) the strings you got from elsewhere (such as from libclang, without using the libclang machinery itself).

  This includes adding/removing/checking cv-qualifiers, type components (`*`/`&`/`[N]`/etc), and so on.

[Examples](#some-examples) | [Installation](#installation) | [FAQ](#faq)

## Some examples

[Getting type name strings](#getting-type-names) | [Explaining complex types](#explaining-type-names--declarations) | [Manipulating types](#manipulating-types) | [Normalization](#normalizing--simplifying-types)

### Getting type names

```cpp
#include <cppdecl/type_name.h>

int main()
{
    std::cout << cppdecl::TypeName<std::string>() << '\n'; // std::string

    // Works at compile-time too:
    static_assert(cppdecl::TypeName<std::string>() == "std::string");

    // And at runtime with `std::type_index`:
    std::cout << cppdecl::TypeNameDynamic(typeid(std::string)) << '\n'; // std::string
}
```
Compare this with the naive approaches:

* `typeid(std::string).name()` produces mangled names: `"NSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE"`, or on MSVC just ugly names: `"class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >"`.

* `std::source_location::current().function_name()` varies wildly across compilers. It can return anything from `"std::string"` (only on Clang+libc++) to `"std::basic_string<char>"` to `"class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >"` at worst. (Same happens to `__PRETTY_FUNCTION__`/`__FUNCSIG__`-based approaches.)

Cppdecl uses the same two methods of obtaining the type names internally, but additionally normalizes the result to produce (in this case) `"std::string"` on all platforms.

Among other clever things, we try to make iterator types human-readable: `std::vector<int>::iterator` expands to `__gnu_cxx::__normal_iterator<int *, std::vector<int>>` (on libstdc++, different strings on different standard libraries), and we rewrite it back to `"std::vector<int>::iterator"`. While this [somewhat](#what-are-the-limitations-of-type-normalizatonsimplification-across-compilers) works on all 3 big standard libraries, iterator types can cause divergence between platforms.

### Explaining type names / declarations

Cppdecl can convert types to human-readable strings, for example:

`char (*(*x())[5])()` → `` `x`, a function taking no parameters, returning a pointer to an array of size [integer 5] of a pointer to a function taking no parameters, returning `char` ``.

A small command-line utility is included for this purpose (called `cppdecl`, needs to be built from source). Or you can do this programmatically:

```cpp
#include <cppdecl/declarations/parse_simple.h>
#include <cppdecl/declarations/to_string.h>

#include <iostream>

int main()
{
    auto type = cppdecl::ParseDecl_Simple("char (*(*x())[5])()");
    std::cout << cppdecl::ToString(type, {}) << '\n';
}
```

Cppdecl can handle arbitrary names without knowing if they are types or something else. Sometimes this causes ambiguities, which we detect, and guess the intended interpretation:

`x(y)` → `` ambiguous, either [unnamed function taking 1 parameter: [unnamed of type `y`], returning `x`] or [`y` of type `x`] or [`x`, a constructor taking 1 parameter: [unnamed of type `y`]]` ``

The alternatives are sorted to put what we think is the intended interpretation first.

### Manipulating types

```cpp
#include <cppdecl/declarations/data.h>
#include <cppdecl/declarations/to_string.h>
#include <cppdecl/declarations/parse_simple.h>

#include <iostream>

int main()
{
    auto decl = cppdecl::ParseDecl_Simple("const char*x");
    std::cout << cppdecl::ToCode(decl, {}) << '\n'; // `const char *x`, notice the automatic formatting.
    std::cout << cppdecl::ToCode(decl, cppdecl::ToCodeFlags::east_const) << '\n'; // `char const *x`
    std::cout << cppdecl::ToCode(decl, cppdecl::ToCodeFlags::left_align_pointer) << '\n'; // `const char* x`

    decl.type.AddQualifiers(cppdecl::CvQualifiers::const_);
    std::cout << cppdecl::ToCode(decl, {}) << '\n'; // `const char *const x`

    decl.type.RemoveQualifiers(cppdecl::CvQualifiers::const_, 1); // Remove constness from the pointee.
    std::cout << cppdecl::ToCode(decl, {}) << '\n'; // `char *const x`

    decl.type.RemoveModifier();
    std::cout << cppdecl::ToCode(decl, {}) << '\n'; // `char x`

    decl.type.AddModifier(cppdecl::Reference{});
    std::cout << cppdecl::ToCode(decl, {}) << '\n'; // `char &x`
}
```

### Normalizing / simplifying types

As mentioned above, cppdecl includes a lot of type normalization rules. Most of them are applied automatically when calling `cppdecl::TypeName()`, but it can be useful to run them manually:

```cpp
#include <cppdecl/declarations/data.h>
#include <cppdecl/declarations/parse_simple.h>
#include <cppdecl/declarations/simplify.h>
#include <cppdecl/declarations/to_string.h>

#include <iostream>

int main()
{
    auto type = cppdecl::ParseType_Simple("class std::vector<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::allocator<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > > >");

    // This prints the above string mostly unmodified.
    std::cout << cppdecl::ToCode(type, {}) << '\n';

    cppdecl::Simplify(cppdecl::SimplifyFlags::all, type);

    // This prints `std::vector<std::string>`.
    std::cout << cppdecl::ToCode(type, {}) << '\n';
}
```

By default, `Simplify()` only handles standard types, but it is modular and support for third-party types can be added as well.

To enable optional support for some known third-party types, pass `cppdecl::FullSimplifyTraits{}` as the third argument to `Simplify()` (`TypeName()` accepts that as well). Currently, the only library we have normalization rules for is [phmap](https://github.com/greg7mdp/parallel-hashmap).

## Installation

Cppdecl is header-only, and should work on more-or-less recent versions of Clang, GCC, and MSVC.

There are some examples and tests that can be compiled with Meson:
```sh
meson setup build
meson compile -C build
```
Amont other things, those include `cppdecl` (an utility explains type names in plain English).

## How to get reasonable compile times

Cppdecl is header-only, which allows it to be `constexpr` on recent compilers.

But this comes with downsides:

* Headers are large and can slow down compilation.

* `constexpr` computations are slow.

In particular, the default behavior of `cppdecl::TypeName<T>()` is do everything at compile-time if possible, so that only the final string appears in the binary. This is great for release builds, but takes about 200ms of compilation time per type.

This can be remedied by using `cppdecl::TypeName<T, cppdecl::TypeNameFlags::no_constexpr>()`. This causes an ugly type name to be saved at compile-time, and then simplified at runtime. Typically you only want this flag in debug builds, so you might want to wrap `TypeName()` in your own function that applies the flag only in debug builds.

Or even better, in debug builds avoid including cppdecl in your headers at all, hide it in a .cpp file, and create a wrapper for `cppdecl::TypeNameDynamic(std::type_index)`, so the rest of your program can get type names without including cppdecl.

If you still want to include cppdecl in your headers, I recommend putting it into a PCH.

## FAQ

### How do you parse a string?

There are simple parsing functions in `<cppdecl/declarations/parse_simple.h>`, that throw on failure. They also throw if some part of the string was left unparsed.

* `cppdecl::ParseQualifiedName_Simple()` - for names like `std::vector<int>`.
* `cppdecl::ParseType_Simple()` - same, and also supports types, e.g. `const std::vector<int> *`.
* `cppdecl::ParseDecl_Simple()` - same, and also supports declarations, e.g. `std::vector<int> x`.

If you don't want exceptions, use the lower-level functions from `<cppdecl/declarations/parse.h>`. Those don't error if some part of the string was left unparsed, allowing you to chain the calls. This header also has functions to parse less common entities (such as lone template argument lists).

There are also classes that call the `..._Simple()` functions and memoize the results: `cppdecl::QualifiedNameParser`, `cppdecl::TypeParser`, `cppdecl::DeclParser`.

### How do you convert a type/etc back to a string?

Most types in the library (`cppdecl::Type`, `Decl`, `QualifiedName`, etc) support following functions:

* `cppdecl::ToCode(x, {})` - convert to C++ code
* `cppdecl::ToString(x, {})` - convert to human-readable description
* `cppdecl::ToString(x, cppdecl::ToString::identifier)` - convert to a valid identifier (basically mangling, but lossy and human-readable)
* `cppdecl::ToString(x, cppdecl::ToString::debug)` - dump the debug representation

### What you can or can't parse?

We can parse type names (that use any C/C++ features I could think of).

We can also parse declarations of single entities, i.e. a type plus a name (which includes names in the middle of types, as in `int *a[42]`).

We can't parse following, either because I'm not particularly interested in those or haven't bothered yet:

* Declarations of more than one entity at a time. Cppdecl is primarily intended to parse *types*, supporting declarations at all is a courtesy.

* Initializers in declarations. Those will be reported as unparsed junk at the end of input. I probably could handle them as token soup.

* Most specifiers in declarations, like `mutable`/`virtual`/`etc`. Again, because I'm primarily interested in parsing *types*.

* `template <...>` in declarations. Same goes for `requires`-clauses.

* Anything involving `...` pack expansions. Again, because cppdecl is primarily intended to parse concrete types as reported by the compiler/libclang.

* See [known issues](/known_issues.md) for more.

### How do you distinguish type vs non-type template arguments?

We don't. Everything that successfully parses as a type gets treated as a type, and everything else is parsed as a token soup (called "pseudo-expression" in cppdecl, which includes some minimal conveniences, like understanding qualified names, or grouping the contents of braces/parentheses/etc).

### How do you parse `A<x < y>`?

Currently we don't. `<` is always assumed to begin a template argument. Currently even `A<(x < y)>` doesn't get parsed.

I don't see it as a big problem, because this library is intended primarily to deal with the types produced by a compiler (or libclang), where template arguments will already be evaluated.

### How do you handle ambiguities?

#### In types:

Turns out there aren't many ambiguities when parsing types, as opposed to declarations.

The only source of ambiguity in types (that I'm aware of) is `x(y)` in function parameters, e.g. `int(x(y))`. This can either be a function taking another function (as an unnamed parameter), or `int(x y)` with redundant parentheses:
```cpp
auto type = cppdecl::ParseType_Simple("int(x(y))");
std::cout << cppdecl::ToString(type, {}) << '\n';
```
This prints: `` a function taking 1 parameter: [ambiguous, either [unnamed function taking 1 parameter: [unnamed of type `y`], returning `x`] or [`y` of type `x`]], returning `int` ``.

In the code, this is handled by representing each function parameter as `cppdecl::MaybeAmbiguousDecl` instead of `cppdecl::Decl`, so each parameter stores the list of alternatives for itself. Notice that it being a function is listed first, as it's a more reasonable interpretation than the parentheses being redundant.

If you don't do anything special, each function parameter will appear as its first alternative (e.g. this is what `cppdecl::ToCode()` does, it ignores the alternative results from ambiguous parsing).

Note also that sometimes we can resolve this ambiguity automatically if we know what `y` is. If it's a known type (e.g. `int(x(int))`), we can parse this unambiguously.

#### In declarations:

In declarations we have the same ambiguity as above, plus more.

First of all, it can now happen at the top level: `x(y)` is either an unnamed function taking `y` and returning `x`, or a variable `x y`. Or (!!) a constructor of class `x` taking `y`.
```cpp
auto type = cppdecl::ParseDecl_Simple("x(y)");
std::cout << cppdecl::ToString(type, {}) << '\n';
```
And indeed this prints: `` ambiguous, either [unnamed function taking 1 parameter: [unnamed of type `y`], returning `x`] or [`y` of type `x`] or [`x`, a constructor taking 1 parameter: [unnamed of type `y`]] ``.

This specific case isn't a problem in practice, because `ParseDecl[_Simple]` has flags to filter the result (by having or not having a name, or for constructors having or not having a return type). For example, this produces no ambiguities:
```cpp
auto type = cppdecl::ParseDecl_Simple("x(y)", cppdecl::ParseDeclFlags::accept_all_named | cppdecl::ParseDeclFlags::force_non_empty_return_type);
std::cout << cppdecl::ToString(type, {}) << '\n'; // `y` of type `x`
```

### How do you handle most vexing parse?

We don't, because we [don't parse initializers](#what-you-can-or-cant-parse).

### How do you handle `T *x` without knowing if `T` is a type or not?

There are separate parsing functions for types and expressions, so we don't need to guess. (And our expression representation is a basically a [glorified token soup](#how-do-you-distinguish-type-vs-non-type-template-arguments) anyway.)

### How do I construct a type object from scratch?

```cpp
// `int` (`void` and all other single-word types go here, `long long` also goes here)
cppdecl::Type a = cppdecl::Type::FromSingleWord("int"); // If no `::` nor other punctuation.
// `std::int32_t` (anything with `::` goes here)
cppdecl::Type b = cppdecl::Type::FromQualifiedName(cppdecl::QualifiedName{}.AddPart("std").AddPart("int32_t"));
// `std::vector<int>` (here's how you handle template arguments)
cppdecl::Type c = cppdecl::Type::FromQualifiedName(cppdecl::QualifiedName{}.AddPart("std").AddPart("vector").AddTemplateArgument(cppdecl::Type::FromSingleWord("int")));
// `int *`
cppdecl::Type d = cppdecl::Type::FromSingleWord("int").AddModifier(cppdecl::Pointer{});
```

### Can you use cppdecl to post-process compiler diagnostics?

Would be nice to simplify types in compiler output, right?

If you [compile](#installation) the examples, there's one called `cppdecl_prettify_errors`. You can pipe compiler output through it, and it'll attempt to prettify anything that looks like a type.

But turns out that this doesn't work well, because compilers already perform some partial simplification of their own, which confuses cppdecl. The only compiler that doesn't do this seems to be MSVC, which is ironically helpful in our case.

### What are the limitations of type normalizaton/simplification across compilers?

Different compilers report slightly different names for different types, and cppdecl tries to normalize them to a common format. It isn't always possible though.

This normalization happens either implicitly in `cppdecl::TypeName()` (unless disabled), or explicitly by calling `cppdecl::Simplify()`.

Here are some cases where we know we can't achieve compiler-independent results:

* Iterators:

  * Some standard libraries use pointers instead of iterator classes for some containers. In particular, MSVC STL uses classes for `std::array` iterators, but libstdc++ and libc++ don't. So printing the type of `std::array<T, N>::iterator` will give either `"T *"` or the actual `"std::array<T, N>::iterator"`.

  * Some standard libraries reuse iterators between containers. All 3 big standard libraries seem to reuse `...set` iterators for the respective `...multiset` containers. Some standard libraries use the same `iterator` and `const_iterator` in some or all `set`s. MSVC STL reuses `std::list` iterators for unordered containers.

    The end result is that rewriting types into `std::...::[const_]iterator` form is best effort, and can give inaccurate results on some platforms.
