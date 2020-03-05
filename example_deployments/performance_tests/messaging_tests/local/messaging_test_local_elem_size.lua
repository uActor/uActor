MAX_SIZE = 65536

function receive(message)

  if(message.type == "ping") then
    testbed_stop_timekeeping(1, "latency")
    if(iteration % 10 == 0) then
      delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="setup"}, 1000 + math.random(0, 199))
    else
      delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 1000 + math.random(0, 199))
    end
  end

  if(message.type == "init") then
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    iteration = 0
    size = -156
  end


  if(message.type == "init" or message.type == "setup") then
    iteration = 0
    size = size + 256
    if(size > MAX_SIZE) then
      testbed_log_string("done" , "true")
      return
    end
    testbed_log_string("_logger_test_postfix", size)

    collectgarbage()
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 1000 + math.random(0, 199))
  end

  if(message.type == "trigger") then
    
    iteration = iteration + 1

    local publication = {node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="ping"}
  
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