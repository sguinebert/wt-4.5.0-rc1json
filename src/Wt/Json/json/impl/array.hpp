//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_IMPL_Array_HPP
#define BOOST_JSON_IMPL_Array_HPP

#include "../value.hpp"
#include "../detail/except.hpp"
#include <algorithm>
#include <stdexcept>
#include <type_traits>

BOOST_JSON_NS_BEGIN

//----------------------------------------------------------

struct alignas(Value)
    Array::table
{
    std::uint32_t size = 0;
    std::uint32_t capacity = 0;

    constexpr table();

    Value&
    operator[](std::size_t pos) noexcept
    {
        return (reinterpret_cast<
            Value*>(this + 1))[pos];
    }

    BOOST_JSON_DECL
    static
    table*
    allocate(
        std::size_t capacity,
        storage_ptr const& sp);

    BOOST_JSON_DECL
    static
    void
    deallocate(
        table* p,
        storage_ptr const& sp);
};

//----------------------------------------------------------

class Array::revert_construct
{
    Array* arr_;

public:
    explicit
    revert_construct(
        Array& arr) noexcept
        : arr_(&arr)
    {
    }

    ~revert_construct()
    {
        if(! arr_)
            return;
        arr_->destroy();
    }

    void
    commit() noexcept
    {
        arr_ = nullptr;
    }
};

//----------------------------------------------------------

class Array::revert_insert
{
    Array* arr_;
    std::size_t const i_;
    std::size_t const n_;

public:
    Value* p;

    BOOST_JSON_DECL
    revert_insert(
        const_iterator pos,
        std::size_t n,
        Array& arr);

    BOOST_JSON_DECL
    ~revert_insert();

    Value*
    commit() noexcept
    {
        auto it =
            arr_->data() + i_;
        arr_ = nullptr;
        return it;
    }
};

//----------------------------------------------------------

void
Array::
relocate(
    Value* dest,
    Value* src,
    std::size_t n) noexcept
{
    if(n == 0)
        return;
    std::memmove(
        static_cast<void*>(dest),
        static_cast<void const*>(src),
        n * sizeof(Value));
}

//----------------------------------------------------------
//
// Construction
//
//----------------------------------------------------------

template<class InputIt, class>
Array::
Array(
    InputIt first, InputIt last,
    storage_ptr sp)
    : Array(
        first, last,
        std::move(sp),
        iter_cat<InputIt>{})
{
    BOOST_STATIC_ASSERT(
        std::is_constructible<Value,
            decltype(*first)>::value);
}

//----------------------------------------------------------
//
// Modifiers
//
//----------------------------------------------------------

template<class InputIt, class>
auto
Array::
insert(
    const_iterator pos,
    InputIt first, InputIt last) ->
        iterator
{
    BOOST_STATIC_ASSERT(
        std::is_constructible<Value,
            decltype(*first)>::value);
    return insert(pos, first, last,
        iter_cat<InputIt>{});
}

template<class Arg>
auto
Array::
emplace(
    const_iterator pos,
    Arg&& arg) ->
        iterator
{
    BOOST_ASSERT(
        pos >= begin() &&
        pos <= end());
    Value jv(
        std::forward<Arg>(arg),
        storage());
    return insert(pos, pilfer(jv));
}

template<class Arg>
Value&
Array::
emplace_back(Arg&& arg)
{
    Value jv(
        std::forward<Arg>(arg),
        storage());
    return push_back(pilfer(jv));
}

//----------------------------------------------------------
//
// Element access
//
//----------------------------------------------------------

Value&
Array::
at(std::size_t pos)
{
    if(pos >= t_->size)
        detail::throw_out_of_range(
            BOOST_JSON_SOURCE_POS);
    return (*t_)[pos];
}

Value const&
Array::
at(std::size_t pos) const
{
    if(pos >= t_->size)
        detail::throw_out_of_range(
            BOOST_JSON_SOURCE_POS);
    return (*t_)[pos];
}

Value&
Array::
operator[](std::size_t pos) noexcept
{
    BOOST_ASSERT(pos < t_->size);
    return (*t_)[pos];
}

Value const&
Array::
operator[](std::size_t pos) const noexcept
{
    BOOST_ASSERT(pos < t_->size);
    return (*t_)[pos];
}

Value&
Array::
front() noexcept
{
    BOOST_ASSERT(t_->size > 0);
    return (*t_)[0];
}

Value const&
Array::
front() const noexcept
{
    BOOST_ASSERT(t_->size > 0);
    return (*t_)[0];
}

Value&
Array::
back() noexcept
{
    BOOST_ASSERT(
        t_->size > 0);
    return (*t_)[t_->size - 1];
}

Value const&
Array::
back() const noexcept
{
    BOOST_ASSERT(
        t_->size > 0);
    return (*t_)[t_->size - 1];
}

