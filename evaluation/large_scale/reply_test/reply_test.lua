NUM_NODES = 256
MAX_ITERATIONS = 25
HOST_NODE = "node_1"

function receive(message)
  if(message.type == "init") then
    print("init")
    iteration = 0
    delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "ready"), 10000)
    -- testbed_log_string("_logger_test_postfix", "default")
  
  elseif(message.type == "ready") then
    print("ready "..instance_id)
    count = 0
    iteration  = iteration + 1
    if(iteration > MAX_ITERATIONS) then
      testbed_log_string("done" , "true")
      return
    end
    collectgarbage()
    start_time_sec, start_time_nsec = unix_timestamp()
    reply_times = {}
    receive_times = {}

    testbed_start_timekeeping(1)
    publish(Publication.new("type", "ping"))
  elseif(message.type == "pong") then
    local time_sec, time_nsec = unix_timestamp()
    
    reply_times[message.publisher_node_id] = (time_sec - start_time_sec) *1000*1000 + (time_nsec - start_time_nsec) // 1000
    if(message.received_sec >= start_time_sec) then
      time_diff = (message.received_sec - start_time_sec) *1000*1000 + (message.received_nsec - start_time_nsec) // 1000
      receive_times[message.publisher_node_id] = time_diff
    end

    count = count + 1
    print(count)
    if(count == NUM_NODES) then
      for peer_node_id, time in pairs(reply_times) do 
        testbed_log_integer("time_"..peer_node_id, time)
        testbed_log_integer("receive_"..peer_node_id, receive_times[peer_node_id])
      end
      testbed_stop_timekeeping(1, "total")
      count = 0
      delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "ready"), 10000) 
    end
  end
end
