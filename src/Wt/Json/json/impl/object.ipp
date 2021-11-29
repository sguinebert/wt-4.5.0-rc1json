//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_IMPL_OBJECT_IPP
#define BOOST_JSON_IMPL_OBJECT_IPP

#include "../object.hpp"
#include "../detail/digest.hpp"
#include "../detail/except.hpp"
#include "../detail/hash_combine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <type_traits>

BOOST_JSON_NS_BEGIN

//----------------------------------------------------------

constexpr Object::table::table() = default;

// empty objects point here
BOOST_JSON_REQUIRE_CONST_INIT
Object::table Object::empty_;

std::size_t
Object::table::
digest(string_view key) const noexcept
{
    BOOST_ASSERT(salt != 0);
    return detail::digest(
        key.data(), key.size(), salt);
}

auto
Object::table::
bucket(std::size_t hash) noexcept ->
    index_t&
{
    return reinterpret_cast<
        index_t*>(&(*this)[capacity])[
            hash % capacity];
}

auto
Object::table::
bucket(string_view key) noexcept ->
    index_t&
{
    return bucket(digest(key));
}

void
Object::table::
clear() noexcept
{
    BOOST_ASSERT(! is_small());
    // initialize buckets
    std::memset(
        reinterpret_cast<index_t*>(
            &(*this)[capacity]),
        0xff, // null_index_
        capacity * sizeof(index_t));
}

Object::table*
Object::table::
allocate(
    std::size_t capacity,
    std::uintptr_t salt,
    storage_ptr const& sp)
{
    BOOST_STATIC_ASSERT(
        alignof(key_value_pair) >=
        alignof(index_t));
    BOOST_ASSERT(capacity > 0);
    BOOST_ASSERT(capacity <= max_size());
    table* p;
    if(capacity <= detail::small_object_size_)
    {
        p = reinterpret_cast<
            table*>(sp->allocate(
                sizeof(table) + capacity *
                    sizeof(key_value_pair)));
        p->capacity = static_cast<
            std::uint32_t>(capacity);
    }
    else
    {
        p = reinterpret_cast<
            table*>(sp->allocate(
                sizeof(table) + capacity * (
                    sizeof(key_value_pair) +
                    sizeof(index_t))));
        p->capacity = static_cast<
            std::uint32_t>(capacity);
        p->clear();
    }
    if(salt)
    {
        p->salt = salt;
    }
    else
    {
        // VFALCO This would be better if it
        //        was random, but maybe this
        //        is good enough.
        p->salt = reinterpret_cast<
            std::uintptr_t>(p);
    }
    return p;
}

//----------------------------------------------------------

void
Object::
revert_construct::
destroy() noexcept
{
    obj_->destroy();
}

//----------------------------------------------------------

void
Object::
revert_insert::
destroy() noexcept
{
    obj_->destroy(
        &(*obj_->t_)[size_],
        obj_->end());
}

//----------------------------------------------------------
//
// Construction
//
//----------------------------------------------------------

Object::
Object(detail::unchecked_object&& uo)
    : sp_(uo.storage())
{
    if(uo.size() == 0)
    {
        t_ = &empty_;
        return;
    }
    // should already be checked
    BOOST_ASSERT(
        uo.size() <= max_size());
    t_ = table::allocate(
        uo.size(), 0, sp_);

    // insert all elements, keeping
    // the last of any duplicate keys.
    auto dest = begin();
    auto src = uo.release();
    auto const end = src + 2 * uo.size();
    if(t_->is_small())
    {
        t_->size = 0;
        while(src != end)
        {
            access::construct_key_value_pair(
                dest, pilfer(src[0]), pilfer(src[1]));
            src += 2;
            auto result = find_impl(dest->key());
            if(! result.first)
            {
                ++dest;
                ++t_->size;
                continue;
            }
            // handle duplicate
            auto& v = *result.first;
            // don't bother to check if
            // storage deallocate is trivial
            v.~key_value_pair();
            // trivial relocate
            std::memcpy(
                static_cast<void*>(&v),
                    dest, sizeof(v));
        }
        return;
    }
    while(src != end)
    {
        access::construct_key_value_pair(
            dest, pilfer(src[0]), pilfer(src[1]));
        src += 2;
        auto& head = t_->bucket(dest->key());
        auto i = head;
        for(;;)
        {
            if(i == null_index_)
            {
                // end of bucket
                access::next(
                    *dest) = head;
                head = static_cast<index_t>(
                    dest - begin());
                ++dest;
                break;
            }
            auto& v = (*t_)[i];
            if(v.key() != dest->key())
            {
                i = access::next(v);
                continue;
            }

            // handle duplicate
            access::next(*dest) =
                access::next(v);
            // don't bother to check if
            // storage deallocate is trivial
            v.~key_value_pair();
            // trivial relocate
            std::memcpy(
                static_cast<void*>(&v),
                    dest, sizeof(v));
            break;
        }
    }
    t_->size = static_cast<
        index_t>(dest - begin());
}

