function receive(message)
  if(message.type == "ping1") then
    publish(Publication.new("type", "pong1"))
  end
  if(message.type == "init") then
    subscribe{type="ping1"}
  end
end