/*
 *  Created on: Jan 29, 2019
 *  Author: Papon
 */

#include <iostream>
#include <cmath>
#include <sys/time.h>
#include <iomanip>
#include <fstream>
#include <unistd.h>
#include <cstdlib>
#include <sys/types.h>
#include <fcntl.h>
#include "parameter.h"
#include "buff_runner.h"

using namespace std;
using namespace bufmanager;

Buffer *Buffer::buffer_instance;

int Buffer::buffer_hit = 0;
int Buffer::buffer_miss = 0;
int Buffer::write_count = 0;
int Buffer::read_io = 0;
int Buffer::write_io = 0;

float Buffer::read_lat = 0;
float Buffer::write_lat = 0;

int* Buffer::pages;

Timeval Buffer::start_time;
Timeval Buffer::complete_time;

Queue* Buffer::queue;
Hash* Buffer::hash;

Queue* createQueue(int size);
Hash* createHash(int capacity);
void *alloc_buffer(char direct, int request_size);
float doOneRead(EmuEnv* _env, int pageId);
float doOneWrite(EmuEnv* _env, int pageId);
void tpwrite (int id, EmuEnv* _env, int pageId);
float doKWrite(EmuEnv* _env, int K);



Buffer::Buffer(EmuEnv *_env)
{
  queue = createQueue(_env->buffer_size_in_pages);
  hash = createHash(_env->disk_size_in_pages);
  pages = (int *)malloc(_env->concurrency * sizeof(int));
  buffer_hit = 0;
  buffer_miss = 0;
  read_io = 0;
  write_io = 0;
  write_count = 0;
}

Buffer *Buffer::getBufferInstance(EmuEnv *_env)
{
  if (buffer_instance == 0)
    buffer_instance = new Buffer(_env);
  return buffer_instance;
}

// The queue Node will store the given 'pageId'
QNode* newQNode(unsigned pageId, bool dirty) 
{
  // Allocate memory and assign 'pageId'
  QNode *temp = (QNode *)malloc(sizeof(QNode));
  temp->pageId = pageId;
  temp->prev = temp->next = NULL;   // Initialize prev and next as NULL
  temp->dirty = dirty;
  return temp;
}

// The queue can have at most 'size' nodes 
Queue* createQueue(int size) 
{
  Queue *queue = (Queue *)malloc(sizeof(Queue));
  queue->count = 0;
  queue->front = queue->rear = NULL;
  queue->size = size;
  return queue;
} 

// A utility function to create an empty Hash of given capacity 
Hash* createHash(int capacity) 
{
  Hash *hash = (Hash *)malloc(sizeof(Hash));
  hash->capacity = capacity;
  // Create an array of pointers for refering queue nodes
  hash->array = (QNode **)malloc(hash->capacity * sizeof(QNode *));
  for (int i = 0; i < hash->capacity; ++i)
    hash->array[i] = NULL;  // Initialize all hash entries as empty
  return hash;
}

// A function to check if there is slot available in memory
int AreAllFramesFull(Queue *queue)
{
  return queue->count == queue->size;
}

// A utility function to check if queue is empty
int isQueueEmpty(Queue *queue)
{
  return queue->rear == NULL;
}

// A utility function to delete a frame from queue
void deQueue(Queue *queue)
{
  if (isQueueEmpty(queue))
    return;
  // If this is the only node in list, then change front
  if (queue->front == queue->rear)
    queue->front = NULL;
  // Change rear and remove the previous rear
  QNode *temp = queue->rear;
  queue->rear = queue->rear->prev;
  if (queue->rear)
    queue->rear->next = NULL;
  free(temp);
  // decrement the number of full frames by 1
  queue->count--;
}

