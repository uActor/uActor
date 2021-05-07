function receive(message)
  if(message.type == "ping") then
    count = count + 1
    if(count == 100000) then
      testbed_stop_timekeeping(1, "time_for_100000")
      testbed_start_timekeeping(1)
      count = 0
    end
    publish(Publication.new("type", "ping"))
  elseif(message.type == "init") then
    subscribe({type="ping"})
    count = 0
    testbed_start_timekeeping(1)
    publish(Publication.new("type", "ping"))
  end
end