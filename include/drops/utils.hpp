#pragma once

#include <eosio.system/eosio.system.hpp>

using namespace std;
using namespace eosio;

namespace utils {

static name parse_name(const string& str);

static vector<string> split(const string& str, const char delim);

static int64_t to_number(const string& str);

} // namespace utils