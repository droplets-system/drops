namespace utils {

/**
 * ## STATIC `parse_name`
 *
 * Parse string for account name. Return default name if invalid. Caller can check validity with name::value.
 *
 * ### params
 *
 * - `{string} str` - string to parse
 *
 * ### returns
 *
 * - `{name}` - name
 *
 * ### example
 *
 * ```c++
 * const name contract = utils::parse_name( "tethertether" );
 * // contract => "tethertether"_n
 * ```
 */
static name parse_name(const string& str)
{

   if (str.length() == 0 || str.length() > 12)
      return {};
   int i = 0;
   for (const auto c : str) {
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '5') || c == '.') {
         if (i == str.length() - 1 && c == '.')
            return {}; // can't end with a .
      } else
         return {};
      i++;
   }
   return name{str};
}

/**
 * ## STATIC `split`
 *
 * Split string into tokens
 *
 * ### params
 *
 * - `{string} str` - string to split
 * - `{string} delim` - delimiter (ex: ",")
 *
 * ### returns
 *
 * - `{vector<string>}` - tokenized strings
 *
 * ### example
 *
 * ```c++
 * const auto[ token0, token1 ] = utils::split( "foo,bar", "," );
 * // token0 => "foo"
 * // token1 => "bar"
 * ```
 */
static vector<string> split(const string& str, const char delim)
{
   vector<string> strings;
   size_t         start;
   size_t         end = 0;
   while ((start = str.find_first_not_of(delim, end)) != string::npos) {
      end = str.find(delim, start);
      strings.push_back(str.substr(start, end - start));
   }
   return strings;
}

/**
 * ## STATIC `to_number`
 *
 * Convert string to number
 *
 * ### params
 *
 * - `{string} str` - string to convert
 *
 * ### returns
 *
 * - `{int64_t}` - signed number
 *
 * ### example
 *
 * ```c++
 * const int64_t num = utils::to_number("123");
 * // 123
 * ```
 */
static int64_t to_number(const string& str)
{
   if (str.empty())
      return 0;

   char*          end;
   const uint64_t num = std::strtoull(str.c_str(), &end, 10);

   // Check if conversion was successful
   check(*end == '\0', "invalid number format or overflow");

   // Check for underflow
   check(num <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()), "number underflow");

   return static_cast<int64_t>(num);
}

} // namespace utils