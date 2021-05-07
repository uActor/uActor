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
      delayed_publish({node_id="node_1", instance_id="xyz.hetzel.deployment.multinode_counter", actor_type="example.multinode_counter", type="next"}, 5000)
    end
  elseif(message.type == "exit") then
    print("exit")
    publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=1}) 
  else
    if(node_id == "node_1") then
      print("send 1")
      publish({node_id="node_2", instance_id="xyz.hetzel.deployment.multinode_counter", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_2") then
      print("send 2")
      publish({node_id="node_3", instance_id="xyz.hetzel.deployment.multinode_counter", actor_type="example.multinode_counter", type="next"}) 
    elseif(node_id == "node_3") then
      print("send 3")
      publish({node_id="node_4", instance_id="xyz.hetzel.deployment.multinode_counter", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_4") then
      print("send 4")
      publish({node_id="node_5", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_5") then
      print("send 5")
      publish({node_id="node_6", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_6") then
      print("send 6")
      publish({node_id="node_7", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_7") then
      print("send 7")
      publish({node_id="node_8", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_8") then
      print("send 8")
      publish({node_id="node_9", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_9") then
      print("send 9")
      publish({node_id="node_10", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_10") then
      print("send 10")
      publish({node_id="node_11", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_11") then
      print("send 11")
      publish({node_id="node_12", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_12") then
      print("send 12")
      publish({node_id="node_13", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_13") then
      print("send 13")
      publish({node_id="node_14", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_14") then
      print("send 14")
      publish({node_id="node_15", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_15") then
      print("send 15")
      publish({node_id="node_16", actor_type="example.multinode_counter", type="next"})
    elseif(node_id == "node_16") then
      print("send 16")
      publish({node_id="node_1", actor_type="example.multinode_counter", type="next"})
    end
  end
end