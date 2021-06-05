function receive(message)
  if(message.exp_1 == "ping") then
    count = count + 1
    if(count == 10000) then
      testbed_stop_timekeeping(1, "time_for_10000")
      testbed_start_timekeeping(1)
      count = 0
    end
    publish(Publication.new("exp_1", "ping"))
  elseif(message.type == "init") then
    subscribe({exp_1="ping"})
    count = 0
    collectgarbage()
    testbed_start_timekeeping(1)
    publish(Publication.new("exp_1", "ping"))
  end
end