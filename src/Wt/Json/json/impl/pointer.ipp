//
// Copyright (c) 2022 Dmitry Arkhipov (grisumbras@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_IMPL_POINTER_IPP
#define BOOST_JSON_IMPL_POINTER_IPP

#include "../value.hpp"

BOOST_JSON_NS_BEGIN

namespace detail {

class pointer_token
{
public:
    class iterator;

    pointer_token(
        char const* b,
        char const* e) noexcept
        : b_(b)
        , e_(e)
    {
    }

    iterator begin() const noexcept;
    iterator end() const noexcept;

private:
    char const* b_;
    char const* e_;
};

class pointer_token::iterator
{
public:
    explicit iterator(char const* base) noexcept
        : base_(base)
    {
    }

    char operator*() const noexcept
    {
        switch( char c = *base_ )
        {
        case '~':
            c = base_[1];
            if( '0' == c )
                return '~';
            BOOST_ASSERT('1' == c);
            return '/';
        default:
            return c;
        }
    }

    iterator& operator++() noexcept
    {
        if( '~' == *base_ )
            base_ += 2;
        else
            ++base_;
        return *this;
    }

    char const* base() const noexcept
    {
        return base_;
    }

private:
    char const* base_;
};

bool operator==(pointer_token::iterator l, pointer_token::iterator r) noexcept
{
    return l.base() == r.base();
}

bool operator!=(pointer_token::iterator l, pointer_token::iterator r) noexcept
{
    return l.base() != r.base();
}

pointer_token::iterator pointer_token::begin() const noexcept
{
    return iterator(b_);
}

pointer_token::iterator pointer_token::end() const noexcept
{
    return iterator(e_);
}

bool operator==(pointer_token token, string_view sv) noexcept
{
    auto t_b = token.begin();
    auto const t_e = token.end();
    auto s_b = sv.begin();
    auto const s_e = sv.end();
    while( s_b != s_e )
    {
        if( t_e == t_b )
            return false;
        if( *t_b != *s_b )
            return false;
        ++t_b;
        ++s_b;
    }
    return t_b == t_e;
}

bool is_invalid_zero(
    char const* b,
    char const* e) noexcept
{
    // in JSON Pointer only zero index can start character '0'
    if( *b != '0' )
        return false;

    // if an index token starts with '0', then it should not have any more
    // characters: either the string should end, or new token should start
    ++b;
    if( b == e )
        return false;
    return *b != '/';
}

bool is_past_the_end_token(
    char const* b,
    char const* e) noexcept
{
    if( *b != '-' )
        return false;

    ++b;
    if( b == e )
        return true;
    return *b == '/';
}

std::size_t
parse_number_token(
    char const*& b,
    char const* e,
    error_code& ec) noexcept
{
    if( ( b == e )
        || is_invalid_zero(b, e) )
    {
        constexpr source_location loc = BOOST_JSON_SOURCE_POS;
        BOOST_JSON_ASSIGN_ERROR_CODE(ec, (int)error::token_not_number, &loc);
        return {};
    }

    if( is_past_the_end_token(b, e) )
    {
        constexpr source_location loc = BOOST_JSON_SOURCE_POS;
        BOOST_JSON_ASSIGN_ERROR_CODE(ec, (int)error::past_the_end, &loc);
        return {};
    }

    std::size_t result = 0;
    for( ; b != e; ++b )
    {
        char const c = *b;

        if( '/' == c)
            break;

        unsigned d = c - '0';
        if( d > 9 )
        {
            constexpr source_location loc = BOOST_JSON_SOURCE_POS;
            BOOST_JSON_ASSIGN_ERROR_CODE(ec, (int)error::token_not_number, &loc);
            return {};
        }

        std::size_t new_result = result * 10 + d;
        if( new_result < result )
        {
            constexpr source_location loc = BOOST_JSON_SOURCE_POS;
            BOOST_JSON_ASSIGN_ERROR_CODE(ec, (int)error::token_overflow, &loc);
            return {};
        }

        result = new_result;

    }
    return result;
}

pointer_token
get_token(
    char const* b,
    char const* e,
    error_code& ec) noexcept
{
    char const* start = b;
    for( ; b < e; ++b )
    {
        char const c = *b;
        if( '/' == c )
            break;

        if( '~' == c )
        {
            if( ++b == e )
            {
                constexpr source_location loc
                    = BOOST_JSON_SOURCE_POS;
                BOOST_JSON_ASSIGN_ERROR_CODE(ec, (int)error::invalid_escape, &loc);
                break;
            }

            switch (*b)
            {
            case '0': // fall through
            case '1':
                // valid escape sequence
                continue;
            default: {
                constexpr source_location loc
                    = BOOST_JSON_SOURCE_POS;
                BOOST_JSON_ASSIGN_ERROR_CODE(ec, (int)error::invalid_escape, &loc);
                break;
            }
            }
            break;
        }
    }

    return pointer_token(start, b);
}

Value*
if_contains_token(Object const& obj, pointer_token token)
{
    if( obj.empty() )
        return nullptr;

    auto const it = detail::find_in_object(obj, token).first;
    if( !it )
        return nullptr;

    return &it->value();
}

} // namespace detail

