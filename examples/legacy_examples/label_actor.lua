function receive(message)
  print(message.type)
  if(message.type == "init") then
      subscribe({type="label_update_notification", node_id=node_id})
      publish({node_id=node_id, type="label_update", command="upsert", key="foo", value="bar"})
  end
end