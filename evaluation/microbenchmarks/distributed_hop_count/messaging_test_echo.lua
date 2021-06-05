function receive(message)
  if(message.exp_1 == "ping") then
    message["node_id"] = message.publisher_node_id
    message["actor_type"] = message.publisher_actor_type
    message["instance_id"] = message.publisher_instance_id
    message["publisher_node_id"] = nil
    message["publisher_actor_type"] = nil
    message["publisher_instance_id"] = nil
    message["_internal_sequence_number"] = nil
    message["_internal_epoch_number"] = nil
    message["_internal_forwarded_by"] = nil
    message["exp_1"] = "pong"
    publish(message)
  elseif(message.type == "init") then
    subscribe{exp_1="ping", node_id=node_id}
  end
end