function receive(message)
  if(message.type == "init") then
    subscribe({type="contact_tracing.risk_encounter"})
    publish(Publication.new(
      "type", "contact_tracing.contact",
      "peer_1", encode_base64("KEY1"),
      "peer_2", encode_base64("KEY2"),
      "duration", 30,
      "end_timestamp", unix_timestamp(),
      "reported_by", "bar"
    ))
    publish(Publication.new(
      "type", "contact_tracing.contact",
      "peer_1", encode_base64("KEY2"),
      "peer_2", encode_base64("KEY1"),
      "duration", 31,
      "end_timestamp", unix_timestamp(),
      "reported_by", "foo"
    ))

    publish(Publication.new(
      "type", "contact_tracing.infection_warning",
      "infected_id", encode_base64("KEY1")
    ))
  end

  if(message.type == "contact_tracing.risk_encounter") then
    print("Risk Encouter: "..decode_base64(message.risk_id).." Risk Level: "..message.risk_level)
  end
end