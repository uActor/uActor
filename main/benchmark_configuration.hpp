#ifndef MAIN_BENCHMARK_CONFIGURATION_HPP_
#define MAIN_BENCHMARK_CONFIGURATION_HPP_

#define BENCHMARK false

#define STATIC_MESSAGE_SIZE 1500

// Write out the id to the task in 8 byte
// steps to touch the data in some way
#define TOUCH_DATA true

// Touch the data array bytewise, which is not efficient for Lua
#define TOUCH_BYTEWISE false

// TRUE: Message copied to the queue FALSE: Reference copied to the queue
#define BY_VALUE false

#define QUEUE_SIZE 100

// TRUE: Use an implementation of the router that
// is based on c++ condition variables
// FALSE: Router based on FreeRTOS queues
// Won't work with multiple actors per thread or by value semantics
#define COND_VAR false

#define LUA_ACTORS true

#define LUA_SHARED_RUNTIME true

#define LUA_PERSISTENT_ACTORS true

// Can only be set to false when both SHARED_RUNTIME and PERSISTENT_ACTORS are
// set to false!
#define LUA_PERSISTENT_RUNTIME true

#define NUM_THREADS 1

#define ACTORS_PER_THREAD 32

#define NUM_ACTORS NUM_THREADS* ACTORS_PER_THREAD

#endif  // MAIN_BENCHMARK_CONFIGURATION_HPP_
