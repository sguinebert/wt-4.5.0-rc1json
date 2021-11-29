//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_IMPL_VALUE_IPP
#define BOOST_JSON_IMPL_VALUE_IPP

#include "../value.hpp"
#include "../detail/hash_combine.hpp"
#include "../string.hpp"
#include <cstring>
#include <limits>
#include <new>
#include <utility>

BOOST_JSON_NS_BEGIN

Value::
~Value()
{
    switch(kind())
    {
    case kind::null:
    case kind::bool_:
    case kind::int64:
    case kind::uint64:
    case kind::double_:
        sca_.~scalar();
        break;

    case kind::string:
        str_.~string();
        break;

    case kind::array:
        arr_.~Array();
        break;

    case kind::object:
        obj_.~Object();
        break;
    }
}

Value::
Value(
    Value const& other,
    storage_ptr sp)
{
    switch(other.kind())
    {
    case kind::null:
        ::new(&sca_) scalar(
            std::move(sp));
        break;

    case kind::bool_:
        ::new(&sca_) scalar(
            other.sca_.b,
            std::move(sp));
        break;

    case kind::int64:
        ::new(&sca_) scalar(
            other.sca_.i,
            std::move(sp));
        break;

    case kind::uint64:
        ::new(&sca_) scalar(
            other.sca_.u,
            std::move(sp));
        break;

    case kind::double_:
        ::new(&sca_) scalar(
            other.sca_.d,
            std::move(sp));
        break;

    case kind::string:
        ::new(&str_) string(
            other.str_,
            std::move(sp));
        break;

    case kind::array:
        ::new(&arr_) Array(
            other.arr_,
            std::move(sp));
        break;

    case kind::object:
        ::new(&obj_) Object(
            other.obj_,
            std::move(sp));
        break;
    }
}

Value::
Value(Value&& other) noexcept
{
    relocate(this, other);
    ::new(&other.sca_) scalar(sp_);
}

Value::
Value(
    Value&& other,
    storage_ptr sp)
{
    switch(other.kind())
    {
    case kind::null:
        ::new(&sca_) scalar(
            std::move(sp));
        break;

    case kind::bool_:
        ::new(&sca_) scalar(
            other.sca_.b, std::move(sp));
        break;

    case kind::int64:
        ::new(&sca_) scalar(
            other.sca_.i, std::move(sp));
        break;

    case kind::uint64:
        ::new(&sca_) scalar(
            other.sca_.u, std::move(sp));
        break;

    case kind::double_:
        ::new(&sca_) scalar(
            other.sca_.d, std::move(sp));
        break;

    case kind::string:
        ::new(&str_) string(
            std::move(other.str_),
            std::move(sp));
        break;

    case kind::array:
        ::new(&arr_) Array(
            std::move(other.arr_),
            std::move(sp));
        break;

    case kind::object:
        ::new(&obj_) Object(
            std::move(other.obj_),
            std::move(sp));
        break;
    }
}

//----------------------------------------------------------
//
// Conversion
//
//----------------------------------------------------------

Value::
Value(
    std::initializer_list<value_ref> init,
    storage_ptr sp)
{
    if(value_ref::maybe_object(init))
        ::new(&obj_) Object(
            value_ref::make_object(
                init, std::move(sp)));
    else
        ::new(&arr_) Array(
            value_ref::make_array(
                init, std::move(sp)));
}

//----------------------------------------------------------
//
// Assignment
//
//----------------------------------------------------------

Value&
Value::
operator=(Value const& other)
{
    Value(other,
        storage()).swap(*this);
    return *this;
}

Value&
Value::
operator=(Value&& other)
{
    Value(std::move(other),
        storage()).swap(*this);
    return *this;
}

Value&
Value::
operator=(
    std::initializer_list<value_ref> init)
{
    Value(init,
        storage()).swap(*this);
    return *this;
}

Value&
Value::
operator=(string_view s)
{
    Value(s, storage()).swap(*this);
    return *this;
}

Value&
Value::
operator=(char const* s)
{
    Value(s, storage()).swap(*this);
    return *this;
}

Value&
Value::
operator=(string const& str)
{
    Value(str, storage()).swap(*this);
    return *this;
}

Value&
Value::
operator=(string&& str)
{
    Value(std::move(str),
        storage()).swap(*this);
    return *this;
}

Value&
Value::
operator=(Array const& arr)
{
    Value(arr, storage()).swap(*this);
    return *this;
}

Value&
Value::
operator=(Array&& arr)
{
    Value(std::move(arr),
        storage()).swap(*this);
    return *this;
}

Value&
Value::
operator=(Object const& obj)
{
    Value(obj, storage()).swap(*this);
    return *this;
}

Value&
Value::
operator=(Object&& obj)
{
    Value(std::move(obj),
        storage()).swap(*this);
    return *this;
}

//----------------------------------------------------------
//
// Modifiers
//
//----------------------------------------------------------

string&
Value::
emplace_string() noexcept
{
    return *::new(&str_) string(destroy());
}

Array&
Value::
emplace_array() noexcept
{
    return *::new(&arr_) Array(destroy());
}

Object&
Value::
emplace_object() noexcept
{
    return *::new(&obj_) Object(destroy());
}

