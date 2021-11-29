//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_IMPL_OBJECT_HPP
#define BOOST_JSON_IMPL_OBJECT_HPP

#include "../value.hpp"
#include <iterator>
#include <cmath>
#include <type_traits>
#include <utility>

BOOST_JSON_NS_BEGIN

namespace detail {

// Objects with size less than or equal
// to this number will use a linear search
// instead of the more expensive hash function.
static
constexpr
std::size_t
small_object_size_ = 18;

BOOST_STATIC_ASSERT(
    small_object_size_ <
    BOOST_JSON_MAX_STRUCTURED_SIZE);

} // detail

//----------------------------------------------------------

struct alignas(key_value_pair)
    Object::table
{
    std::uint32_t size = 0;
    std::uint32_t capacity = 0;
    std::uintptr_t salt = 0;

#if defined(_MSC_VER) && BOOST_JSON_ARCH == 32
    // VFALCO If we make key_value_pair smaller,
    //        then we might want to revisit this
    //        padding.
    BOOST_STATIC_ASSERT(
        sizeof(key_value_pair) == 32);
    char pad[4] = {}; // silence warnings
#endif

    constexpr table();

    // returns true if we use a linear
    // search instead of the hash table.
    bool is_small() const noexcept
    {
        return capacity <=
            detail::small_object_size_;
    }

    key_value_pair&
    operator[](
        std::size_t pos) noexcept
    {
        return reinterpret_cast<
            key_value_pair*>(
                this + 1)[pos];
    }

    // VFALCO This is exported for tests
    BOOST_JSON_DECL
    std::size_t
    digest(string_view key) const noexcept;

    inline
    index_t&
    bucket(std::size_t hash) noexcept;

    inline
    index_t&
    bucket(string_view key) noexcept;

    inline
    void
    clear() noexcept;

    static
    inline
    table*
    allocate(
        std::size_t capacity,
        std::uintptr_t salt,
        storage_ptr const& sp);

    static
    void
    deallocate(
        table* p,
        storage_ptr const& sp) noexcept
    {
        if(p->capacity == 0)
            return;
        if(! p->is_small())
            sp->deallocate(p,
                sizeof(table) + p->capacity * (
                    sizeof(key_value_pair) +
                    sizeof(index_t)));
        else
            sp->deallocate(p,
                sizeof(table) + p->capacity *
                    sizeof(key_value_pair));
    }
};

//----------------------------------------------------------

class Object::revert_construct
{
    Object* obj_;

    BOOST_JSON_DECL
    void
    destroy() noexcept;

public:
    explicit
    revert_construct(
        Object& obj) noexcept
        : obj_(&obj)
    {
    }

    ~revert_construct()
    {
        if(! obj_)
            return;
        destroy();
    }

    void
    commit() noexcept
    {
        obj_ = nullptr;
    }
};

//----------------------------------------------------------

class Object::revert_insert
{
    Object* obj_;
    std::size_t size_;

    BOOST_JSON_DECL
    void
    destroy() noexcept;

public:
    explicit
    revert_insert(
        Object& obj) noexcept
        : obj_(&obj)
        , size_(obj_->size())
    {
    }

    ~revert_insert()
    {
        if(! obj_)
            return;
        destroy();
        obj_->t_->size = static_cast<
            index_t>(size_);
    }

    void
    commit() noexcept
    {
        obj_ = nullptr;
    }
};

//----------------------------------------------------------
//
// Iterators
//
//----------------------------------------------------------

auto
Object::
begin() noexcept ->
    iterator
{
    return &(*t_)[0];
}

auto
Object::
begin() const noexcept ->
    const_iterator
{
    return &(*t_)[0];
}

auto
Object::
cbegin() const noexcept ->
    const_iterator
{
    return &(*t_)[0];
}

auto
Object::
end() noexcept ->
    iterator
{
    return &(*t_)[t_->size];
}

auto
Object::
end() const noexcept ->
    const_iterator
{
    return &(*t_)[t_->size];
}

auto
Object::
cend() const noexcept ->
    const_iterator
{
    return &(*t_)[t_->size];
}

auto
Object::
rbegin() noexcept ->
    reverse_iterator
{
    return reverse_iterator(end());
}

auto
Object::
rbegin() const noexcept ->
    const_reverse_iterator
{
    return const_reverse_iterator(end());
}

auto
Object::
crbegin() const noexcept ->
    const_reverse_iterator
{
    return const_reverse_iterator(end());
}

auto
Object::
rend() noexcept ->
    reverse_iterator
{
    return reverse_iterator(begin());
}

auto
Object::
rend() const noexcept ->
    const_reverse_iterator
{
    return const_reverse_iterator(begin());
}

auto
Object::
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

bool
Object::
empty() const noexcept
{
    return t_->size == 0;
}

auto
Object::
size() const noexcept ->
    std::size_t
{
    return t_->size;
}