Value*
Array::
data() noexcept
{
    return &(*t_)[0];
}

Value const*
Array::
data() const noexcept
{
    return &(*t_)[0];
}

Value const*
Array::
if_contains(
    std::size_t pos) const noexcept
{
    if( pos < t_->size )
        return &(*t_)[pos];
    return nullptr;
}

Value*
Array::
if_contains(
    std::size_t pos) noexcept
{
    if( pos < t_->size )
        return &(*t_)[pos];
    return nullptr;
}

//----------------------------------------------------------
//
// Iterators
//
//----------------------------------------------------------

auto
Array::
begin() noexcept ->
    iterator
{
    return &(*t_)[0];
}

auto
Array::
begin() const noexcept ->
    const_iterator
{
    return &(*t_)[0];
}

auto
Array::
cbegin() const noexcept ->
    const_iterator
{
    return &(*t_)[0];
}

auto
Array::
end() noexcept ->
    iterator
{
    return &(*t_)[t_->size];
}

auto
Array::
end() const noexcept ->
    const_iterator
{
    return &(*t_)[t_->size];
}

auto
Array::
cend() const noexcept ->
    const_iterator
{
    return &(*t_)[t_->size];
}

auto
Array::
rbegin() noexcept ->
    reverse_iterator
{
    return reverse_iterator(end());
}

auto
Array::
rbegin() const noexcept ->
    const_reverse_iterator
{
    return const_reverse_iterator(end());
}

auto
Array::
crbegin() const noexcept ->
    const_reverse_iterator
{
    return const_reverse_iterator(end());
}

auto
Array::
rend() noexcept ->
    reverse_iterator
{
    return reverse_iterator(begin());
}

auto
Array::
rend() const noexcept ->
    const_reverse_iterator
{
    return const_reverse_iterator(begin());
}

auto
Array::
crend() const noexcept ->
    const_reverse_iterator
{
    return const_reverse_iterator(begin());
}

//----------------------------------------------------------
//
// Capacity
//
//----------------------------------------------------------

std::size_t
Array::
size() const noexcept
{
    return t_->size;
}

constexpr
std::size_t
Array::
max_size() noexcept
{
    // max_size depends on the address model
    using min = std::integral_constant<std::size_t,
        (std::size_t(-1) - sizeof(table)) / sizeof(Value)>;
    return min::value < BOOST_JSON_MAX_STRUCTURED_SIZE ?
        min::value : BOOST_JSON_MAX_STRUCTURED_SIZE;
}

std::size_t
Array::
capacity() const noexcept
{
    return t_->capacity;
}

bool
Array::
empty() const noexcept
{
    return t_->size == 0;
}

void
Array::
reserve(
    std::size_t new_capacity)
{
    // never shrink
    if(new_capacity <= t_->capacity)
        return;
    reserve_impl(new_capacity);
}

//----------------------------------------------------------
//
// private
//
//----------------------------------------------------------

template<class InputIt>
Array::
Array(
    InputIt first, InputIt last,
    storage_ptr sp,
    std::input_iterator_tag)
    : sp_(std::move(sp))
    , t_(&empty_)
{
    revert_construct r(*this);
    while(first != last)
    {
        reserve(size() + 1);
        ::new(end()) Value(
            *first++, sp_);
        ++t_->size;
    }
    r.commit();
}

template<class InputIt>
Array::
Array(
    InputIt first, InputIt last,
    storage_ptr sp,
    std::forward_iterator_tag)
    : sp_(std::move(sp))
{
    std::size_t n =
        std::distance(first, last);
    if( n == 0 )
    {
        t_ = &empty_;
        return;
    }

    t_ = table::allocate(n, sp_);
    t_->size = 0;
    revert_construct r(*this);
    while(n--)
    {
        ::new(end()) Value(
            *first++, sp_);
        ++t_->size;
    }
    r.commit();
}

template<class InputIt>
auto
Array::
insert(
    const_iterator pos,
    InputIt first, InputIt last,
    std::input_iterator_tag) ->
        iterator
{
    BOOST_ASSERT(
        pos >= begin() && pos <= end());
    if(first == last)
        return data() + (pos - data());
    Array temp(first, last, sp_);
    revert_insert r(
        pos, temp.size(), *this);
    relocate(
        r.p,
        temp.data(),
        temp.size());
    temp.t_->size = 0;
    return r.commit();
}

template<class InputIt>
auto
Array::
insert(
    const_iterator pos,
    InputIt first, InputIt last,
    std::forward_iterator_tag) ->
        iterator
{
    std::size_t n =
        std::distance(first, last);
    revert_insert r(pos, n, *this);
    while(n--)
    {
        ::new(r.p) Value(*first++);
        ++r.p;
    }
    return r.commit();
}

BOOST_JSON_NS_END

#endif