Value const&
Value::at_pointer(string_view ptr) const
{
    error_code ec;
    auto const found = find_pointer(ptr, ec);
    if( !found )
        detail::throw_system_error(ec, BOOST_JSON_SOURCE_POS);
    return *found;
}

Value&
Value::at_pointer(string_view ptr)
{
    Value const& self = *this;
    return const_cast<Value&>(self.at_pointer(ptr));
}

Value const*
Value::find_pointer(string_view ptr, error_code& ec) const noexcept
{
    ec.clear();

    Value const* result = this;
    char const* cur = ptr.data();
    char const* const end = cur + ptr.size();
    while( end != cur )
    {
        if( *cur++ != '/' )
        {
            constexpr source_location loc = BOOST_JSON_SOURCE_POS;
            BOOST_JSON_ASSIGN_ERROR_CODE(ec, (int)error::missing_slash, &loc);
            return nullptr;
        }

        if( auto const po = result->if_object() )
        {
            auto const token = detail::get_token(cur, end, ec);
            if( ec )
                return nullptr;

            result = detail::if_contains_token(*po, token);
            cur = token.end().base();
        }
        else if( auto const pa = result->if_array() )
        {
            auto const index = detail::parse_number_token(cur, end, ec);
            if( ec )
                return nullptr;

            result = pa->if_contains(index);
        }
        else
        {
            constexpr source_location loc = BOOST_JSON_SOURCE_POS;
            BOOST_JSON_ASSIGN_ERROR_CODE(ec, (int)error::value_is_scalar, &loc);
            return nullptr;
        }

        if( !result )
        {
            constexpr source_location loc = BOOST_JSON_SOURCE_POS;
            BOOST_JSON_ASSIGN_ERROR_CODE(ec, (int)error::not_found, &loc);
            return nullptr;
        }
    }

    BOOST_ASSERT(result);
    return result;
}

Value*
Value::find_pointer(string_view ptr, error_code& ec) noexcept
{
    Value const& self = *this;
    return const_cast<Value*>(self.find_pointer(ptr, ec));
}

// Value const*
// Value::find_pointer(string_view ptr, std::error_code& ec) const noexcept
// {
//     error_code jec;
//     Value const* result = find_pointer(ptr, jec);
//     ec = jec;
//     return result;
// }

// Value*
// Value::find_pointer(string_view ptr, std::error_code& ec) noexcept
// {
//     Value const& self = *this;
//     return const_cast<Value*>(self.find_pointer(ptr, ec));
// }

BOOST_JSON_NS_END

#endif // BOOST_JSON_IMPL_POINTER_IPP
