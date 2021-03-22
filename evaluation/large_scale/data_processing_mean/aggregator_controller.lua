function receive(message)
  if(message.type == "init") then
    
    subscribe({type="aggregator_overload", overload_level="BUILDING"})

    active_aggregation_deployment_type = {"BUILDING"}
    active_aggregation_deployment_type_changed = true
    switched = false
   
    enqueue_wakeup(30000, "start_source_sink")
  elseif( message.type == "wakeup" and message.wakeup_id == "start_source_sink")
    telemetry_set("deploy_source_sink", 1)
    deploy_sink(0, true)
    deploy_generator(0, true)

    enqueue_wakeup(30000, "start_aggregator")
  elseif (message.type == "wakeup" and message.wakeup_id == "start_aggregator") then
    deploy_aggregators(120000)
    deployment_count = 1
    enqueue_wakeup(100000, "refresh_soft_state")
  elseif(message.type == "wakeup" and message.wakeup_id == "refresh_soft_state") then
    
    --deploy_sink(120000, false)
    --deploy_generator(120000, false)
    
    deploy_aggregators(120000)

    deployment_count = deployment_count + 1

    if(deployment_count < 10) then
      enqueue_wakeup(100000, "refresh_soft_state") 
    end

  elseif(message.type == "aggregator_overload" and message.overload_level == "BUILDING" and not switched) then
    print("called switch function")
    cancel_aggregators(active_aggregation_deployment_type)
    active_aggregation_deployment_type = {"BUILDING", "NODE"}
    active_aggregation_deployment_type_changed = true
    deploy_aggregators(120000)
    switched = true
  end

end

function deploy_aggregators(lifetime)
  for stage_index, output in ipairs(active_aggregation_deployment_type) do
    local is_last = stage_index == #active_aggregation_deployment_type
    local input = "VALUE"
    if (not is_last) then
      input = active_aggregation_deployment_type[stage_index+1]
    end
    telemetry_set("deploy_aggregator", input.."-"..output)
    deploy_aggregation_stage(input, output, lifetime, active_aggregation_deployment_type_changed)
  end
  active_aggregation_deployment_type_changed = false
end

function cancel_aggregators(stages_to_cancel)
  for stage_index, output in ipairs(stages_to_cancel) do
    local is_last = stage_index == #stages_to_cancel
    local input = "VALUE"
    if (not is_last) then
      input = stages_to_cancel[stage_index+1]
    end
    cancel_deployment("data_aggregator_"..input.."_"..output)
  end 
end

function deploy_aggregation_stage(input, output, lifetime, include_code)
  
  constraints = {building="fmi"}

  if(output == "BUILDING") then
    constraints["node_type"] = "building_server"
  elseif(output == "WING") then
    constraints["node_type"] = "wing_Server"
  elseif(output == "ROOM") then
    constraints["node_type"] = "room_server"
  elseif(output == "NODE") then
    constraints["data_gathering_role"] = "sensor"
  end

  -- print(aggregator_code_for(output, input))
  deploy(
    "data_aggregator_"..input.."_"..output,
    code_hash(aggregator_code_for(output, input)),
    lifetime,
    {},
    constraints,
    include_code and aggregator_code_for(output, input) or false
  )
end

function deploy_sink(lifetime, include_code)
  deploy(
    "data_sink",
    code_hash(sink_code()),
    lifetime,
    {},
    {node_type="building_server", building="fmi"},
    include_code and sink_code() or false
  )
end

function deploy_generator(lifetime, include_code)
  deploy(
    "data_generator",
    code_hash(source_code()),
    lifetime,
    {},
    {data_gathering_role="sensor", building="fmi"},
    include_code and source_code() or false
  )
end

function deploy(deployment_name, version, lifetime, required_actors, constraints, code)
  
  local pub = Publication.new(
    "type", "deployment",
    "deployment_name", deployment_name,
    "deployment_actor_type", deployment_name.."_actor",
    "deployment_actor_runtime_type", "lua",
    "deployment_actor_version", version,
    "deployment_ttl", lifetime
  )

  local required_actor_string = ""
  for _key, required_actor in ipairs(required_actors) do
    required_actor_string = required_actor_string..required_actor..","
  end
  if(#required_actor_string >= 1) then
    pub["deployment_required_actors"] = string.sub(constraints_string, 1, -2)
  else
    pub["deployment_required_actors"] = ""
  end
  required_actor_string = nil

  local constraints_string = ""
  for key, value in pairs(constraints) do
    constraints_string = constraints_string..key..","
    pub[key] = value
  end
  if(#constraints_string >= 1) then
    pub["deployment_constraints"] = string.sub(constraints_string, 1, -2)
  end
  constraints_string = nil

  if(code) then
    pub["deployment_actor_code"] = code
  end

  publish(pub)

end

function cancel_deployment(deployment_name)
  publish(Publication.new(
    "type", "deployment_cancelation",
    "deployment_name", deployment_name
  ))
end

function aggregator_code_for(aggregation_level, aggregation_level_in)
  return [[
AGGREGATION_LEVEL = "]]..aggregation_level..[["

INCOMMING_AGGREGATION_LEVEL = "]]..aggregation_level_in..[["
    
--include <aggregator_base>
  ]]
end
  
function sink_code()
  return [[
--include <data_sink>
  ]]
end

function source_code()
  return [[
--include <data_generator>
  ]]
end