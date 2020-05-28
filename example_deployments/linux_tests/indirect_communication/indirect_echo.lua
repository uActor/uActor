function receive(message)
  if(message.type == "ping2") then
    publish({type="pong2"})
  end
  if(message.type == "init") then
    subscribe{type="ping2"}
  end
end