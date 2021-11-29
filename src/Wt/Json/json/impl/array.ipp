//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_IMPL_Array_IPP
#define BOOST_JSON_IMPL_Array_IPP

#include "../array.hpp"
#include "../pilfer.hpp"
#include "../detail/except.hpp"
#include "../detail/hash_combine.hpp"
#include <cstdlib>
#include <limits>
#include <new>
#include <utility>

BOOST_JSON_NS_BEGIN

//----------------------------------------------------------

constexpr Array::table::table() = default;

// empty Arrays point here
BOOST_JSON_REQUIRE_CONST_INIT
Array::table Array::empty_;

auto
Array::
table::
allocate(
    std::size_t capacity,
    storage_ptr const& sp) ->
        table*
{
    BOOST_ASSERT(capacity > 0);
    if(capacity > Array::max_size())
        detail::throw_length_error(
            "Array too large",
            BOOST_JSON_SOURCE_POS);
    auto p = reinterpret_cast<
        table*>(sp->allocate(
            sizeof(table) +
                capacity * sizeof(Value),
            alignof(Value)));
    p->capacity = static_cast<
        std::uint32_t>(capacity);
    return p;
}

void
Array::
table::
deallocate(
    table* p,
    storage_ptr const& sp)
{
    if(p->capacity == 0)
        return;
    sp->deallocate(p,
        sizeof(table) +
            p->capacity * sizeof(Value),
        alignof(Value));
}

//----------------------------------------------------------

Array::
revert_insert::
revert_insert(
    const_iterator pos,
    std::size_t n,
    Array& arr)
    : arr_(&arr)
    , i_(pos - arr_->data())
    , n_(n)
{
    BOOST_ASSERT(
        pos >= arr_->begin() &&
        pos <= arr_->end());
    if( n_ <= arr_->capacity() -
        arr_->size())
    {
        // fast path
        p = arr_->data() + i_;
        if(n_ == 0)
            return;
        relocate(
            p + n_,
            p,
            arr_->size() - i_);
        arr_->t_->size = static_cast<
            std::uint32_t>(
                arr_->t_->size + n_);
        return;
    }
    if(n_ > max_size() - arr_->size())
        detail::throw_length_error(
            "Array too large",
            BOOST_JSON_SOURCE_POS);
    auto t = table::allocate(
        arr_->growth(arr_->size() + n_),
            arr_->sp_);
    t->size = static_cast<std::uint32_t>(
        arr_->size() + n_);
    p = &(*t)[0] + i_;
    relocate(
        &(*t)[0],
        arr_->data(),
        i_);
    relocate(
        &(*t)[i_ + n_],
        arr_->data() + i_,
        arr_->size() - i_);
    t = detail::exchange(arr_->t_, t);
    table::deallocate(t, arr_->sp_);
}

Array::
revert_insert::
~revert_insert()
{
    if(! arr_)
        return;
    BOOST_ASSERT(n_ != 0);
    auto const pos =
        arr_->data() + i_;
    arr_->destroy(pos, p);
    arr_->t_->size = static_cast<
        std::uint32_t>(
            arr_->t_->size - n_);
    relocate(
        pos,
        pos + n_,
        arr_->size() - i_);
}

//----------------------------------------------------------

void
Array::
destroy(
    Value* first, Value* last) noexcept
{
    if(sp_.is_not_shared_and_deallocate_is_trivial())
        return;
    while(last-- != first)
        last->~Value();
}

void
Array::
destroy() noexcept
{
    if(sp_.is_not_shared_and_deallocate_is_trivial())
        return;
    auto last = end();
    auto const first = begin();
    while(last-- != first)
        last->~Value();
    table::deallocate(t_, sp_);
}

//----------------------------------------------------------
//
// Special Members
//
//----------------------------------------------------------

Array::
Array(detail::unchecked_array&& ua)
    : sp_(ua.storage())
{
    BOOST_STATIC_ASSERT(
        alignof(table) == alignof(Value));
    if(ua.size() == 0)
    {
        t_ = &empty_;
        return;
    }
    t_= table::allocate(
        ua.size(), sp_);
    t_->size = static_cast<
        std::uint32_t>(ua.size());
    ua.relocate(data());
}

Array::
~Array()
{
    destroy();
}

Array::
Array(
    std::size_t count,
    Value const& v,
    storage_ptr sp)
    : sp_(std::move(sp))
{
    if(count == 0)
    {
        t_ = &empty_;
        return;
    }
    t_= table::allocate(
        count, sp_);
    t_->size = 0;
    revert_construct r(*this);
    while(count--)
    {
        ::new(end()) Value(v, sp_);
        ++t_->size;
    }
    r.commit();
}

Array::
Array(
    std::size_t count,
    storage_ptr sp)
    : sp_(std::move(sp))
{
    if(count == 0)
    {
        t_ = &empty_;
        return;
    }
    t_ = table::allocate(
        count, sp_);
    t_->size = static_cast<
        std::uint32_t>(count);
    auto p = data();
    do
    {
        ::new(p++) Value(sp_);
    }
    while(--count);
}

