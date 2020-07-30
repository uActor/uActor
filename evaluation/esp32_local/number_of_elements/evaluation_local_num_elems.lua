MAX_COUNT = 8192

function receive(message)

  if(message.type == "ping") then
    testbed_stop_timekeeping(1, "latency")
    if(iteration % 25 == 0) then
      delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "setup"), 1000 + math.random(0, 199))
    else
      delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"), 1000 + math.random(0, 199))
    end
  end

  if(message.type == "init") then
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    iteration = 0
    count = -32
  end

  if(message.type == "init" or message.type == "setup") then

    local publication
    local buffer

    iteration = 0
    count = count + 32
    if(count > MAX_COUNT) then
      testbed_log_string("done" , "true")
      return
    end

    testbed_log_string("_logger_test_postfix", tostring(count))
    
    collectgarbage()
    delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"), 1000 + math.random(0, 199))
  end

  if(message.type == "trigger") then
    
    iteration = iteration + 1

    
    local publication = Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "ping")
    
    testbed_start_timekeeping(7)
    
    for x=1,count,1 do
      if(x % 7 == 0) then
        publication["dummy_int_"..tostring(x)] = x
      elseif(x % 8 == 0) then
        publication["dummy_float_"..tostring(x)] = 0.1*x
      else
        publication["dummy_"..tostring(x)] = "ABCD"
      end
    end
    
    testbed_stop_timekeeping(7, "create")


    collectgarbage()
    testbed_start_timekeeping(1)
    publish(publication)
  end
end