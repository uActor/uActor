-- Actor implementing the co2-based corona risk-assessment from https://doi.org/10.1101/2021.04.04.21254903 (alg. 19).
-- The actor subscribes to the en beacon counts and uses them as an estimate for the person count in the room.
-- It furthermore subscribes to the CO2 averages and uses them to compute the current transmission risk.
-- It sends a warning message to the display if the transmission risk exceeds a threshold.
-- The threshold and room-specific parameters are fetched from a global configuration actor.
-- The algorithm starts a risk-assesment session as soon as two ore more persons are present and ends as soon as the person count drops below 2.
-- Older values are ignored.

function receive(message)
  if(message.type == "node_environment_info" and detected_n_persons >= 2) then
    process_value((message.timestamp//60)*60, message.value_co2)
    local current_risk = risk()
    print("current_risk: "..current_risk)
    if(current_risk > risk_tolerance) then
      send_warning_message(current_risk)
    end
  end

  if(message.type == "node_en_key_count") then
    process_count(message.num_keys)
  end

  if(message.type == "init") then
    -- These could be detected by another sensor
    wearing_mask = false
    activity_factor = 72

    -- Required State
    detected_n_persons = -1
    memory_value = 0
    start_time = - 1
    --- Used to fill in missing data points
    last_timestamp = -1
    last_value = -1

    --
    co2_sub_id = -1
    -- Allow for some people shortly leaving the room
    person_missing_tolerance = -1
    delayed_publish(Publication.new("type", "fetch_room_data", "room_id", "paper_example_room"), 1000)
    print("Init CO2-risk actor")
  end

  if(message.type == "room_configuration_data") then -- Init Part 2
    -- Parameters fetched from the configuration server
    outdoor_ventilation_rate = message.room_ventilation_rate
    room_area = message.room_width*message.room_depth
    room_volume = room_area * message.room_height
    co2_baseline = message.room_co2_baseline
    relative_suscceptibility = message.relative_suscceptibility
    risk_tolerance = message.risk_tolerance
    subscribe({type="node_en_key_count", publisher_node_id=node_id})
    print("Completed CO2-risk actor initialization")
  end
end

function process_count(count)
  if(count >= 2) then
    if(start_time < 0) then
      start_time = now()
      co2_sub_id = subscribe({type="node_environment_info", publisher_node_id=node_id})
    end
    person_missing_tolerance = 5
    detected_n_persons = count
  else
    if(person_missing_tolerance <= 0 and start_time > 0) then
      stop()
      detected_n_persons = 0
    else
      person_missing_tolerance = person_missing_tolerance  - 1
    end
  end
end

function process_value(timestamp, value)
  if(start_time > 0) then
    if(last_value > -1) then
      local time_diff = (timestamp - last_timestamp)
      for i = 1, (time_diff // 60 - 1) do
        print("path")
        memory_value = memory_value + minute_factor(last_value)
      end
    end
  end
  last_value = value
  last_timestamp = timestamp
  memory_value = memory_value + minute_factor(last_value)
end

function stop()
  local total_contact_time = (now() - start_time) / 60000.0
  print("Risk analysis reset after "..total_contact_time..". End risk: "..risk())
  if(co2_sub_id > 0) then
    unsubscribe(co2_sub_id)
    co2_sub_id = -1
  end
  start_time = -1
  memory_value = 0
  last_timestamp = -1
  last_value = -1
  person_missing_tolerance = -1
end

function risk()
  if(detected_n_persons < 0) then
    return 0
  end
  return transmission_term() * person_term() * co2_ventilation()/virus_ventilation() * memory_value
end

function minute_factor(co2_value)
  local breathing_rate = 0.5
  local b_part = 1 + (virus_ventilation()/co2_ventilation() - 1) * math.exp(-1*virus_ventilation())
  return breathing_rate * activity_factor * (co2_value - co2_baseline) * b_part * 1.0/60.0
end

function person_term()
  return (detected_n_persons-1) / detected_n_persons
end

function transmission_term()
  local mask_factor = wearing_mask and 0.3 or 1
  local breath_co2 = 38000
  return (relative_suscceptibility*mask_factor*mask_factor) / breath_co2
end

function co2_ventilation()
  return outdoor_ventilation_rate
end

function virus_ventilation()
  local filtration_efficiency = 0
  local recirculation_rate = 6
  local filtration_rate = filtration_efficiency * recirculation_rate
  local settling_velocity = 0.108
  local sedimentation_rate = settling_velocity * room_area/room_volume
  local deactivation_rate = 0.3
  
  return outdoor_ventilation_rate + filtration_rate + sedimentation_rate  + deactivation_rate
end

function send_warning_message(risk_level)
  print("warning: "..risk_level)
  publish(Publication.new(
    "type", "notification", "notification_text", "WARNING:\nINCREASED\nINFECTION\nRISK",
    "node_id", node_id, "notification_lifetime", 60000,
    "notification_id", node_id.."."..actor_type.."."..instance_id..".co2_corona_warning"
  ))
end