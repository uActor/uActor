NUM_NODES = 341

function receive(message)
  if(message.type == "init") then
    message_counts = 0
    delayed_publish(
      Publication.new(
        "type", "request_message_counts"
      ),
      60000
    )
  end
  
  if(message.type == "message_counts") then
    message_counts = message_counts + 1
    testbed_log_integer("accepted_message_count_"..message.publisher_node_id, message.accepted_message_count)
    testbed_log_integer("accepted_message_size_"..message.publisher_node_id, message.accepted_message_size)
    testbed_log_integer("rejected_message_count_"..message.publisher_node_id, message.rejected_message_count)
    testbed_log_integer("rejected_message_size_"..message.publisher_node_id, message.rejected_message_size)
    if(message_counts == NUM_NODES) then
      testbed_log_integer("done", 1)
    end
  end
end