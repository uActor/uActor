function receive(message)
  if(message.type == "init" or message.type == "delayed_publish") then
    send({node_id=node_id, actor_type="xyz.hetzel.pong", message="ping"})
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="delayed_publish"}, 1000)
  else
    print("pong")
  end
end