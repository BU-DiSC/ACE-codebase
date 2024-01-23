/*
 *  Created on: Jan 29, 2020
 *  Author: Papon
 */
 
#include <sstream>
#include <iostream>
#include <cstdio>
#include <sys/time.h>
#include <cmath>
#include <unistd.h>
#include <assert.h>
#include <fstream>
#include <iomanip>

#include "args.hxx"
#include "parameter.h"
#include "buff_runner.h"
#include "workload_generator.h"

using namespace std;
using namespace bufmanager;

/*
 * DECLARATIONS
*/

int parse_arguments(int argc, char *argvx[], EmuEnv* _env);
void printEmulationOutput(EmuEnv* _env);
int runWorkload(EmuEnv* _env);
inline void showProgress(const int &workload_size, const int &counter);

int main(int argc, char *argvx[])
{
  Timeval time1, time2;

  EmuEnv* _env = EmuEnv::getInstance();
  if (parse_arguments(argc, argvx, _env)){
    exit(1);
  }
  
  printEmulationOutput(_env);
  if (_env->num_operations > 0) 
  {
    std::cerr << "Issuing operations ... " << std::endl << std::flush;  
    WorkloadGenerator workload_generator;
    workload_generator.generateWorkload();
    gettimeofday(&(time1), NULL);
    int s = runWorkload(_env);
    gettimeofday(&(time2), NULL);
    cout << "Workload Execution Time (s): " << Buffer::time_diff(time2, time1)/1000000.0 << endl;
    close(_env->fd);
  }
  Buffer::printStats();
  return 0;
}

int runWorkload(EmuEnv* _env) {

  Buffer* buffer_instance = Buffer::getBufferInstance(_env);
  bufmanager::WorkloadExecutor workload_executer;
  ifstream workload_file;
  workload_file.open("workload.txt"); 
  int counter = 0;
  

  assert(workload_file);

  while(!workload_file.eof()) {
    char instruction;
    int pageId;

    workload_file >> instruction >> pageId;
    switch (instruction)
    {
      case 'R':
        if (_env->verbosity == 2)
        {
          cout << "R " << pageId << endl;
        }
        workload_executer.read(buffer_instance, pageId, _env->algorithm);
        if (_env->verbosity == 2)
        {
          Buffer::printBuffer(_env, buffer_instance);
        }

        break;
      case 'W':
        if (_env->verbosity == 2)
        {
          cout << "W " << pageId << endl;
        }
        workload_executer.write(buffer_instance, pageId, _env->algorithm);
        if (_env->verbosity == 2)
        {
          Buffer::printBuffer(_env, buffer_instance);
        }
        break;
      case 'U':
        if (_env->verbosity == 2)
        {
          cout << "U " << pageId << endl;
        }
        workload_executer.unpin(buffer_instance, pageId);
        if (_env->verbosity == 2)
        {
          Buffer::printBuffer(_env, buffer_instance);
        }
        break;
    }
    instruction = '\0';

    counter++;

    if (_env->verbosity != 2)
    {
      if (_env->num_operations >= 100)
      {
        if (counter % (_env->num_operations / 100) == 0 && counter <= _env->num_operations)
          showProgress(_env->num_operations, counter);
      }
      else
      {
        if (counter <= _env->num_operations)
          showProgress(_env->num_operations, counter);
      }
    }
  }
  return 1;
}


