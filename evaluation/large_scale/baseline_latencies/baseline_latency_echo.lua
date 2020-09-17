function receive(message)
  if(message.type == "ping") then
    received_sec, received_nsec = unix_timestamp()
    publish(
      Publication.new(
        "node_id", message.publisher_node_id,
        "actor_type", message.publisher_actor_type,
        "instance_id", message.publisher_instance_id,
        "type", "pong",
        "received_sec", received_sec,
        "received_nsec", received_nsec
      )
    )
    print("ping received")
  end
  if(message.type == "init") then
    print("ECHO SERVER INIT")
    subscribe{type="ping", node_id=node_id}
  end
end