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

function cancel_deployment(deployment_name, local_only)
  p = Publication.new(
    "type", "deployment_cancelation",
    "deployment_name", deployment_name
  )
  if(local_only) then
    p["node_id"] = node_id
  end
  publish(p)
end