void Enqueue(EmuEnv* _env, Queue *queue, Hash *hash, unsigned pageId, bool isWrite, int algorithm)
{
  // If all frames are full, remove the page at the rear
  Buffer* buffer_instance = Buffer::getBufferInstance(_env);
  if (AreAllFramesFull(queue))
  {
    ///**********************************************
    // Only this part has been added for CFLRU
    QNode *iterator_tail = queue->rear;
    int counter = 0;
    while (counter < _env->buffer_size_in_pages / _env->window) // Sending the least recent clean page of the window to the rear of the queue. (To use the 'dequeue' method)
    {
      if (!iterator_tail->dirty)
      {
        if (iterator_tail->next)
          iterator_tail->next->prev = iterator_tail->prev;
        else
          break;
        
        if (iterator_tail->prev)
          iterator_tail->prev->next = iterator_tail->next;

        iterator_tail->prev = queue->rear;
        iterator_tail->next = NULL;
        iterator_tail->prev->next = iterator_tail;

        queue->rear = iterator_tail;
        break;
      }
      else
      {
        if (iterator_tail->prev)
          iterator_tail = iterator_tail->prev;
        else
          break;
      }
      counter++;
    }
    // Only this part has been added for CFLRU
    ///**********************************************

  
    if (algorithm == 1) // Classical CFLRU
    {
      // remove page from hash
      hash->array[queue->rear->pageId] = NULL;
      if (queue->rear->dirty) // Increase write IO if page is dirty. & perform the write
      {
        Buffer::write_io++;
        Buffer::write_count++;
        Buffer::write_lat += doOneWrite(_env, queue->rear->pageId);
        if (_env->verbosity == 2)
          cout << "Write IO: " << Buffer::write_io << endl;
      }
      deQueue(queue);
    }

    else if (algorithm == 2) // CONE-a
    {
      if (queue->rear->dirty)   // Evict multiple pages ONLY if you are about to evict a dirty page
      {
        Buffer::write_io++;   
        if (_env->verbosity == 2)
          cout << "Write IO: " << Buffer::write_io << endl;
        
        Queue* tempq = queue;
        QNode *moving_tail = tempq->rear->prev;
        QNode *temp_node;
        int flag = 0;
        int alpha_window = 1;
        int eligible_for_eviction_count = 1;

        while (alpha_window < _env->concurrency)    // Sending all the dirty pages of the alpha_window to the rear of the queue. (To use the 'dequeue' method)
        {
          if (moving_tail->dirty)
          {
            eligible_for_eviction_count++;
            
            if (moving_tail->prev)    
              temp_node = moving_tail->prev;
            else    // Corner case: if no prev, marking to exit loop
              flag = 1;

            moving_tail->next->prev = moving_tail->prev;
            if (moving_tail->prev)
              moving_tail->prev->next = moving_tail->next;

            if (moving_tail == tempq->front)
            {
              tempq->front = moving_tail->next;
              tempq->front->prev = NULL;
            }
            
            moving_tail->prev = tempq->rear;
            moving_tail->next = NULL;
            
            moving_tail->prev->next = moving_tail;
            
            tempq->rear = moving_tail;
            moving_tail = temp_node;
            if (flag == 1)
              break;
          }
          else
          {
            if (moving_tail->prev)
              moving_tail = moving_tail->prev;
            else
              break;
          }
          alpha_window++;
        }
        queue = tempq;
        
        for (int i = 0; i < eligible_for_eviction_count; i++)
        {
          hash->array[queue->rear->pageId] = NULL;
          Buffer::pages[i] = queue->rear->pageId;
          deQueue(queue);
        }
        if (_env->verbosity == 2)
          cout << "Writing in File! Count: " << eligible_for_eviction_count << endl;
        Buffer::write_lat += doKWrite(_env, eligible_for_eviction_count);
        Buffer::write_count += eligible_for_eviction_count;
      }
      else 
      {
        hash->array[queue->rear->pageId] = NULL;
        deQueue(queue);
      }
    }

    else if (algorithm == 3) // CONE-Xa
    {
      if (queue->rear->dirty) // Evict multiple pages ONLY if you are about to evict a dirty page
      {
        Buffer::write_io++;
        if (_env->verbosity == 2)
          cout << "Write IO: " << Buffer::write_io << endl;

        Queue *tempq = queue;
        QNode *moving_tail = tempq->rear->prev;
        QNode *temp_node;
        int flag = 0;
        int eligible_for_eviction_count = 1;

        while (eligible_for_eviction_count < _env->concurrency) // Sending all the dirty pages in the alpha_window to the rear of the queue. (To use the 'dequeue' method)
        {
          if (moving_tail->dirty)
          {
            eligible_for_eviction_count++;
            
            if (moving_tail->prev)
              temp_node = moving_tail->prev;
            else
              flag = 1;

            moving_tail->next->prev = moving_tail->prev;
            if (moving_tail->prev)
              moving_tail->prev->next = moving_tail->next;

            if (moving_tail == tempq->front)
            {
              tempq->front = moving_tail->next;
              tempq->front->prev = NULL;
            }

            moving_tail->prev = tempq->rear;
            moving_tail->next = NULL;

            moving_tail->prev->next = moving_tail;

            tempq->rear = moving_tail;
            moving_tail = temp_node;
            if (flag == 1)
              break;
          }
          else
          {
            if (moving_tail->prev)
              moving_tail = moving_tail->prev;
            else
              break;
          }
        }
        queue = tempq;

        for (int i = 0; i < eligible_for_eviction_count; i++)
        {
          hash->array[queue->rear->pageId] = NULL;
          Buffer::pages[i] = queue->rear->pageId;
          deQueue(queue);
        }
        if (_env->verbosity == 2)
          cout << "Writing in File! Count: " << eligible_for_eviction_count << endl;
        Buffer::write_lat += doKWrite(_env, eligible_for_eviction_count);
        Buffer::write_count += eligible_for_eviction_count;
      }
      else
      {
        hash->array[queue->rear->pageId] = NULL;
        deQueue(queue);
      }
    }

    else if (algorithm == 4) // COW-a
    {
      // remove page from hash
      hash->array[queue->rear->pageId] = NULL;
      // Increase write IO if page is dirty. & perform the write
      if (queue->rear->dirty)
      {
        Buffer::write_io++;
        if (_env->verbosity == 2)
          cout << "Write IO: " << Buffer::write_io << endl;

        QNode *moving_tail = queue->rear->prev;
        int alpha_window = 1;
        int eligible_for_write_count = 1;
        Buffer::pages[0] = queue->rear->pageId;
      
        while (alpha_window < _env->concurrency)    
        {
          if (moving_tail->dirty)
          {
            Buffer::pages[eligible_for_write_count] = moving_tail->pageId;
            eligible_for_write_count++;
            moving_tail->dirty = false;
          }
          
          if (moving_tail->prev)
            moving_tail = moving_tail->prev;
          else
            break;
          alpha_window++;
        }

        if (_env->verbosity == 2)
          cout << "Writing in File! Count: " << eligible_for_write_count << endl;
        Buffer::write_lat += doKWrite(_env, eligible_for_write_count);
        Buffer::write_count += eligible_for_write_count;
      }
      
      deQueue(queue);
    }

    else if (algorithm == 5) // COW-Xa
    {
      // remove page from hash
      hash->array[queue->rear->pageId] = NULL;
      // Increase write IO if page is dirty. & perform the write
      if (queue->rear->dirty)
      {
        Buffer::write_io++;
        if (_env->verbosity == 2)
          cout << "Write IO: " << Buffer::write_io << endl;

        QNode *moving_tail = queue->rear->prev;
        int eligible_for_write_count = 1;
        Buffer::pages[0] = queue->rear->pageId;

        while (eligible_for_write_count < _env->concurrency)    
        {
          if (moving_tail->dirty)
          {
            Buffer::pages[eligible_for_write_count] = moving_tail->pageId;
            eligible_for_write_count++;
            moving_tail->dirty = false;
          }
          
          if (moving_tail->prev)
            moving_tail = moving_tail->prev;
          else
            break;
        }
        
        if (_env->verbosity == 2)
          cout << "Writing in File! Count: " << eligible_for_write_count << endl;
        Buffer::write_lat += doKWrite(_env, eligible_for_write_count);
        Buffer::write_count += eligible_for_write_count;
      }
      deQueue(queue);
    }
  }

  // Create a new node with given page number and add the new node to the front of queue
  QNode *temp = newQNode(pageId, isWrite);
  temp->next = queue->front;

  // If queue is empty, change both front and rear pointers
  if (isQueueEmpty(queue))
    queue->rear = queue->front = temp;
  else // Else change the front
  {
    queue->front->prev = temp;
    queue->front = temp;
  }
  // Add page entry to hash also
  hash->array[pageId] = temp;
  // increment number of full frames
  queue->count++;
}

