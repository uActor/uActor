MAX_PEER_ID = 1365

function receive(message)

  if(message.type == "pong") then
    -- testbed_stop_timekeeping(1, "latency_"..receiver_id)
    reply_time = calculate_time_diff(start_time_sec, start_time_nsec)
    receive_time = calculate_time_diff(message.received_sec, message.received_nsec)
    
    if(message.received_sec >= start_time_sec) then
      testbed_log_integer("time_"..receiver_id, reply_time)
      testbed_log_integer("receive_"..receiver_id, receive_time)
    end

    if(iteration % 25 == 0) then
      delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "setup"), math.random(0, 199))
    else
      delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"), math.random(0, 199))
    end
  end

  if(message.type == "init") then
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    peer_id = 0
    iteration = 0
  end

  if(message["_internal_timeout"] == "_timeout") then
    print("Timeout - retrying")
    iteration = iteration -1
    delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"), math.random(0, 199)) 
  end
  
  if(message.type == "init" or message.type == "setup") then

    iteration = 0
    peer_id = peer_id + 1
    receiver_id = "node_"..tostring(peer_id)

    
    if(peer_id > MAX_PEER_ID) then
      return
    end

    testbed_log_string("_logger_test_postfix", peer_id)
    collectgarbage()
    delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"), math.random(0, 199))
  end

  if(message.type == "trigger") then
    iteration = iteration + 1

    deferred_block_for({type="pong"}, 30000)
    collectgarbage()
    -- testbed_start_timekeeping(1)
    start_time_sec, start_time_nsec = unix_timestamp()
    publish(Publication.new("node_id", receiver_id, "type", "ping"))
  end
end