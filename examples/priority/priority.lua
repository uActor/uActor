function receive(message)

  if(message.type == "init") then
    print(actor_type)
    subscribe({foo="bar", processor_actor_type={2, actor_type, optional=true}, processor_node_id={2, node_id, optional=true}, processor_instance_id={2, instance_id, optional=true}}, 2)
    delayed_publish(Publication.new("foo", "bar", "key_2", "baz"), 10000)
  end

  if(message.foo == "bar") then
    print("called 2")
    republish(message)
  end

end