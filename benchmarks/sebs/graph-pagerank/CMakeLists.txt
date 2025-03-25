cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 14)

project(cpplessBenchmarksSeBSGraphPageRank CXX)

find_package(igraph REQUIRED)

add_executable("benchmark_sebs_graph_pagerank" dispatcher.cpp)
target_link_libraries("benchmark_sebs_graph_pagerank" PRIVATE cppless::cppless)
aws_lambda_target("benchmark_sebs_graph_pagerank")
aws_lambda_serverless_target("benchmark_sebs_graph_pagerank")
target_link_libraries("benchmark_sebs_graph_pagerank" PUBLIC igraph::igraph)


add_executable("benchmark_sebs_graph_pagerank_local" local.cpp)
target_link_libraries("benchmark_sebs_graph_pagerank_local" PUBLIC igraph::igraph)
