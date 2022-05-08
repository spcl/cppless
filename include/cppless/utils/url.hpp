#include <array>
#include <string>
#include <vector>

#include <boost/algorithm/hex.hpp>

inline auto url_encode(const std::string& s) -> std::string
{
  std::vector<char> v(s.size());
  v.clear();
  for (char c : s) {
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.'
        || c == '!' || c == '~' || c == '*' || c == '\'' || c == '('
        || c == ')')
    {
      v.push_back(c);
    } else if (c == ' ') {
      v.push_back('+');
    } else {
      v.push_back('%');
      std::array<char, 1> hex_input {c};
      boost::algorithm::hex_lower(
          hex_input.begin(), hex_input.end(), std::back_inserter(v));
    }
  }

  return {v.cbegin(), v.cend()};
}