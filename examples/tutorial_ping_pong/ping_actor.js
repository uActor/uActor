function receive(message, state) {
  if(message.type == "init") {
    log("Start JS Ping Actor")
    state.sequence_number = 0
    send_ping(state)
    enqueue_wakeup(5000, "ping_trigger")
  } else if(message.type == "wakeup" && message.wakeup_id == "ping_trigger") {
    send_ping(state)
    enqueue_wakeup(5000, "ping_trigger") 
  } else if(message.type == "exit") {
    log("Stop JS Ping Actor")
  } else if(message.type == "pong") {
    log("Pong Received: "+message.sequence_number)
  }
}

function send_ping(state) {
  let sequence_number = state.sequence_number++
  log("Send Ping: "+sequence_number)
  publish(new Message({
    type: "ping",
    sequence_number: sequence_number
  }))
}
