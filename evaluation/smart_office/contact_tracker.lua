function receive(message)
  if(message.type == "init") then
    encounters = {}
    subscribe({type="contact_tracing.contact"})
    subscribe({type="contact_tracing.infection_warning"})

    enqueue_wakeup(60000*60*12, "cleanup_trigger")
  end

  if(message.type == "contact_tracing.contact") then
    local contact_event = {
      peer_1=decode_base64(message.peer_1),
      peer_2=decode_base64(message.peer_2),
      duration=message.duration,
      end_timestamp=message.end_timestamp//1,
      reported_by=message.reported_by
    }

    encounters[#encounters+1] = contact_event
  end

  if(message.type == "contact_tracing.infection_warning") then
    local risk_encounters = {}
    local infected_id = decode_base64(message.infected_id)

    for _key, encounter in pairs(encounters) do
      local risk_encounter = nil
      if(encounter.peer_1 == infected_id) then
        risk_encounter = encounter.peer_2
      elseif(encounter.peer_2 == infected_id) then
        risk_encounter = encounter.peer_1
      end
      if(risk_encounter) then
        if(not risk_encounters[risk_encounter]) then
          risk_encounters[risk_encounter] = {encounter.duration}
        else
          risk_encounters[risk_encounter][#risk_encounters[risk_encounter]+1] = encounter.duration
        end
      end
    end

    for risk_encounter, durations in pairs(risk_encounters) do
      local total_contact_time = 0
      for _id, duration in pairs(durations) do
        print(duration)
        total_contact_time = total_contact_time + duration
      end
      if(total_contact_time > 60) then
        local risk_level = "low"

        -- todo(raphaelhetzel) one could add additional risk levels based on the duration.
        -- Furthermore, one could subtract duplicate timeframes

        publish(Publication.new(
          "type", "contact_tracing.risk_encounter",
          "risk_id", encode_base64(risk_encounter),
          "risk_level", risk_level
        ))
      end
    end
  end

  if(message.type == "wakeup" and message.wakeup_id == "cleanup_trigger") then
    local to_delete = {}
    local cutoff_timestamp = unix_timestamp() - 60*60*24*14 

    local old_encounters = encounters

    encounters = {}

    for key, encounter in pairs(old_encounters) do
      if(now - encounter.end_timestamp < cutoff_timestamp) then
        encounters[#encounters+1] = encounter
      end
    end

    enqueue_wakeup(60000*60*12, "cleanup_trigger")
  end
end
