/*
 *  Created on: Jan 29, 2019
 *  Author: Papon
 */
 
#include "parameter.h"

#include <iostream>
#include <vector>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h> 
#include <pthread.h>

using namespace std;

namespace bufmanager {

  typedef struct timeval Timeval;

  typedef struct QNode
  {
    struct QNode *prev, *next;
    unsigned pageId; // the page number stored in this QNode
    bool dirty;
  } QNode;

  // A Queue (A FIFO collection of Queue Nodes)
  typedef struct Queue
  {
    unsigned count;          // Number of filled frames
    unsigned size; // total number of frames
    QNode *front, *rear;
  } Queue;

  // A hash (Collection of pointers to Queue Nodes)
  typedef struct Hash
  {
    int capacity;  // how many pages can be there
    QNode **array; // an array of queue nodes
  } Hash;

  class Buffer {
    private:
      Buffer(EmuEnv *_env);
      static Buffer *buffer_instance;

    public:

      static Timeval start_time, complete_time; 

      static Buffer *getBufferInstance(EmuEnv *_env);

      static Queue* queue;
      static Hash* hash;

      static long clock;

      static float read_lat;
      static float write_lat;

      static int buffer_hit;
      static int buffer_miss;
      static int read_io;
      static int write_io;
      static int write_count;

      static bool hasWritten;

      static int *pages;

      int LRU();

      static int printBuffer(EmuEnv *_env, Buffer* buffer_instance);
      static int printStats();
      static long time_diff(Timeval end, Timeval start);
  };

  class WorkloadExecutor {
    private:
    public:

    static int read(Buffer* buffer_instance, int pageId, int algorithm);
    static int write(Buffer* buffer_instance, int pageId, int algorithm);
    // static int search(Buffer* buffer_instance, int pageId);
    static int unpin(Buffer* buffer_instance, int pageId);
  };
}