Array::
Array(Array const& other)
    : Array(other, other.sp_)
{
}

Array::
Array(
    Array const& other,
    storage_ptr sp)
    : sp_(std::move(sp))
{
    if(other.empty())
    {
        t_ = &empty_;
        return;
    }
    t_ = table::allocate(
        other.size(), sp_);
    t_->size = 0;
    revert_construct r(*this);
    auto src = other.data();
    auto dest = data();
    auto const n = other.size();
    do
    {
        ::new(dest++) Value(
            *src++, sp_);
        ++t_->size;
    }
    while(t_->size < n);
    r.commit();
}

Array::
Array(
    Array&& other,
    storage_ptr sp)
    : sp_(std::move(sp))
{
    if(*sp_ == *other.sp_)
    {
        // same resource
        t_ = detail::exchange(
            other.t_, &empty_);
        return;
    }
    else if(other.empty())
    {
        t_ = &empty_;
        return;
    }
    // copy
    t_ = table::allocate(
        other.size(), sp_);
    t_->size = 0;
    revert_construct r(*this);
    auto src = other.data();
    auto dest = data();
    auto const n = other.size();
    do
    {
        ::new(dest++) Value(
            *src++, sp_);
        ++t_->size;
    }
    while(t_->size < n);
    r.commit();
}

Array::
Array(
    std::initializer_list<
        value_ref> init,
    storage_ptr sp)
    : sp_(std::move(sp))
{
    if(init.size() == 0)
    {
        t_ = &empty_;
        return;
    }
    t_ = table::allocate(
        init.size(), sp_);
    t_->size = 0;
    revert_construct r(*this);
    value_ref::write_array(
        data(), init, sp_);
    t_->size = static_cast<
        std::uint32_t>(init.size());
    r.commit();
}

//----------------------------------------------------------

Array&
Array::
operator=(Array const& other)
{
    Array(other,
        storage()).swap(*this);
    return *this;
}

Array&
Array::
operator=(Array&& other)
{
    Array(std::move(other),
        storage()).swap(*this);
    return *this;
}

Array&
Array::
operator=(
    std::initializer_list<value_ref> init)
{
    Array(init,
        storage()).swap(*this);
    return *this;
}

//----------------------------------------------------------
//
// Capacity
//
//----------------------------------------------------------

void
Array::
shrink_to_fit() noexcept
{
    if(capacity() <= size())
        return;
    if(size() == 0)
    {
        table::deallocate(t_, sp_);
        t_ = &empty_;
        return;
    }

#ifndef BOOST_NO_EXCEPTIONS
    try
    {
#endif
        auto t = table::allocate(
            size(), sp_);
        relocate(
            &(*t)[0],
            data(),
            size());
        t->size = static_cast<
            std::uint32_t>(size());
        t = detail::exchange(
            t_, t);
        table::deallocate(t, sp_);
#ifndef BOOST_NO_EXCEPTIONS
    }
    catch(...)
    {
        // eat the exception
        return;
    }
#endif
}

//----------------------------------------------------------
//
// Modifiers
//
//----------------------------------------------------------

void
Array::
clear() noexcept
{
    if(size() == 0)
        return;
    destroy(
        begin(), end());
    t_->size = 0;
}

auto
Array::
insert(
    const_iterator pos,
    Value const& v) ->
        iterator
{
    return emplace(pos, v);
}

auto
Array::
insert(
    const_iterator pos,
    Value&& v) ->
        iterator
{
    return emplace(pos, std::move(v));
}

auto
Array::
insert(
    const_iterator pos,
    std::size_t count,
    Value const& v) ->
        iterator
{
    revert_insert r(
        pos, count, *this);
    while(count--)
    {
        ::new(r.p) Value(v, sp_);
        ++r.p;
    }
    return r.commit();
}

auto
Array::
insert(
    const_iterator pos,
    std::initializer_list<
        value_ref> init) ->
    iterator
{
    revert_insert r(
        pos, init.size(), *this);
    value_ref::write_array(
        r.p, init, sp_);
    return r.commit();
}

auto
Array::
erase(
    const_iterator pos) noexcept ->
    iterator
{
    BOOST_ASSERT(
        pos >= begin() &&
        pos <= end());
    auto const p = &(*t_)[0] +
        (pos - &(*t_)[0]);
    destroy(p, p + 1);
    relocate(p, p + 1, 1);
    --t_->size;
    return p;
}

auto
Array::
erase(
    const_iterator first,
    const_iterator last) noexcept ->
        iterator
{
    std::size_t const n =
        last - first;
    auto const p = &(*t_)[0] +
        (first - &(*t_)[0]);
    destroy(p, p + n);
    relocate(p, p + n,
        t_->size - (last -
            &(*t_)[0]));
    t_->size = static_cast<
        std::uint32_t>(t_->size - n);
    return p;
}

void
Array::
push_back(Value const& v)
{
    emplace_back(v);
}

void
Array::
push_back(Value&& v)
{
    emplace_back(std::move(v));
}

