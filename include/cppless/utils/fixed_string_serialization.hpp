#pragma once
#include <tuple>

#include <cppless/utils/fixed_string.hpp>

namespace cppless
{

constexpr unsigned int tag_size = 1;

constexpr char ui_tag = 0x00;
constexpr char ul_tag = 0x01;
constexpr char array_tag = 0x03;
constexpr char map_tag = 0x04;
constexpr char string_tag = 0x02;

template<class... ArrayData>
class array_wrapper
{
public:
  std::tuple<ArrayData...> array;
};

template<class... MapData>
class map_wrapper
{
public:
  constexpr explicit map_wrapper(std::tuple<MapData...> map)
      : map(map)
  {
  }
  std::tuple<MapData...> map;
};

template<class... FirstArray, class... SecondArray>
constexpr auto operator+(array_wrapper<FirstArray...> first,
                         array_wrapper<SecondArray...> second)
{
  return array_wrapper<FirstArray..., SecondArray...> {
      std::tuple_cat(first.array, second.array)};
}

template<class... FirstMap, class... SecondMap>
constexpr auto operator+(map_wrapper<FirstMap...> first,
                         map_wrapper<SecondMap...> second)
{
  return map_wrapper<FirstMap..., SecondMap...> {
      std::tuple_cat(first.map, second.map)};
}

template<class... R>
constexpr auto serialize(array_wrapper<R...> rest);

template<class... R>
constexpr auto serialize(map_wrapper<R...> rest);

template<unsigned long L>
constexpr auto serialize(basic_fixed_string<char, L> str);

template<unsigned long L>
constexpr auto serialize(const char (&str)[L]);

constexpr unsigned int ui_size = 4;
constexpr auto serialize(unsigned int x)
    -> basic_fixed_string<char, ui_size + tag_size + 1>;

constexpr unsigned int ul_size = 8;
constexpr auto serialize(unsigned long x)
    -> basic_fixed_string<char, ul_size + tag_size + 1>;

constexpr char unsigned_to_signed[256] = {
    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,    10,   11,
    12,   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,   23,
    24,   25,   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,
    36,   37,   38,   39,   40,   41,   42,   43,   44,   45,   46,   47,
    48,   49,   50,   51,   52,   53,   54,   55,   56,   57,   58,   59,
    60,   61,   62,   63,   64,   65,   66,   67,   68,   69,   70,   71,
    72,   73,   74,   75,   76,   77,   78,   79,   80,   81,   82,   83,
    84,   85,   86,   87,   88,   89,   90,   91,   92,   93,   94,   95,
    96,   97,   98,   99,   100,  101,  102,  103,  104,  105,  106,  107,
    108,  109,  110,  111,  112,  113,  114,  115,  116,  117,  118,  119,
    120,  121,  122,  123,  124,  125,  126,  127,  -128, -127, -126, -125,
    -124, -123, -122, -121, -120, -119, -118, -117, -116, -115, -114, -113,
    -112, -111, -110, -109, -108, -107, -106, -105, -104, -103, -102, -101,
    -100, -99,  -98,  -97,  -96,  -95,  -94,  -93,  -92,  -91,  -90,  -89,
    -88,  -87,  -86,  -85,  -84,  -83,  -82,  -81,  -80,  -79,  -78,  -77,
    -76,  -75,  -74,  -73,  -72,  -71,  -70,  -69,  -68,  -67,  -66,  -65,
    -64,  -63,  -62,  -61,  -60,  -59,  -58,  -57,  -56,  -55,  -54,  -53,
    -52,  -51,  -50,  -49,  -48,  -47,  -46,  -45,  -44,  -43,  -42,  -41,
    -40,  -39,  -38,  -37,  -36,  -35,  -34,  -33,  -32,  -31,  -30,  -29,
    -28,  -27,  -26,  -25,  -24,  -23,  -22,  -21,  -20,  -19,  -18,  -17,
    -16,  -15,  -14,  -13,  -12,  -11,  -10,  -9,   -8,   -7,   -6,   -5,
    -4,   -3,   -2,   -1};

constexpr unsigned long byte_mask = 0xff;
constexpr unsigned long byte_shift = 8;
constexpr auto ul_to_string(unsigned long x)
{
  basic_fixed_string<char, ul_size + 1> res;
  res[0] = unsigned_to_signed[byte_mask & (x >> (7UL * byte_shift))];
  res[1] = unsigned_to_signed[byte_mask & (x >> (6UL * byte_shift))];
  res[2] = unsigned_to_signed[byte_mask & (x >> (5UL * byte_shift))];
  res[3] = unsigned_to_signed[byte_mask & (x >> (4UL * byte_shift))];
  res[4] = unsigned_to_signed[byte_mask & (x >> (3UL * byte_shift))];
  res[5] = unsigned_to_signed[byte_mask & (x >> (2UL * byte_shift))];
  res[6] = unsigned_to_signed[byte_mask & (x >> (1UL * byte_shift))];
  res[7] = unsigned_to_signed[byte_mask & (x >> (0UL * byte_shift))];

  return res;
}

constexpr auto ui_to_string(unsigned int x)
{
  basic_fixed_string<char, ui_size + 1> res;
  res[0] = unsigned_to_signed[byte_mask & (x >> (3UL * byte_shift))];
  res[1] = unsigned_to_signed[byte_mask & (x >> (2UL * byte_shift))];
  res[2] = unsigned_to_signed[byte_mask & (x >> (1UL * byte_shift))];
  res[3] = unsigned_to_signed[byte_mask & (x >> (0UL * byte_shift))];

  return res;
}

constexpr auto array_helper()
{
  return ""_f;
}

template<class F, class... R>
constexpr auto array_helper(F&& element, R&&... elements)
{
  if constexpr (sizeof...(elements) > 0) {
    return serialize(std::forward<F>(element))
        + array_helper(std::forward<R>(elements)...);
  } else {
    return serialize(std::forward<F>(element));
  }
}

template<class R, std::size_t... Indices>
constexpr auto serialize_array(R rest, std::index_sequence<Indices...>)
{
  return array_helper(std::get<Indices>(rest)...);
}

template<class... R>
constexpr auto serialize(array_wrapper<R...> rest)
{
  const char id[] = {array_tag, 0x00};
  return id + ui_to_string(sizeof...(R))
      + serialize_array<std::tuple<R...>>(
             rest.array, std::make_index_sequence<sizeof...(R)> {});
}

template<class... R>
constexpr auto serialize(map_wrapper<R...> rest)
{
  const char id[] = {map_tag, 0x00};
  return id + ui_to_string(sizeof...(R) / 2)
      + serialize_array<std::tuple<R...>>(
             rest.map, std::make_index_sequence<sizeof...(R)> {});
}

template<unsigned long L>
constexpr auto serialize(basic_fixed_string<char, L> str)
{
  const char id[] = {string_tag, 0x00};
  return id + ui_to_string(L - 1) + str;
}

template<unsigned long L>
constexpr auto serialize(const char (&str)[L])
{
  const char id[] = {string_tag, 0x00};
  return id + ui_to_string(L - 1) + str;
}

constexpr auto serialize(const unsigned int x)
    -> basic_fixed_string<char, ui_size + tag_size + 1>
{
  const char id[] = {ui_tag, 0x00};
  return id + ui_to_string(x);
}

constexpr auto serialize(const unsigned long x)
    -> basic_fixed_string<char, ul_size + tag_size + 1>
{
  const char id[] = {ul_tag, 0x00};
  return id + ul_to_string(x);
}

template<class... R>
constexpr auto array(R&&... r) -> array_wrapper<R...>
{
  return {{r...}};
}

template<class K, class V>
constexpr auto kv(K&& k, V&& v) -> std::tuple<K, V>
{
  return {k, v};
}

template<class... R>
constexpr auto map(R&&... r)
{
  return map_wrapper {std::tuple_cat(r...)};
}

constexpr const char base64_lookup_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

template<unsigned long InputLength>
constexpr auto encode_base64(basic_fixed_string<char, InputLength> input)
{
  fixed_string<((InputLength + 2) / 3) * 4 + 1> buffer;

  size_t i = 0;
  size_t j = 0;
  for (size_t n = input.size() / 3 * 3; i < n; i += 3, j += 4) {
    uint32_t x = ((unsigned char)input[i] << 16)
        | ((unsigned char)input[i + 1] << 8) | (unsigned char)input[i + 2];
    buffer[j + 0] = base64_lookup_table[(x >> 18) & 63];
    buffer[j + 1] = base64_lookup_table[(x >> 12) & 63];
    buffer[j + 2] = base64_lookup_table[(x >> 6) & 63];
    buffer[j + 3] = base64_lookup_table[x & 63];
  }
  if (i + 1 == input.size()) {
    uint32_t x = ((unsigned char)input[i] << 16);
    buffer[j + 0] = base64_lookup_table[(x >> 18) & 63];
    buffer[j + 1] = base64_lookup_table[(x >> 12) & 63];
    buffer[j + 2] = '=';
    buffer[j + 3] = '=';
  } else if (i + 2 == input.size()) {
    uint32_t x =
        ((unsigned char)input[i] << 16) | ((unsigned char)input[i + 1] << 8);
    buffer[j + 0] = base64_lookup_table[(x >> 18) & 63];
    buffer[j + 1] = base64_lookup_table[(x >> 12) & 63];
    buffer[j + 2] = base64_lookup_table[(x >> 6) & 63];
    buffer[j + 3] = '=';
  }
  return buffer;
}

}  // namespace cppless