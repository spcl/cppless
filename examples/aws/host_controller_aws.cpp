#include <iostream>

#include <aws/lambda-runtime/runtime.h>

using invocation_response = aws::lambda_runtime::invocation_response;
using invocation_request = aws::lambda_runtime::invocation_request;

auto my_handler(invocation_request const& /*request*/) -> invocation_response
{
  return invocation_response::success("Hello, World 123!", "application/json");
}

__attribute((entry)) auto alt_entry(int /*argc*/, char* /*argv*/[]) -> int
{
  run_handler(my_handler);
  return 0;
}

__attribute((weak)) auto main(int /*argc*/, char* /*argv*/[]) -> int
{
  std::cout << "Hello main 123" << std::endl;
  return 0;
}