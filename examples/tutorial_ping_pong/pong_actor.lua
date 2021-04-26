function receive(message)
  if(message.type == "init") then
    print("Start pong actor")

    subscribe({type="ping"})

  end
  if(message.type == "exit") then
    print("Stop pong actor")
  end
  if(message.type == "ping") then
    print("ping received: "..pub.sequence_number)
    publish(Publication.new(
      -- Assume sender default subscription is present and sufficient
      -- TODO(raphaelhetzel): Implement generic reply and demonstrate it here
      "node_id", message.publisher_node_id,
      "actor_type", message.publisher_actor_type,
      "instance_id", message.publisher_instance_id,
      "type", "pong",
      "sequence_number", message.sequence_number
    ))
  end
end