void ReferencePage(Queue *queue, Hash *hash, unsigned pageId, bool isWrite, int algorithm)
{
  QNode *reqPage = hash->array[pageId];
  EmuEnv* _env = EmuEnv::getInstance();
  // the page is not in cache, bring it
  if (reqPage == NULL)
  {
    Enqueue(_env, queue, hash, pageId, isWrite, algorithm);
    Buffer::buffer_miss++;
    Buffer::read_io++;
    Buffer::read_lat += doOneRead(_env, pageId);
    if (_env->verbosity == 2)
    {
      cout << "Miss! Buffer Hit: " << Buffer::buffer_hit << ". Buffer Miss: " << Buffer::buffer_miss << endl;
      cout << "Read IO: " << Buffer::read_io << endl;
    }
  }
  // page is there and not at front, change pointer
  else if (reqPage != queue->front)
  {
    Buffer::buffer_hit++;

    if (_env->verbosity == 2)
    {
      cout << "Hit! Buffer Hit: " << Buffer::buffer_hit << ". Buffer Miss: " << Buffer::buffer_miss << endl;
    }
    // Unlink requested page from its current location in queue.
    reqPage->prev->next = reqPage->next;
    if (reqPage->next)
      reqPage->next->prev = reqPage->prev;

    // If the requested page is rear, then change rear as this node will be moved to front
    if (reqPage == queue->rear)
    {
      queue->rear = reqPage->prev;
      queue->rear->next = NULL;
    }
    // Put the requested page before current front
    reqPage->next = queue->front;
    reqPage->prev = NULL;
    // Change prev of current front
    reqPage->next->prev = reqPage;
    // Change front to the requested page
    queue->front = reqPage;
  }
  else
  {
    Buffer::buffer_hit++;
    if (_env->verbosity == 2)
    {
      cout << "Hit! Buffer Hit: " << Buffer::buffer_hit << ". Buffer Miss: " << Buffer::buffer_miss << endl;
    }
  }
  if (isWrite)
    queue->front->dirty = isWrite;  //change the dirty bit
}