void
Value::
swap(Value& other)
{
    if(*storage() == *other.storage())
    {
        // fast path
        union U
        {
            Value tmp;
            U(){}
            ~U(){}
        };
        U u;
        relocate(&u.tmp, *this);
        relocate(this, other);
        relocate(&other, u.tmp);
        return;
    }

    // copy
    Value temp1(
        std::move(*this),
        other.storage());
    Value temp2(
        std::move(other),
        this->storage());
    other.~Value();
    ::new(&other) Value(pilfer(temp1));
    this->~Value();
    ::new(this) Value(pilfer(temp2));
}

//----------------------------------------------------------
//
// private
//
//----------------------------------------------------------

storage_ptr
Value::
destroy() noexcept
{
    switch(kind())
    {
    case kind::null:
    case kind::bool_:
    case kind::int64:
    case kind::uint64:
    case kind::double_:
        break;

    case kind::string:
    {
        auto sp = str_.storage();
        str_.~string();
        return sp;
    }

    case kind::array:
    {
        auto sp = arr_.storage();
        arr_.~Array();
        return sp;
    }

    case kind::object:
    {
        auto sp = obj_.storage();
        obj_.~Object();
        return sp;
    }

    }
    return std::move(sp_);
}

bool
Value::
equal(Value const& other) const noexcept
{
    switch(kind())
    {
    default: // unreachable()?
    case kind::null:
        return other.kind() == kind::null;

    case kind::bool_:
        return
            other.kind() == kind::bool_ &&
            get_bool() == other.get_bool();

    case kind::int64:
        switch(other.kind())
        {
        case kind::int64:
            return get_int64() == other.get_int64();
        case kind::uint64:
            if(get_int64() < 0)
                return false;
            return static_cast<std::uint64_t>(
                get_int64()) == other.get_uint64();
        default:
            return false;
        }

    case kind::uint64:
        switch(other.kind())
        {
        case kind::uint64:
            return get_uint64() == other.get_uint64();
        case kind::int64:
            if(other.get_int64() < 0)
                return false;
            return static_cast<std::uint64_t>(
                other.get_int64()) == get_uint64();
        default:
            return false;
        }

    case kind::double_:
        return
            other.kind() == kind::double_ &&
            get_double() == other.get_double();

    case kind::string:
        return
            other.kind() == kind::string &&
            get_string() == other.get_string();

    case kind::array:
        return
            other.kind() == kind::array &&
            get_array() == other.get_array();

    case kind::object:
        return
            other.kind() == kind::object &&
            get_object() == other.get_object();
    }
}

//----------------------------------------------------------
//
// key_value_pair
//
//----------------------------------------------------------

// empty keys point here
BOOST_JSON_REQUIRE_CONST_INIT
char const
key_value_pair::empty_[1] = { 0 };

key_value_pair::
key_value_pair(
    pilfered<Json::Value> key,
    pilfered<Json::Value> value) noexcept
    : value_(value)
{
    std::size_t len;
    key_ = access::release_key(key.get(), len);
    len_ = static_cast<std::uint32_t>(len);
}

key_value_pair::
key_value_pair(
    key_value_pair const& other,
    storage_ptr sp)
    : value_(other.value_, std::move(sp))
{
    auto p = reinterpret_cast<
        char*>(value_.storage()->
            allocate(other.len_ + 1,
                alignof(char)));
    std::memcpy(
        p, other.key_, other.len_);
    len_ = other.len_;
    p[len_] = 0;
    key_ = p;
}

//----------------------------------------------------------

BOOST_JSON_NS_END

//----------------------------------------------------------
//
// std::hash specialization
//
//----------------------------------------------------------

std::size_t
std::hash<::Wt::Json::Value>::operator()(
    ::Wt::Json::Value const& jv) const noexcept
{
  std::size_t seed = static_cast<std::size_t>(jv.kind());
  switch (jv.kind()) {
    default:
    case ::Wt::Json::kind::null:
      return seed;
    case ::Wt::Json::kind::bool_:
      return ::Wt::Json::detail::hash_combine(
        seed,
        hash<bool>{}(jv.get_bool()));
    case ::Wt::Json::kind::int64:
      return ::Wt::Json::detail::hash_combine(
        static_cast<size_t>(::Wt::Json::kind::uint64),
        hash<std::uint64_t>{}(jv.get_int64()));
    case ::Wt::Json::kind::uint64:
      return ::Wt::Json::detail::hash_combine(
        seed,
        hash<std::uint64_t>{}(jv.get_uint64()));
    case ::Wt::Json::kind::double_:
      return ::Wt::Json::detail::hash_combine(
        seed,
        hash<double>{}(jv.get_double()));
    case ::Wt::Json::kind::string:
      return ::Wt::Json::detail::hash_combine(
        seed,
        hash<::Wt::Json::string>{}(jv.get_string()));
    case ::Wt::Json::kind::array:
      return ::Wt::Json::detail::hash_combine(
        seed,
        hash<::Wt::Json::Array>{}(jv.get_array()));
    case ::Wt::Json::kind::object:
      return ::Wt::Json::detail::hash_combine(
        seed,
        hash<::Wt::Json::Object>{}(jv.get_object()));
  }
}

//----------------------------------------------------------

#endif
