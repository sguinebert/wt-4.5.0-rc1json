//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_SRC_HPP
#define BOOST_JSON_SRC_HPP

/*

This file is meant to be included once,
in a translation unit of the program.

*/

#ifndef BOOST_JSON_SOURCE
#define BOOST_JSON_SOURCE
#endif

// We include this in case someone is using
// src.hpp as their main JSON header file
// https://github.com/boostorg/json/issues/223#issuecomment-689264149
#include "../json.hpp"

#include "detail/config.hpp"

#include "impl/array.ipp"
#include "impl/error.ipp"
#include "impl/kind.ipp"
#include "impl/monotonic_resource.ipp"
#include "impl/null_resource.ipp"
#include "impl/object.ipp"
#include "impl/parse.ipp"
#include "impl/parser.ipp"
#include "impl/serialize.ipp"
#include "impl/serializer.ipp"
#include "impl/static_resource.ipp"
#include "impl/stream_parser.ipp"
#include "impl/string.ipp"
#include "impl/value.ipp"
#include "impl/value_stack.ipp"
#include "impl/value_ref.ipp"

#include "detail/impl/shared_resource.ipp"
#include "detail/impl/default_resource.ipp"
#include "detail/impl/except.ipp"
#include "detail/impl/format.ipp"
#include "detail/impl/handler.ipp"
#include "detail/impl/stack.ipp"
#include "detail/impl/string_impl.ipp"

#include "detail/ryu/impl/d2s.ipp"

#endif