int WorkloadExecutor::read(Buffer* buffer_instance, int pageId, int algorithm)
{
  bool isWrite = false;
  ReferencePage(buffer_instance->queue, buffer_instance->hash, pageId, isWrite, algorithm);
}

int WorkloadExecutor::write(Buffer* buffer_instance, int pageId, int algorithm)
{
  bool isWrite = true;
  ReferencePage(buffer_instance->queue, buffer_instance->hash, pageId, isWrite, algorithm);
}

int WorkloadExecutor::unpin(Buffer* buffer_instance, int pageId)
{

}

int Buffer::LRU()
{

}

long Buffer::time_diff(Timeval end, Timeval start)
{
  long seconds;
  long micro_seconds;
  long diff_micro_seconds;
  seconds = end.tv_sec - start.tv_sec;
  micro_seconds = end.tv_usec - start.tv_usec;
  diff_micro_seconds = seconds * 1000000 + micro_seconds;

  return diff_micro_seconds;
}

float doOneRead(EmuEnv* _env, int pageId)
{
  void *buffer;
  off_t offset, max_offset;
  offset = pageId * _env->request_size;
  lseek(_env->fd, offset, SEEK_SET);
  buffer = alloc_buffer('T', _env->request_size);

  gettimeofday(&(Buffer::start_time), NULL);
  int n = read(_env->fd, buffer, _env->request_size);
  gettimeofday(&(Buffer::complete_time), NULL);

  free(buffer);

  if (n != _env->request_size)
  {
    // cout << n << endl;
    cout << "Error in doOneRead" << endl;
    exit(1);
  }
  return Buffer::time_diff(Buffer::complete_time, Buffer::start_time);
}

