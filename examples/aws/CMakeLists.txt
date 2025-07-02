include(FetchContent)
project(cpplessExamplesAws LANGUAGES CXX)

include(../../cmake/aws.cmake)

add_executable(hello_world "hello_world.cpp")
aws_lambda_target(hello_world)
aws_lambda_package_target(hello_world)

#add_executable(host_controller_executor_aws "host_controller_executor.cpp")
#aws_lambda_target(host_controller_executor_aws)
#aws_lambda_serverless_target(host_controller_executor_aws)

add_executable(simple_dispatcher_aws "simple_dispatcher.cpp")
aws_lambda_target(simple_dispatcher_aws)
aws_lambda_serverless_target(simple_dispatcher_aws)

add_executable(dispatcher_aws "dispatcher.cpp")
aws_lambda_target(dispatcher_aws)
aws_lambda_serverless_target(dispatcher_aws)

#add_executable(hole_punching_aws "hole_punching.cpp")
#aws_lambda_target(hole_punching_aws)
#aws_lambda_serverless_target(hole_punching_aws)