Object::
~Object()
{
    if(sp_.is_not_shared_and_deallocate_is_trivial())
        return;
    if(t_->capacity == 0)
        return;
    destroy();
}

Object::
Object(
    std::size_t min_capacity,
    storage_ptr sp)
    : sp_(std::move(sp))
    , t_(&empty_)
{
    reserve(min_capacity);
}

Object::
Object(Object&& other) noexcept
    : sp_(other.sp_)
    , t_(detail::exchange(
        other.t_, &empty_))
{
}

Object::
Object(
    Object&& other,
    storage_ptr sp)
    : sp_(std::move(sp))
{
    if(*sp_ == *other.sp_)
    {
        t_ = detail::exchange(
            other.t_, &empty_);
        return;
    }

    t_ = &empty_;
    Object(other, sp_).swap(*this);
}

Object::
Object(
    Object const& other,
    storage_ptr sp)
    : sp_(std::move(sp))
    , t_(&empty_)
{
    reserve(other.size());
    revert_construct r(*this);
    if(t_->is_small())
    {
        for(auto const& v : other)
        {
            ::new(end())
                key_value_pair(v, sp_);
            ++t_->size;
        }
        r.commit();
        return;
    }
    for(auto const& v : other)
    {
        // skip duplicate checking
        auto& head =
            t_->bucket(v.key());
        auto pv = ::new(end())
            key_value_pair(v, sp_);
        access::next(*pv) = head;
        head = t_->size;
        ++t_->size;
    }
    r.commit();
}

Object::
Object(
    std::initializer_list<std::pair<
        string_view, value_ref>> init,
    std::size_t min_capacity,
    storage_ptr sp)
    : sp_(std::move(sp))
    , t_(&empty_)
{
    if( min_capacity < init.size())
        min_capacity = init.size();
    reserve(min_capacity);
    revert_construct r(*this);
    insert(init);
    r.commit();
}

//----------------------------------------------------------
//
// Assignment
//
//----------------------------------------------------------

Object&
Object::
operator=(Object const& other)
{
    Object tmp(other, sp_);
    this->~Object();
    ::new(this) Object(pilfer(tmp));
    return *this;
}

Object&
Object::
operator=(Object&& other)
{
    Object tmp(std::move(other), sp_);
    this->~Object();
    ::new(this) Object(pilfer(tmp));
    return *this;
}

Object&
Object::
operator=(
    std::initializer_list<std::pair<
        string_view, value_ref>> init)
{
    Object tmp(init, sp_);
    this->~Object();
    ::new(this) Object(pilfer(tmp));
    return *this;
}

//----------------------------------------------------------
//
// Modifiers
//
//----------------------------------------------------------

void
Object::
clear() noexcept
{
    if(empty())
        return;
    if(! sp_.is_not_shared_and_deallocate_is_trivial())
        destroy(begin(), end());
    if(! t_->is_small())
        t_->clear();
    t_->size = 0;
}

