# HTTP Client Actor Example

Contains several examples utilizing the `http_client_actor`.

For all examples there are no special requirements to launch the node.
```bash
./path_to/uActorBin
```

## Example 1: Get Request

This sample will do two http get request to `http://webhookinbox.com/` (one with the standard curl headers, and the other one with custom http headers). The results will be printed afterwards.

### Important Files
* deployment specification [http_get_request.yaml](http_get_request.yaml)
* actor code [http_get_request.lua](http_get_request.lua)

