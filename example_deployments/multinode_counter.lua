function receive(message)
  if(last) then
    print(now()-last)
  end
  last = now()
  if(light == nil or light == 0) then
    light = 1
    publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=0})
  else
    light = 0
    publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=1}) 
  end

  if(message.type == "init") then
    print("init")
    if(node_id == "node_1") then
      print("send init")
      publish({node_id="node_2", actor_type="example.multinode_counter", type="next"})
      
    end
  elseif(message.type == "exit") then
    print("exit")
    publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=1}) 
  else
    if(node_id == "node_1") then
      print("send 1")
      publish({node_id="node_2", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_2") then
      print("send 2")
      publish({node_id="node_3", actor_type="example.multinode_counter", type="next"}) 
    elseif(node_id == "node_3") then
      print("send 3")
      publish({node_id="node_1", actor_type="example.multinode_counter", type="next"})
    end
  end
end