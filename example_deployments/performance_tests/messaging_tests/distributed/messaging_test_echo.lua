function receive(message)
  -- if(message.type == "init") then
  --   count = 0
  --   testbed_log_string("_logger_test_postfix", 1)
  -- end
  if(message.type == "ping") then
    message["node_id"] = message.publisher_node_id
    message["actor_type"] = message.publisher_actor_type
    message["instance_id"] = message.publisher_instance_id
    message["publisher_node_id"] = nil
    message["publisher_actor_type"] = nil
    message["publisher_instance_id"] = nil
    message["_internal_sequence_number"] = nil
    message["_internal_epoch_number"] = nil
    message["_internal_forwarded_by"] = nil
    message["type"] = "pong"
    publish(message)
    -- count = count + 1
    -- if(count == 200) then
    --   testbed_log_string("_logger_test_postfix", 20) 
    -- end
  end
end