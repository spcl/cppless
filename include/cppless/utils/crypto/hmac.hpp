#pragma once

#include <vector>

#include "./wrappers.hpp"
namespace cppless
{

template<class KeyDataT, class PlainDataT>
auto hmac(const KeyDataT& lhs, const PlainDataT& rhs)
    -> std::vector<unsigned char>
{
  evp_p_key key(lhs);
  evp_sign_ctx ctx(key);
  ctx.update(rhs);
  return ctx.final();
}

}  // namespace cppless