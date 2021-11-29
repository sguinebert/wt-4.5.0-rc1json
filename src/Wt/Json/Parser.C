
#include "Parser.h"
//#include <Wt/Json/json.hpp>
#include "Wt/WStringStream.h"
#include "json/src.hpp"

namespace Wt {
  namespace Json {

namespace {

static constexpr int MAX_RECURSION_DEPTH = 1000;

}



Value parse(const std::string& input, bool valideUTF8, bool allow_trailing_commas, bool allow_comments)
{
    parse_options opt; // all extensions default to off
    opt.allow_comments = allow_comments;                          // permit C and C++ style comments to appear in whitespace
    opt.allow_trailing_commas = allow_trailing_commas;            // allow an additional trailing comma in object and array element lists
    opt.allow_invalid_utf8 = !valideUTF8;                         // skip utf-8 validation of keys and strings
    return Wt::Json::parse(input, storage_ptr(), opt);
}

Value parse(const std::string& input, ParseError &error, bool valideUTF8, bool allow_trailing_commas, bool allow_comments)
{
    parse_options opt; // all extensions default to off
    opt.allow_comments = allow_comments;                          // permit C and C++ style comments to appear in whitespace
    opt.allow_trailing_commas = allow_trailing_commas;            // allow an additional trailing comma in object and array element lists
    opt.allow_invalid_utf8 = !valideUTF8;                         // skip utf-8 validation of keys and strings
    return Wt::Json::parse(input, error, storage_ptr(), opt);
}

void parse(const std::string& input, Object& result, bool valideUTF8, bool allow_trailing_commas, bool allow_comments)
{
    auto value = parse(input, valideUTF8, allow_trailing_commas, allow_comments);

    Object& parsedObject = value.as_object();

    parsedObject.swap(result);
}

bool parse(const std::string& input, Object& result, ParseError& error, bool valideUTF8, bool allow_trailing_commas, bool allow_comments)
{
    auto value = parse(input, error, valideUTF8, allow_trailing_commas, allow_comments);

    if(!error){
        Object& parsedObject = value.as_object();
        parsedObject.swap(result);
    }

    return !error;
}


void parse(const std::string& input, Array& result, bool valideUTF8, bool allow_trailing_commas, bool allow_comments)
{
    auto value = parse(input, valideUTF8, allow_trailing_commas, allow_comments);

    Array& parsedObject = value.as_array();

    parsedObject.swap(result);
}

bool parse(const std::string& input, Array& result, ParseError& error, bool valideUTF8, bool allow_trailing_commas, bool allow_comments)
{
    auto value = parse(input, error, valideUTF8, allow_trailing_commas, allow_comments);

    if(!error){
        Array& parsedObject = value.as_array();
        parsedObject.swap(result);
    }

    return !error;
}


  }
}
