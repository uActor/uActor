function receive(message)
  if(message.type == "init") then
    subscribe({type="request_message_counts"})
  end

  if(message.type == "request_message_counts") then
    print("Called Message Counts")
    accepted_message_count, accepted_message_size, rejected_message_count, rejected_message_size = connection_traffic()
    publish(
      Publication.new(
        "type", "message_counts",
        "node_id", message.publisher_node_id,
        "actor_type", message.publisher_actor_type,
        "instance_id", message.publisher_instance_id,
        "accepted_message_count", accepted_message_count,
        "accepted_message_size", accepted_message_size,
        "rejected_message_count", rejected_message_count,
        "rejected_message_size", rejected_message_size
      )
    )
  end
end