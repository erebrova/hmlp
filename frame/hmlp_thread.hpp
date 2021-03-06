/**
 *  HMLP (High-Performance Machine Learning Primitives)
 *  
 *  Copyright (C) 2014-2017, The University of Texas at Austin
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see the LICENSE file.
 *
 **/  






#ifndef HMLP_THREAD_HPP
#define HMLP_THREAD_HPP

#include <string>
#include <stdio.h>
#include <iostream>
#include <cstddef>
#include <cassert>
#include <map>
#include <set>
#include <omp.h>

#include <hmlp_device.hpp>


namespace hmlp
{


//typedef enum 
//{
//  HOST,
//  NVIDIA_GPU,
//  OTHER_GPU,
//  TI_DSP
//} DeviceType;


class thread_communicator 
{
	public:
    
    thread_communicator();

	  thread_communicator( int jc_nt, int pc_nt, int ic_nt, int jr_nt );

    void Create( int level, int num_threads, int *config );

    void Barrier();

    void Print();

    int GetNumThreads();

    int GetNumGroups();

    friend std::ostream& operator<<( std::ostream& os, const thread_communicator& obj );

    thread_communicator *kids;

    std::string name;

	private:

	  void          *sent_object;

    int           comm_id;

	  int           n_threads;

    int           n_groups;

	  volatile bool barrier_sense;

	  int           barrier_threads_arrived;

}; /** end class thread_communicator */




/**
 *
 *
 **/ 
class Worker 
{
  public:

    Worker();

	  //worker( int jc_nt, int pc_nt, int ic_nt, int jr_nt );
   
    Worker( thread_communicator *my_comm );

    void Communicator( thread_communicator *comm );

    void SetDevice( class Device *device );

    class Device *GetDevice();

    bool Execute( class Task *task );

    void WaitExecute();

    float EstimateCost( class Task* task );

    class Scheduler *scheduler;

#ifdef USE_PTHREAD_RUNTIME
    pthread_t pthreadid;
#endif

    int tid;

    int jc_id;

    int pc_id;

    int ic_id;

    int jr_id;

    int ic_jr;

    int jc_nt;

    int pc_nt;

    int ic_nt;

    int jr_nt;

    thread_communicator *my_comm;

    thread_communicator *jc_comm;

    thread_communicator *pc_comm;

    thread_communicator *ic_comm;

  private:

    class Task *current_task;

    class Device *device;

}; /** end class Worker */

}; /** end namespace hmlp */

#endif /** end define HMLP_THREAD_HPP */
