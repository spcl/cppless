#pragma once

#include <chrono>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

struct measurements
{
  std::vector<std::tuple<int, int, uint64_t, std::string, bool>> time_results;


  void write(std::string output_location)
  {
    std::ofstream output_file{output_location, std::ios::out};
    output_file << "repetition,time" << std::endl;
    for(auto & res : time_results)
      output_file << std::get<0>(res) << "," << std::get<1>(res) << std::endl;
  }
};

struct serverless_measurements
{
  typedef std::chrono::high_resolution_clock clock_t;
  typedef typename clock_t::time_point time_point_t;

  std::vector<std::tuple<int, int, uint64_t, std::string, bool>> time_results;
  int start_position_vec;
  int rep;
  int first_id;

  void start_repetition(int rep)
  {
    this->rep = rep;
    this->first_id = -1;
    this->start_position_vec = time_results.size();
  }

  void add_function_start(int id, time_point_t t)
  {
    auto ts = std::chrono::time_point_cast<std::chrono::microseconds>(t).time_since_epoch().count();
    if(first_id == -1)
      first_id = id;

    time_results.emplace_back(rep, id, ts, "", false);
  }

  void add_function_result(const std::tuple<int, std::string, bool>& res)
  {
    auto end = std::chrono::high_resolution_clock::now(); 
    auto ts = std::chrono::time_point_cast<std::chrono::microseconds>(end).time_since_epoch().count();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();

    int pos = std::get<0>(res);
    int pos_shift = pos - first_id;
    int pos_in_vector = start_position_vec + pos_shift;

    std::get<2>(time_results[pos_in_vector]) = ts - std::get<2>(time_results[pos_in_vector]);
    std::get<3>(time_results[pos_in_vector]) = std::get<1>(res).invocation_id;
    std::get<4>(time_results[pos_in_vector]) = std::get<1>(res).is_cold;
  }

  void add_result(time_point_t begin, time_point_t end, const std::string& label)
  {
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count();
    time_results.emplace_back(rep, -1, duration, label, false);
  }

  void write(std::string output_location)
  {

    std::ofstream output_file{output_location, std::ios::out};
    output_file << "repetition,sample,time,request_id,is_cold" << std::endl;
    for(auto & res : time_results)
      output_file << std::get<0>(res) << "," << std::get<1>(res) << "," << std::get<2>(res) << "," << std::get<3>(res) << "," << std::get<4>(res) << std::endl;

  }
};
