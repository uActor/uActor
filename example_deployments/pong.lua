function receive(message)
  if(message.type == "init") then
    subscribe({message="ping"})
  end
  if(message.message == "ping") then
    print("ping")
    publish({node_id=message.publisher_node_id, actor_type=message.publisher_actor_type, instance_id=message.publisher_instance_id, message="pong"})
  end
end