void
Object::
insert(
    std::initializer_list<std::pair<
        string_view, value_ref>> init)
{
    auto const n0 = size();
    if(init.size() > max_size() - n0)
        detail::throw_length_error(
            "object too large",
            BOOST_JSON_SOURCE_POS);
    reserve(n0 + init.size());
    revert_insert r(*this);
    if(t_->is_small())
    {
        for(auto& iv : init)
        {
            auto result =
                find_impl(iv.first);
            if(result.first)
            {
                // ignore duplicate
                continue;
            }
            ::new(end()) key_value_pair(
                iv.first,
                iv.second.make_value(sp_));
            ++t_->size;
        }
        r.commit();
        return;
    }
    for(auto& iv : init)
    {
        auto& head = t_->bucket(iv.first);
        auto i = head;
        for(;;)
        {
            if(i == null_index_)
            {
                // VFALCO value_ref should construct
                // a key_value_pair using placement
                auto& v = *::new(end())
                    key_value_pair(
                        iv.first,
                        iv.second.make_value(sp_));
                access::next(v) = head;
                head = static_cast<index_t>(
                    t_->size);
                ++t_->size;
                break;
            }
            auto& v = (*t_)[i];
            if(v.key() == iv.first)
            {
                // ignore duplicate
                break;
            }
            i = access::next(v);
        }
    }
    r.commit();
}

auto
Object::
erase(const_iterator pos) noexcept ->
    iterator
{
    auto p = begin() + (pos - begin());
    if(t_->is_small())
    {
        p->~value_type();
        --t_->size;
        auto const pb = end();
        if(p != end())
        {
            // the casts silence warnings
            std::memcpy(
                static_cast<void*>(p),
                static_cast<void const*>(pb),
                sizeof(*p));
        }
        return p;
    }
    remove(t_->bucket(p->key()), *p);
    p->~value_type();
    --t_->size;
    auto const pb = end();
    if(p != end())
    {
        auto& head = t_->bucket(pb->key());
        remove(head, *pb);
        // the casts silence warnings
        std::memcpy(
            static_cast<void*>(p),
            static_cast<void const*>(pb),
            sizeof(*p));
        access::next(*p) = head;
        head = static_cast<
            index_t>(p - begin());
    }
    return p;
}

auto
Object::
erase(string_view key) noexcept ->
    std::size_t
{
    auto it = find(key);
    if(it == end())
        return 0;
    erase(it);
    return 1;
}

void
Object::
swap(Object& other)
{
    if(*sp_ == *other.sp_)
    {
        t_ = detail::exchange(
            other.t_, t_);
        return;
    }
    Object temp1(
        std::move(*this),
        other.storage());
    Object temp2(
        std::move(other),
        this->storage());
    other.~Object();
    ::new(&other) Object(pilfer(temp1));
    this->~Object();
    ::new(this) Object(pilfer(temp2));
}

//----------------------------------------------------------
//
// Lookup
//
//----------------------------------------------------------

auto
Object::
operator[](string_view key) ->
    Value&
{
    auto const result =
        emplace(key, nullptr);
    return result.first->value();
}

auto
Object::
count(string_view key) const noexcept ->
    std::size_t
{
    if(find(key) == end())
        return 0;
    return 1;
}

auto
Object::
find(string_view key) noexcept ->
    iterator
{
    if(empty())
        return end();
    auto const p =
        find_impl(key).first;
    if(p)
        return p;
    return end();
}

auto
Object::
find(string_view key) const noexcept ->
    const_iterator
{
    if(empty())
        return end();
    auto const p =
        find_impl(key).first;
    if(p)
        return p;
    return end();
}

bool
Object::
contains(
    string_view key) const noexcept
{
    if(empty())
        return false;
    return find_impl(
        key).first != nullptr;
}

Value const*
Object::
if_contains(
    string_view key) const noexcept
{
    auto const it = find(key);
    if(it != end())
        return &it->value();
    return nullptr;
}

Value*
Object::
if_contains(
    string_view key) noexcept
{
    auto const it = find(key);
    if(it != end())
        return &it->value();
    return nullptr;
}

//----------------------------------------------------------
//
// (private)
//
//----------------------------------------------------------

auto
Object::
find_impl(
    string_view key) const noexcept ->
        std::pair<
            key_value_pair*,
            std::size_t>
{
    BOOST_ASSERT(t_->capacity > 0);
    if(t_->is_small())
    {
        auto it = &(*t_)[0];
        auto const last =
            &(*t_)[t_->size];
        for(;it != last; ++it)
            if(key == it->key())
                return { it, 0 };
        return { nullptr, 0 };
    }
    std::pair<
        key_value_pair*,
        std::size_t> result;
    result.second = t_->digest(key);
    auto i = t_->bucket(
        result.second);
    while(i != null_index_)
    {
        auto& v = (*t_)[i];
        if(v.key() == key)
        {
            result.first = &v;
            return result;
        }
        i = access::next(v);
    }
    result.first = nullptr;
    return result;
}

