/*
 *  Created on: Jan 29, 2020
 *  Author: Papon
 */

#include <iostream>
#include <cmath>
#include <sys/time.h> 
#include "parameter.h"

/*Set up the singleton object with the experiment wide options*/
EmuEnv* EmuEnv::instance = 0;

EmuEnv::EmuEnv() 
{
  int buffer_size_in_pages = 100;
  int disk_size_in_pages = 10000;

  int num_operations = 10000;
  float perct_reads = 70;
  float perct_writes = 30;
  int algorithm = 0;

  float skewed_perct = 100;
  float skewed_data_perct = 100;
  long read_cost = 1;
  long write_cost = 1;

  int concurrency = 1;

  int pin_mode = 0;
  int verbosity = 0;

  int request_size = 4096;
  string filename = "testfile";
  int fd;
  
  ctpl::thread_pool tp (16);
}

EmuEnv* EmuEnv::getInstance()
{
  if (instance == 0)
    instance = new EmuEnv();

  return instance;
}
