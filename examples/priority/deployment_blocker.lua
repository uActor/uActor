BLOCKED_TYPES = {}
BLOCKED_TYPES["de.tum.test.prio"] = "de.tum.test.prio.actor"

function receive(publication)

  if(publication.type == "init") then
    
    print("init deployment blocker")

    subscribe(
      {
        type="deployment",
        processor_node_id={2, node_id, optional=true},
        processor_actor_type={2, actor_type, optional=true},
        processor_instance_id={2, instance_id, optional=true}
      },
      1
    )

    for deployment_name, actor_name in pairs(BLOCKED_TYPES) do
      publish(
        Publication.new(
          "type", "deployment_cancelation",
          "deployment_name", deployment_name,
          "node_id", node_id
        )
      )
    end


  end

  if(publication.type == "deployment") then
    if(not BLOCKED_TYPES[publication.deployment_name]) then
      republish(publication)
    else
      print("blocked")
    end
  end
end