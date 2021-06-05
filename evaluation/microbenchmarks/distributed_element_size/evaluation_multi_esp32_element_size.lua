MAX_SIZE = 30720

function receive(message)

  if(message.exp_1 == "pong") then
    testbed_stop_timekeeping(1, "latency")
    if(iteration % 50 == 0) then
      enqueue_wakeup(1000 + math.random(0, 199), "setup")
    else
      enqueue_wakeup(1000 + math.random(0, 199), "trigger")
    end
  end

  if(message.type == "init") then
    -- Return path uses the unique default subscription; No explicit subscription is need.
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    size = -1024
    iteration = 0
  end
  
  if(message.type == "init" or (message.type == "wakeup" and message.wakeup_id == "setup")) then

    iteration = 0
    size = size + 1024
    if(size > MAX_SIZE) then
      testbed_log_string("done" , "true")
      return
    end
    testbed_log_string("_logger_test_postfix", size)

    collectgarbage()
    enqueue_wakeup(10000 + math.random(0, 199), "trigger")
  end

  if(message.type == "wakeup" and message.wakeup_id == "trigger") then

    iteration = iteration + 1
    
    local publication = Publication.new("exp_1", "ping", "node_id", PEER_ID)
    
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