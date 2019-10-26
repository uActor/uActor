#ifndef MAIN_BENCHMARK_CONFIGURATION_HPP_
#define MAIN_BENCHMARK_CONFIGURATION_HPP_

#define STATIC_MESSAGE_SIZE 1500

// Write out the id to the task in 8 byte
// steps to touch the data in some way
#define TOUCH_DATA true

// TRUE: Message copied to the queue FALSE: Reference copied to the queue
#define BY_VALUE false

#define QUEUE_SIZE 1

// TRUE: Use an implementation of the router that
// is based on c++ condition variables
// FALSE: Router based on FreeRTOS queues
// Won't work with multiple actors per thread or by value semantics
#define COND_VAR false

#define LUA_ACTORS true

// Touch the data array bytewise, which is not efficient for Lua
#define TOUCH_BYTEWISE false

#define NUM_THREADS 16

#define ACTORS_PER_THREAD 1

#define NUM_ACTORS NUM_THREADS* ACTORS_PER_THREAD

#endif  // MAIN_BENCHMARK_CONFIGURATION_HPP_
