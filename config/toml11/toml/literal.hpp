//     Copyright Toru Niina 2019.
// Distributed under the MIT License.
#ifndef TOML11_LITERAL_HPP
#define TOML11_LITERAL_HPP
#include "parser.hpp"

namespace toml
{
inline namespace literals
{
inline namespace toml_literals
{

// implementation
inline ::toml::value literal_internal_impl(::toml::detail::location loc)
{
    // if there are some comments or empty lines, skip them.
    using skip_line = ::toml::detail::repeat<toml::detail::sequence<
            ::toml::detail::maybe<::toml::detail::lex_ws>,
            ::toml::detail::maybe<::toml::detail::lex_comment>,
            ::toml::detail::lex_newline
        >, ::toml::detail::at_least<1>>;
    skip_line::invoke(loc);

    // if there are some whitespaces before a value, skip them.
    using skip_ws = ::toml::detail::repeat<
        ::toml::detail::lex_ws, ::toml::detail::at_least<1>>;
    skip_ws::invoke(loc);

    // to distinguish arrays and tables, first check it is a table or not.
    //
    // "[1,2,3]"_toml;   // this is an array
    // "[table]"_toml;   // a table that has an empty table named "table" inside.
    // "[[1,2,3]]"_toml; // this is an array of arrays
    // "[[table]]"_toml; // this is a table that has an array of tables inside.
    //
    // "[[1]]"_toml;     // this can be both... (currently it becomes a table)
    // "1 = [{}]"_toml;  // this is a table that has an array of table named 1.
    // "[[1,]]"_toml;    // this is an array of arrays.
    // "[[1],]"_toml;    // this also.

    const auto the_front = loc.iter();

    const bool is_table_key = ::toml::detail::lex_std_table::invoke(loc);
    loc.reset(the_front);

    const bool is_aots_key  = ::toml::detail::lex_array_table::invoke(loc);
    loc.reset(the_front);

    // If it is neither a table-key or a array-of-table-key, it may be a value.
    if(!is_table_key && !is_aots_key)
    {
        if(auto data = ::toml::detail::parse_value<::toml::value>(loc))
        {
            return data.unwrap();
        }
    }

    // Note that still it can be a table, because the literal might be something
    // like the following.
    // ```cpp
    // R"( // c++11 raw string literals
    //   key = "value"
    //   int = 42
    // )"_toml;
    // ```
    // It is a valid toml file.
    // It should be parsed as if we parse a file with this content.

    if(auto data = ::toml::detail::parse_toml_file<::toml::value>(loc))
    {
        return data.unwrap();
    }
    else // none of them.
    {
        throw ::toml::syntax_error(data.unwrap_err(), source_location(loc));
    }

}

inline ::toml::value operator"" _toml(const char* str, std::size_t len)
{
    ::toml::detail::location loc(
            std::string("TOML literal encoded in a C++ code"),
            std::vector<char>(str, str + len));
    return literal_internal_impl(std::move(loc));
}

// value of __cplusplus in C++2a/20 mode is not fixed yet along compilers.
// So here we use the feature test macro for `char8_t` itself.
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
// value of u8"" literal has been changed from char to char8_t and char8_t is
// NOT compatible to char
inline ::toml::value operator"" _toml(const char8_t* str, std::size_t len)
{
    ::toml::detail::location loc(
            std::string("TOML literal encoded in a C++ code"),
            std::vector<char>(reinterpret_cast<const char*>(str),
                              reinterpret_cast<const char*>(str) + len));
    return literal_internal_impl(std::move(loc));
}
#endif

} // toml_literals
} // literals
} // toml
#endif//TOML11_LITERAL_HPP
