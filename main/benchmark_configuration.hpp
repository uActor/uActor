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
#define COND_VAR true

#endif  // MAIN_BENCHMARK_CONFIGURATION_HPP_