float doOneWrite(EmuEnv* _env, int pageId)
{
  void *buffer;
  off_t offset;
  offset = pageId * _env->request_size;
  lseek(_env->fd, offset, SEEK_SET);
  buffer = alloc_buffer('T', _env->request_size);
  gettimeofday(&(Buffer::start_time), NULL);
  int n = write(_env->fd, buffer, _env->request_size);
  gettimeofday(&(Buffer::complete_time), NULL);
  free(buffer);
  if (n != _env->request_size)
  {
    // cout << n << endl;
    cout << "Error in doOneWrite" << endl;
    exit(1);
  }
  return Buffer::time_diff(Buffer::complete_time, Buffer::start_time);
}

void tpwrite (int id, EmuEnv* _env, int pageId)
{
  void *buffer;
  off_t offset, max_offset;

  offset = pageId * _env->request_size;
  lseek(_env->fd, offset, SEEK_SET);
  buffer = alloc_buffer('T', _env->request_size);
  int n = write(_env->fd, buffer, _env->request_size);
  free(buffer);
  if (n != _env->request_size)
  {
    // cout << n << endl;
    cout << "Error in doKWrite. Id: " << id << endl;
    exit(1);
  }
}

float doKWrite(EmuEnv* _env, int K)
{
  if (_env->verbosity == 2)
  {
    cout << "Pages to be written concurrently: ";
    for (int i = 0; i < K; i++)
    {
      cout << Buffer::pages[i] << " ";
    }
    cout << endl;
  }

  Timeval start_time, complete_time;
  // _env->tp.resize(K);

  gettimeofday(&(start_time), NULL);
  for (int i = 0; i < K; i++)
  {
    _env->tp.push(tpwrite, _env, Buffer::pages[i]);
  }
  while (1)
  {
    if (_env->tp.n_idle() == _env->concurrency)
    {
      gettimeofday(&(complete_time), NULL);
      break;
    }
  }
  return Buffer::time_diff(complete_time, start_time);
}

void *alloc_buffer(char direct, int request_size)
{
  int pagesize;
  void *buffer;

  if (direct == 'F')
  {
    buffer = malloc(request_size + pagesize);
    return buffer;
  }
  pagesize = getpagesize();
  posix_memalign(&buffer, pagesize, request_size);
  return buffer;
}

int Buffer::printBuffer(EmuEnv *_env, Buffer* buffer_instance)
{
  Queue* tempq = buffer_instance->queue;
  QNode* moving_head = tempq->front;
  QNode* dirty_iterator = tempq->front;

  cout << "Pages in buffer: " << endl;
  while (moving_head)
  {
    cout << moving_head->pageId << "\t" ;
    moving_head = moving_head->next;
  }
  cout << endl;

  while (dirty_iterator)
  {
    if (dirty_iterator->dirty)
      std::cout << "\033[1;31m1\t"<< "\033[0m";
    else
      std::cout << "\033[1;32m-1\t"<< "\033[0m";
    dirty_iterator = dirty_iterator->next;
  }
  cout << endl;

}

int Buffer::printStats()
{
  EmuEnv* _env = EmuEnv::getInstance();
  cout << "******************************************************" << endl;
  cout << "Printing Stats..." << endl;
  cout << "Number of operations: " << _env->num_operations << endl;
  cout << "Buffer Hit: " << buffer_hit << endl;
  cout << "Buffer Miss: " << buffer_miss << endl;
  cout << "Read IO: " << read_io << endl;
  cout << "Write IO: " << write_io << endl;
  cout << "Actual Write Count: " << write_count << endl;
  cout << "Read Cost: " << read_io * _env->read_cost << endl;
  cout << "Write Cost: " << write_io * _env->write_cost << endl;
  cout << "Total Cost: " << read_io * _env->read_cost + write_io * _env->write_cost << endl;
  cout << "Execution Time (ms): " << read_lat/1000 + write_lat/1000 << endl;
  //cout << "Global Clock: " << clock << endl;
  cout << "******************************************************" << endl;

  // fstream fout1, fout2,fout3;
  // fout1.open("out1.txt", ios::out);
  // fout1 << (read_io * _env->read_cost + write_io * _env->write_cost);
  // fout1.close();
  // fout2.open("out2.txt", ios::out);
  // fout2 << buffer_miss;
  // fout2.close();
  // fout3.open("out3.txt", ios::out);
  // fout3 << write_count;
  // fout3.close();
}