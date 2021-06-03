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

## Example 2: Post Request

This sample will do two http post requests. In order to test it, we recommend to use a webhook service like [webhook inbox](http://webhookinbox.com/), the resulting address can then be changed in [http_post_request.lua](http_post_request.lua) as target address. After this example is executed the requests should be visible in the webhook service.

### Important Files
* deployment specification [http_post_request.yaml](http_get_request.yaml)
* actor code [http_get_request.lua](http_post_request.lua)
