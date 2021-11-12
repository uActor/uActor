function receive(message, state) {
  if(message.type == "init") {
    log("Start JS Pong Actor.")
  } else if(message.type == "exit") {
    log("Stop JS Pong Actor.")
  } else if(message.type == "ping") {
    log("Ping Received: "+message.sequence_number)
    publish(new Message({
      node_id: message.publisher_node_id,
      actor_type: message.publisher_actor_type,
      instance_id: message.publisher_instance_id,
      type: "pong",
      sequence_number: message.sequence_number 
    }))
  }
}