void
Array::
pop_back() noexcept
{
    auto const p = &back();
    destroy(p, p + 1);
    --t_->size;
}

void
Array::
resize(std::size_t count)
{
    if(count <= t_->size)
    {
        // shrink
        destroy(
            &(*t_)[0] + count,
            &(*t_)[0] + t_->size);
        t_->size = static_cast<
            std::uint32_t>(count);
        return;
    }

    reserve(count);
    auto p = &(*t_)[t_->size];
    auto const end = &(*t_)[count];
    while(p != end)
        ::new(p++) Value(sp_);
    t_->size = static_cast<
        std::uint32_t>(count);
}

void
Array::
resize(
    std::size_t count,
    Value const& v)
{
    if(count <= size())
    {
        // shrink
        destroy(
            data() + count,
            data() + size());
        t_->size = static_cast<
            std::uint32_t>(count);
        return;
    }
    count -= size();
    revert_insert r(
        end(), count, *this);
    while(count--)
    {
        ::new(r.p) Value(v, sp_);
        ++r.p;
    }
    r.commit();
}

void
Array::
swap(Array& other)
{
    BOOST_ASSERT(this != &other);
    if(*sp_ == *other.sp_)
    {
        t_ = detail::exchange(
            other.t_, t_);
        return;
    }
    Array temp1(
        std::move(*this),
        other.storage());
    Array temp2(
        std::move(other),
        this->storage());
    this->~Array();
    ::new(this) Array(
        pilfer(temp2));
    other.~Array();
    ::new(&other) Array(
        pilfer(temp1));
}

//----------------------------------------------------------
//
// Private
//
//----------------------------------------------------------

std::size_t
Array::
growth(
    std::size_t new_size) const
{
    if(new_size > max_size())
        detail::throw_length_error(
            "Array too large",
            BOOST_JSON_SOURCE_POS);
    std::size_t const old = capacity();
    if(old > max_size() - old / 2)
        return new_size;
    std::size_t const g =
        old + old / 2; // 1.5x
    if(g < new_size)
        return new_size;
    return g;
}

// precondition: new_capacity > capacity()
void
Array::
reserve_impl(
    std::size_t new_capacity)
{
    BOOST_ASSERT(
        new_capacity > t_->capacity);
    auto t = table::allocate(
        growth(new_capacity), sp_);
    relocate(
        &(*t)[0],
        &(*t_)[0],
        t_->size);
    t->size = t_->size;
    t = detail::exchange(t_, t);
    table::deallocate(t, sp_);
}

// precondition: pv is not aliased
Value&
Array::
push_back(
    pilfered<Value> pv)
{
    auto const n = t_->size;
    if(n < t_->capacity)
    {
        // fast path
        auto& v = *::new(
            &(*t_)[n]) Value(pv);
        ++t_->size;
        return v;
    }
    auto const t =
        detail::exchange(t_,
            table::allocate(
                growth(n + 1),
                    sp_));
    auto& v = *::new(
        &(*t_)[n]) Value(pv);
    relocate(
        &(*t_)[0],
        &(*t)[0],
        n);
    t_->size = n + 1;
    table::deallocate(t, sp_);
    return v;
}

// precondition: pv is not aliased
auto
Array::
insert(
    const_iterator pos,
    pilfered<Value> pv) ->
        iterator
{
    BOOST_ASSERT(
        pos >= begin() &&
        pos <= end());
    std::size_t const n =
        t_->size;
    std::size_t const i =
        pos - &(*t_)[0];
    if(n < t_->capacity)
    {
        // fast path
        auto const p =
            &(*t_)[i];
        relocate(
            p + 1,
            p,
            n - i);
        ::new(p) Value(pv);
        ++t_->size;
        return p;
    }
    auto t =
        table::allocate(
            growth(n + 1), sp_);
    auto const p = &(*t)[i];
    ::new(p) Value(pv);
    relocate(
        &(*t)[0],
        &(*t_)[0],
        i);
    relocate(
        p + 1,
        &(*t_)[i],
        n - i);
    t->size = static_cast<
        std::uint32_t>(size() + 1);
    t = detail::exchange(t_, t);
    table::deallocate(t, sp_);
    return p;
}

//----------------------------------------------------------

bool
Array::
equal(
    Array const& other) const noexcept
{
    if(size() != other.size())
        return false;
    for(std::size_t i = 0; i < size(); ++i)
        if((*this)[i] != other[i])
            return false;
    return true;
}

BOOST_JSON_NS_END

//----------------------------------------------------------
//
// std::hash specialization
//
//----------------------------------------------------------

std::size_t
std::hash<::Wt::Json::Array>::operator()(
    ::Wt::Json::Array const& ja) const noexcept
{
  std::size_t seed = ja.size();
  for (const auto& jv : ja) {
    seed = ::Wt::Json::detail::hash_combine(
        seed,
        std::hash<::Wt::Json::Value>{}(jv));
  }
  return seed;
}

//----------------------------------------------------------

#endif
