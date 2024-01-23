/*
 *  Created on: Jan 29, 2020
 *  Author: Papon
 */

#ifndef PARAMETER_H_
#define PARAMETER_H_
 

#include <iostream>
#include "ctpl_stl.h" 


using namespace std;

class EmuEnv
{
private:
  EmuEnv(); 
  static EmuEnv *instance;

public:
  static EmuEnv* getInstance();

  int buffer_size_in_pages;   // b
  int disk_size_in_pages;     // n

  int num_operations;    // x
  int perct_reads;       // e
  int perct_writes;      

  float skewed_perct;      //s
  float skewed_data_perct; //d
  long read_cost;     //r
  long write_cost;    //w

  int pin_mode;   //p

  int verbosity;         // v
  int algorithm;         // a
  int concurrency;      // k

  int request_size;
  string filename;
  int fd;
  ctpl::thread_pool tp;

};

#endif /*PARAMETER_H_*/

