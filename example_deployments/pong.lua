function receive(message)
  if(message.type == "init") then
    subscribe({message="ping"})
  end
  if(message.message == "ping") then
    print("ping")
    send({node_id=message.sender_node_id, actor_type=message.sender_actor_type, instance_id=message.sender_instance_id, message="pong"})
  end
end