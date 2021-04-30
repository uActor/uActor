function receive(message)
  if(message.type == "init") then
    print("Init example_deployment")
  end

  if(message.type == "exit") then
    print("Exit example_deployment")
  end
end