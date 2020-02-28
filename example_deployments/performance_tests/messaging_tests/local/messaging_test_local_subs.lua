MAX_SUB_COUNT = 208

function receive(message)

  if(message.type == "ping") then
    testbed_stop_timekeeping(1, iteration_name)
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 1000 + math.random(0, 199))
  end

  if(message.type == "init") then
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    iteration = 0
    count = - 1
  end

  if(message.type == "init" or message.type == "trigger") then

    local publication

    if(iteration % 10 == 0) then
      iteration = 0
      
      if(count == -1) then
        count = 0
      else
        count = count + 16
      end

      if(count > MAX_SUB_COUNT) then
        testbed_log_string("done" , "true")
        return
      end

      for i=1,16 do
        if(i % 4 == 0) then
          subscribe({"type", "value"..(count-16+i)})
        else
          subscribe({"key_"..i, "value"..(count-16+i)})
        end
      end

      iteration_name = "latency_subs_"..tostring(count)
      publish_iteration_name = "publish_subs_"..tostring(count)
    end

    publication = {node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="ping"}

    iteration = iteration + 1

    collectgarbage()
    testbed_start_timekeeping(1)
    
    -- testbed_start_timekeeping(3) -- breakdown measurement
    publish(publication)
    -- testbed_stop_timekeeping(3, publish_iteration_name) -- breakdown measurement
  end
end