constexpr
std::size_t
Object::
max_size() noexcept
{
    // max_size depends on the address model
    using min = std::integral_constant<std::size_t,
        (std::size_t(-1) - sizeof(table)) /
            (sizeof(key_value_pair) + sizeof(index_t))>;
    return min::value < BOOST_JSON_MAX_STRUCTURED_SIZE ?
        min::value : BOOST_JSON_MAX_STRUCTURED_SIZE;
}

auto
Object::
capacity() const noexcept ->
    std::size_t
{
    return t_->capacity;
}

//----------------------------------------------------------
//
// Lookup
//
//----------------------------------------------------------

auto
Object::
at(string_view key) ->
    Value&
{
    auto it = find(key);
    if(it == end())
        detail::throw_out_of_range(
            BOOST_JSON_SOURCE_POS);
    return it->value();
}

auto
Object::
at(string_view key) const ->
    Value const&
{
    auto it = find(key);
    if(it == end())
        detail::throw_out_of_range(
            BOOST_JSON_SOURCE_POS);
    return it->value();
}

auto
Object::
get(string_view key) -> Value*
{
    auto vsc = splitSV(key);
    Value* jv = if_contains(vsc[0]);
    for(unsigned i(1); i < vsc.size(); i++)
    {
        if(!jv)
            return nullptr;
        if(auto o = jv->if_object())
            jv = o->if_contains(vsc[i]);
        else
            return nullptr;
    }
    return jv;
}

auto
Object::
get(string_view key) const -> Value const*
{
    auto vsc = splitSV(key);
    const Value* jv = if_contains(vsc[0]);
    for(unsigned i(1); i < vsc.size(); i++)
    {
        if(!jv)
            return nullptr;
        if(auto o = jv->if_object())
            jv = o->if_contains(vsc[i]);
        else
            return nullptr;
    }
    return jv;
}
//----------------------------------------------------------

template<class P, class>
auto
Object::
insert(P&& p) ->
    std::pair<iterator, bool>
{
    key_value_pair v(
        std::forward<P>(p), sp_);
    return insert_impl(pilfer(v));
}

template<class M>
auto
Object::
insert_or_assign(
    string_view key, M&& m) ->
        std::pair<iterator, bool>
{
    reserve(size() + 1);
    auto const result = find_impl(key);
    if(result.first)
    {
        Value(std::forward<M>(m),
            sp_).swap(result.first->value());
        return { result.first, false };
    }
    key_value_pair kv(key,
        std::forward<M>(m), sp_);
    return { insert_impl(pilfer(kv),
        result.second), true };
}

template<class Arg>
auto
Object::
emplace(
    string_view key,
    Arg&& arg) ->
        std::pair<iterator, bool>
{
    reserve(size() + 1);
    auto const result = find_impl(key);
    if(result.first)
        return { result.first, false };
    key_value_pair kv(key,
        std::forward<Arg>(arg), sp_);
    return { insert_impl(pilfer(kv),
        result.second), true };
}

//----------------------------------------------------------
//
// (private)
//
//----------------------------------------------------------

template<class InputIt>
void
Object::
construct(
    InputIt first,
    InputIt last,
    std::size_t min_capacity,
    std::input_iterator_tag)
{
    reserve(min_capacity);
    revert_construct r(*this);
    while(first != last)
    {
        insert(*first);
        ++first;
    }
    r.commit();
}

template<class InputIt>
void
Object::
construct(
    InputIt first,
    InputIt last,
    std::size_t min_capacity,
    std::forward_iterator_tag)
{
    auto n = static_cast<
        std::size_t>(std::distance(
            first, last));
    if( n < min_capacity)
        n = min_capacity;
    reserve(n);
    revert_construct r(*this);
    while(first != last)
    {
        insert(*first);
        ++first;
    }
    r.commit();
}

template<class InputIt>
void
Object::
insert(
    InputIt first,
    InputIt last,
    std::input_iterator_tag)
{
    // Since input iterators cannot be rewound,
    // we keep inserted elements on an exception.
    //
    while(first != last)
    {
        insert(*first);
        ++first;
    }
}

template<class InputIt>
void
Object::
insert(
    InputIt first,
    InputIt last,
    std::forward_iterator_tag)
{
    auto const n =
        static_cast<std::size_t>(
            std::distance(first, last));
    auto const n0 = size();
    if(n > max_size() - n0)
        detail::throw_length_error(
            "object too large",
            BOOST_JSON_SOURCE_POS);
    reserve(n0 + n);
    revert_insert r(*this);
    while(first != last)
    {
        insert(*first);
        ++first;
    }
    r.commit();
}

//----------------------------------------------------------

namespace detail {

unchecked_object::
~unchecked_object()
{
    if(! data_)
        return;
    if(sp_.is_not_shared_and_deallocate_is_trivial())
        return;
    Value* p = data_;
    while(size_--)
    {
        p[0].~Value();
        p[1].~Value();
        p += 2;
    }
}

} // detail

BOOST_JSON_NS_END

#endif