auto
Object::
insert_impl(
    pilfered<key_value_pair> p) ->
        std::pair<iterator, bool>
{
    // caller is responsible
    // for preventing aliasing.
    reserve(size() + 1);
    auto const result =
        find_impl(p.get().key());
    if(result.first)
        return { result.first, false };
    return { insert_impl(
        p, result.second), true };
}

key_value_pair*
Object::
insert_impl(
    pilfered<key_value_pair> p,
    std::size_t hash)
{
    BOOST_ASSERT(
        capacity() > size());
    if(t_->is_small())
    {
        auto const pv = ::new(end())
            key_value_pair(p);
        ++t_->size;
        return pv;
    }
    auto& head =
        t_->bucket(hash);
    auto const pv = ::new(end())
        key_value_pair(p);
    access::next(*pv) = head;
    head = t_->size;
    ++t_->size;
    return pv;
}

// rehash to at least `n` buckets
void
Object::
rehash(std::size_t new_capacity)
{
    BOOST_ASSERT(
        new_capacity > t_->capacity);
    auto t = table::allocate(
        growth(new_capacity),
            t_->salt, sp_);
    if(! empty())
        std::memcpy(
            static_cast<
                void*>(&(*t)[0]),
            begin(),
            size() * sizeof(
                key_value_pair));
    t->size = t_->size;
    table::deallocate(t_, sp_);
    t_ = t;

    if(! t_->is_small())
    {
        // rebuild hash table,
        // without dup checks
        auto p = end();
        index_t i = t_->size;
        while(i-- > 0)
        {
            --p;
            auto& head =
                t_->bucket(p->key());
            access::next(*p) = head;
            head = i;
        }
    }
}

bool
Object::
equal(Object const& other) const noexcept
{
    if(size() != other.size())
        return false;
    auto const end_ = other.end();
    for(auto e : *this)
    {
        auto it = other.find(e.key());
        if(it == end_)
            return false;
        if(it->value() != e.value())
            return false;
    }
    return true;
}

std::size_t
Object::
growth(
    std::size_t new_size) const
{
    if(new_size > max_size())
        detail::throw_length_error(
            "object too large",
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

void
Object::
remove(
    index_t& head,
    key_value_pair& v) noexcept
{
    BOOST_ASSERT(! t_->is_small());
    auto const i = static_cast<
        index_t>(&v - begin());
    if(head == i)
    {
        head = access::next(v);
        return;
    }
    auto* pn =
        &access::next((*t_)[head]);
    while(*pn != i)
        pn = &access::next((*t_)[*pn]);
    *pn = access::next(v);
}

void
Object::
destroy() noexcept
{
    BOOST_ASSERT(t_->capacity > 0);
    BOOST_ASSERT(! sp_.is_not_shared_and_deallocate_is_trivial());
    destroy(begin(), end());
    table::deallocate(t_, sp_);
}

void
Object::
destroy(
    key_value_pair* first,
    key_value_pair* last) noexcept
{
    BOOST_ASSERT(! sp_.is_not_shared_and_deallocate_is_trivial());
    while(last != first)
        (--last)->~key_value_pair();
}

BOOST_JSON_NS_END

//----------------------------------------------------------
//
// std::hash specialization
//
//----------------------------------------------------------

std::size_t
std::hash<::Wt::Json::Object>::operator()(
    ::Wt::Json::Object const& jo) const noexcept
{
    std::size_t seed = jo.size();
    for (const auto& kv_pair : jo) {
        auto const hk = ::Wt::Json::detail::digest(
            kv_pair.key().data(), kv_pair.key().size(), 0);
        auto const hkv = ::Wt::Json::detail::hash_combine(
            hk,
            std::hash<::Wt::Json::Value>{}(kv_pair.value()));
        seed = ::Wt::Json::detail::hash_combine_commutative(seed, hkv);
    }
    return seed;
}

//----------------------------------------------------------


#endif
