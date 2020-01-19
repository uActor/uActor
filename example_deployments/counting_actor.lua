function receive(message)
    if(foo == nil) then
        foo = 0
    else
        foo = foo + 1
    end
    print(foo)
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="delayed_publish"}, 2000);
end