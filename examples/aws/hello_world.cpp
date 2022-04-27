#include <aws/lambda-runtime/runtime.h>

using invocation_response = aws::lambda_runtime::invocation_response;
using invocation_request = aws::lambda_runtime::invocation_request;

auto my_handler(invocation_request const& /*request*/) -> invocation_response
{
  return invocation_response::success("Hello, World 123!", "application/json");
}

auto main() -> int
{
  run_handler(my_handler);
  return 0;
}