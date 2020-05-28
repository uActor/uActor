function receive(message)
  
  if(message.type == "init") then
    subscribe({type="iteration_done"})
    iteration = 0
    done = false
    additional_size = 4096
    delay = 1300
    testbed_log_string("_logger_test_postfix", delay)
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 1000 + math.random(0, 199))
  end
  if(message.type == "trigger") then
    iteration = iteration + 1
    if(iteration == 501) then
      iteration = 0
      delay = delay - 50
      if(delay < 0) then
        done = true
      end
      return
    end
    publication = {type="fake_sensor_value", value=0.1*(math.random(0, 400))}
    if (additional_size > 0) then
      local elem_256 = "A"
      for x=1,8 do
        elem_256 = elem_256 .. elem_256
      end
      local buffer = ""
      for x=1, additional_size / 256 do
        buffer = buffer .. elem_256
      end
      -- publication["payload"] = buffer
    end
    collectgarbage()
    
    publish(publication)
    testbed_stop_timekeeping(1, "publish")
    testbed_start_timekeeping(1)
    publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"})
  end
  if(message.type == "iteration_done") then
    if(not done) then
      testbed_log_string("_logger_test_postfix", delay)
      delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 5000)
    else
      testbed_log_string("done", 1)
    end
  end
end