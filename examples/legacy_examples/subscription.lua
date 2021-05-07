function receive(message)
  print("called")
  if(message.type == "init") then
    subscribe({example_number={LE, 5, optional=true}, example_number={GE, 1, optional=false}})
  end
  delayed_publish({example_number=2}, 2000)
end