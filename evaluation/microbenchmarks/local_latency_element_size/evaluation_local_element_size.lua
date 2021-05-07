MAX_SIZE = 131072

function receive(message)

  if(message.foo == "bar") then
    testbed_stop_timekeeping(1, "latency")
    if(iteration % 5 == 0) then
      enqueue_wakeup(1000 + math.random(0, 199), "setup")
    else
      enqueue_wakeup(1000 + math.random(0, 199), "trigger")
    end
  end

  if(message.type == "init") then
    subscribe({foo="bar", baz="qux"})
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    iteration = 0
    size = -256
  end


  if(message.type == "init" or (message.type == "wakeup" and message.wakeup_id == "setup")) then
    iteration = 0
    size = size + 256
    if(size > MAX_SIZE) then
      testbed_log_string("done" , "true")
      return
    end
    testbed_log_string("_logger_test_postfix", size)

    collectgarbage()
    enqueue_wakeup(1000 + math.random(0, 199), "trigger")
  end

  if(message.type == "wakeup" and message.wakeup_id == "trigger") then
    
    iteration = iteration + 1

    local publication = Publication.new("foo", "bar", "baz", "qux")
  
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