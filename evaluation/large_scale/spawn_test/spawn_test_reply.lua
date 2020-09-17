NUM_NODES = 256
MAX_ITERATIONS = 25
HOST_NODE = "node_1"

function receive(message)
  code = [[function receive(message)
    if(message.type == "init") then
      print("called spawned actor")
      received_sec, received_nsec = unix_timestamp()
      publish(Publication.new("node_id", "]]..HOST_NODE..[[", "actor_type", "spawn_test_host", "instance_id", "spawn_test_deployment", "type", "pong", "received_sec", received_sec, "received_nsec", received_nsec))
    end
  end]]
  if(message.type == "init" and node_id == HOST_NODE) then
    -- wait until the subscription for the backwards pass is forwarded to all nodes 
    iteration = 0
    delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "ready"), 5000)
    testbed_log_string("_logger_test_postfix", "default")
  elseif(message.type == "ready") then
    count = 0
    iteration  = iteration + 1
    if(iteration > MAX_ITERATIONS) then
      testbed_log_string("done" , "true")
      return
    end
    collectgarbage()

    --- breakdown
    start_time_sec, start_time_nsec = unix_timestamp()
    reply_times = {}
    receive_times = {}

    print("published new deployment") 
    testbed_start_timekeeping(1)
    publish(Publication.new(
      "type", "deployment",
      "deployment_name", "benchmark.spawn_deployment",
      "deployment_actor_type", "benchmark.spawn_actor",
      "deployment_actor_runtime_type", "lua",
      "deployment_actor_version", "0.1",
      "deployment_actor_code", code,
      "deployment_required_actors", "",
      "deployment_ttl", 10000
    ))
  elseif(node_id == HOST_NODE and message.type == "pong") then
    local time_sec, time_nsec = unix_timestamp()
    --- breakdown
    --- testbed_stop_timekeeping(1, "time_"..message.publisher_node_id)
    
    reply_times[message.publisher_node_id] = (time_sec - start_time_sec) *1000*1000 + (time_nsec - start_time_nsec) // 1000
    --- estimate the time it takes for the actor to receive the init message
    if(message.received_sec >= start_time_sec) then
      time_diff = (message.received_sec - start_time_sec) *1000*1000 + (message.received_nsec - start_time_nsec) // 1000
      receive_times[message.publisher_node_id] = time_diff
    end

    count = count + 1
    -- print(message.publisher_node_id.." - "..count)
    if(count == NUM_NODES) then
      for peer_node_id, time in pairs(reply_times) do 
        testbed_log_integer("time_"..peer_node_id, time)
        testbed_log_integer("receive_"..peer_node_id, receive_times[peer_node_id])
      end
      testbed_stop_timekeeping(1, "total")
      count = 0
      delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "ready"), 20000) 
    end
  end
end
