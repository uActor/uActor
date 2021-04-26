function receive(message)
  if(message.type == "init" or (message.type == "wakeup" and message.wakeup_id == "ping_trigger")) then

    if(message.type == "init") then
      print("Start ping actor")
      sequence_number = 0  
    else
      sequence_number = sequence_number + 1
    end

    publish(Publication.new(
      "type", "ping",
      "sequence_number", sequence_number
    ))
    print("ping sent: "..sequence_number) 

    enqueue_wakeup(5000, "ping_trigger")
  elseif(message.type == "exit") then
    print("Stop ping actor")
  elseif(message.type == "pong") then
    print("pong received: "..message.sequence_number)
  end
end