int parse_arguments(int argc,char *argvx[], EmuEnv* _env) {
  args::ArgumentParser parser("Buffer Manager Emulator", "");

  args::Group group1(parser, "This group is all exclusive:", args::Group::Validators::DontCare);

  args::ValueFlag<int> buffer_size_in_pages_cmd(group1, "b", "Size of Buffer in terms of pages [def: 100]", {'b', "buffer_size_in_pages"});
  args::ValueFlag<int> disk_size_in_pages_cmd(group1, "n", "Size of Disk in terms of pages [def: 10000]", {'n', "disk_size_in_pages"});
  args::ValueFlag<int> num_operations_cmd(group1, "x", "Total number of operations to be performed [def: 10000]", {'x', "num_operations"});
  args::ValueFlag<int> perct_reads_cmd(group1, "e", "Percentage of read in workload [def: 70.0%]", {'e', "perct_reads"});
  args::ValueFlag<int> verbosity_cmd(group1, "v", "The verbosity level of execution [0,1,2; def:0]", {'v', "verbosity"});
  args::ValueFlag<int> algorithm_cmd(group1, "a", "Algorithm of page eviction [0 for rand, 1 for LRU, 2 for LFU; def:0]", {'a', "algorithm"});
  args::ValueFlag<int> skewed_perct_cmd(group1, "s", "Skewed distribution of operation on data [s% r/w on d% data, def: 100]", {'s', "skewed_perct"});
  args::ValueFlag<int> skewed_data_perct_cmd(group1, "d", "Skewed distribution of operation on data [s% r/w on d% data, def: 100]", {'d', "skewed_data_perct"});
  args::ValueFlag<int> read_cost_cmd(group1, "r", "Cost of read operation [def:1]", {'r', "read_cost"});
  args::ValueFlag<int> write_cost_cmd(group1, "w", "Cost of write operation [def:1]", {'w', "write_cost"});
  args::ValueFlag<int> pin_mode_cmd(group1, "p", "Pin Enabled/Disabled [def:0]", {'p', "pin_mode"});
  args::ValueFlag<int> concurrency_cmd(group1, "k", "Level of concurrency [def:1]", {'k', "concurrency"});

  args::ValueFlag<string> filename_cmd(group1, "f", "File name [def:testfile]", {'f', "filename"});


  try
  {
      parser.ParseCLI(argc, argvx);
  }
  catch (args::Help&)
  {
      std::cout << parser;
      exit(0);
      // return 0;
  }
  catch (args::ParseError& e)
  {
      std::cerr << e.what() << std::endl;
      std::cerr << parser;
      return 1;
  }
  catch (args::ValidationError& e)
  {
      std::cerr << e.what() << std::endl;
      std::cerr << parser;
      return 1;
  }

  _env->buffer_size_in_pages = buffer_size_in_pages_cmd ? args::get(buffer_size_in_pages_cmd) : 100;
  
  _env->num_operations = num_operations_cmd ? args::get(num_operations_cmd) : 10000;
  _env->perct_reads = perct_reads_cmd ? args::get(perct_reads_cmd) : 70.0;
  _env->perct_writes = 100.0 - _env->perct_reads;
  _env->verbosity = verbosity_cmd ? args::get(verbosity_cmd) : 0;
  _env->algorithm = algorithm_cmd ? args::get(algorithm_cmd) : 0;
  _env->skewed_perct = skewed_perct_cmd ? args::get(skewed_perct_cmd) : 100;
  _env->skewed_data_perct = skewed_data_perct_cmd ? args::get(skewed_data_perct_cmd) : 100;
  _env->read_cost = read_cost_cmd ? args::get(read_cost_cmd) : 1;
  _env->write_cost = write_cost_cmd ? args::get(write_cost_cmd) : 1;
  _env->pin_mode = pin_mode_cmd ? args::get(pin_mode_cmd) : 0;
  _env->concurrency = concurrency_cmd ? args::get(concurrency_cmd) : 1;

  _env->filename = filename_cmd ? args::get(filename_cmd) : "testfile";
  _env->request_size = 4096;
  _env->fd = open(_env->filename.c_str(), O_RDWR | O_DIRECT | O_SYNC);
  long filesize = lseek(_env->fd, 0, SEEK_END);
  _env->disk_size_in_pages = filesize / 4096;
  _env->disk_size_in_pages = disk_size_in_pages_cmd ? args::get(disk_size_in_pages_cmd) : filesize / 4096;
  _env->tp.resize(_env->concurrency);
  
  return 0;
}

void printEmulationOutput(EmuEnv* _env) {
  //if (_env->verbosity >= 1) 
  std::cout << "b\t n\t x\t e\t e'\t a\t s\t d\t r\t w\t p\t k  \n";
  std::cout << _env->buffer_size_in_pages << "\t ";
  std::cout << _env->disk_size_in_pages << "\t ";  
  std::cout << _env->num_operations << "\t ";
  std::cout << _env->perct_reads << "\t ";
  std::cout << _env->perct_writes << "\t ";
  std::cout << _env->algorithm << "\t ";
  std::cout << _env->skewed_perct << "\t ";
  std::cout << _env->skewed_data_perct << "\t ";
  std::cout << _env->read_cost << "\t ";
  std::cout << _env->write_cost << "\t ";
  std::cout << _env->pin_mode << "\t ";
  std::cout << _env->concurrency << "\t ";

  std::cout << std::endl;
}

inline void showProgress(const int &workload_size, const int &counter) {
  
    // std::cout << "counter = " << counter << std::endl;
    if (counter / (workload_size/100.0) >= 1) {
      for (int i = 0; i<104; i++){
        std::cout << "\b";
        fflush(stdout);
      }
    }
    for (int i = 0; i<counter / (workload_size/100.0); i++){
      std::cout << "=" ;
      fflush(stdout);
    }
    std::cout << std::setfill(' ') << std::setw(101 - counter / (workload_size/100.0));
    std::cout << counter*100/workload_size << "%";
      fflush(stdout);

  if (counter == workload_size) {
    std::cout << "\n";
    return;
  }
}