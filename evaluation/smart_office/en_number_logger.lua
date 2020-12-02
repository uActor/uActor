function receive(message)
  
  if(message.type == "init") then
    subscribe{type="node_en_key_count"}
  end

  if(message.type == "node_en_key_count") then
    publish(Publication.new(
      "type", "data_point",
      "measurement", "en_app",
      "timestamp", message.timestamp,
      "values", "num_keys",
      "tags", "node",
      "num_keys",  message.num_keys,
      "node", message.publisher_node_id
    ))
  end
end