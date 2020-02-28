function receive(message)
  if(message.type == "ping") then
    publish({node_id=message.publisher_node_id, actor_type=message.publisher_actor_type, instance_id=message.publisher_instance_id, type="pong", payload=message.payload})
  end
end