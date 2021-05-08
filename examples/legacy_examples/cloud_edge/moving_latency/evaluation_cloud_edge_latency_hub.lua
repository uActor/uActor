function receive(message)
  if(message.type == "ping1") then
    publish(Publication.new("type", "ping2"))
  end
  if(message.type == "pong2") then
    publish(Publication.new("type", "pong1"))
  end
  if(message.type == "init") then
    subscribe{type="ping1"}
    subscribe{type="pong2"}
  end
end