PEER_ID = "node_5"
MAX_SIZE = 65536

function receive(message)

  if(message.type == "pong") then
    testbed_stop_timekeeping(1, "latency")
    if(iteration % 50 == 0) then
      publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "setup"))
    else
      delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"), 1000 + math.random(0, 199))
    end
  end

  if(message.type == "init") then
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    size = -1024
    iteration = 0
  end
  
  if(message.type == "init" or message.type == "setup") then

    iteration = 0
    size = size + 1024
    if(size > MAX_SIZE) then
      testbed_log_string("done" , "true")
      return
    end
    testbed_log_string("_logger_test_postfix", size)

    collectgarbage()
    delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"), 10000 + math.random(0, 199))
  end

  if(message.type == "trigger") then
    
    iteration = iteration + 1
    
    local publication = Publication.new("node_id", PEER_ID, "actor_type", "test_echo_actor", "instance_id", "test_deployment_echo", "type", "ping")
    
    if (size > 0) then
      local elem_256 = "A"
      for x=1,8 do
        elem_256 = elem_256 .. elem_256
      end
      local buffer = ""
      for x=1, size / 256 do
        buffer = buffer .. elem_256
      end
      publication["payload"] = buffer
    end


    collectgarbage()
    testbed_start_timekeeping(1)
    publish(publication)
  end
end