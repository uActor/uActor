MAX_SUB_COUNT = 400

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
    count = - 1
  end

  if(message.type == "init" or message.type == "setup") then

    local publication


    iteration = 0
    
    if(count == -1) then
      count = 0
    else
      count = count + 4
    end

    if(count > MAX_SUB_COUNT) then
      testbed_log_string("done" , "true")
      return
    end

    if count > 0 then
      subscribe({"type", "value"..count})
      subscribe({"not_type"..count, "value"..count})
      subscribe({"not_type_int"..count, count})
      subscribe({"not_type_float"..count, 3.14*count})
    end

    testbed_log_string("_logger_test_postfix", count)

    collectgarbage()
    delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"), 1000 + math.random(0, 199))
  end

  if(message.type == "trigger") then
    iteration = iteration + 1

    local publication = Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "ping")

    -- for x=1,64,1 do
    --   if(x % 7 == 0) then
    --     publication["dummy_int_"..tostring(x)] = x
    --   elseif(x % 8 == 0) then
    --     publication["dummy_float_"..tostring(x)] = 0.1*x
    --   else
    --     publication["dummy_"..tostring(x)] = "ABCD"
    --   end
    -- end
    
    collectgarbage()
    testbed_start_timekeeping(1)
    
    publish(publication)
  end
end