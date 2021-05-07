MAX_COUNT = 8192

function receive(message)

  if(message.foo == "bar") then
    testbed_stop_timekeeping(1, "latency")
    if(iteration % 25 == 0) then
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
    count = -128
  end

  if(message.type == "init" or (message.type == "wakeup" and message.wakeup_id == "setup")) then

    local publication
    local buffer

    iteration = 0
    count = count + 128
    if(count > MAX_COUNT) then
      testbed_log_string("done" , "true")
      return
    end

    testbed_log_string("_logger_test_postfix", tostring(count))
    
    collectgarbage()
    enqueue_wakeup(1000 + math.random(0, 199), "trigger")
  end

  if(message.type == "wakeup" and message.wakeup_id == "trigger") then
    
    iteration = iteration + 1

    local publication = Publication.new("foo", "bar", "baz", "qux")
    
    for x=1,count,1 do
        publication["dummy_"..tostring(x)] = "ABCD"
    end


    collectgarbage()
    testbed_start_timekeeping(1)
    publish(publication)
  end
end