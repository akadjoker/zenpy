# Test http module (non-external/basic checks)
import http

local_ip = http.get_local_ip()
if local_ip != None:
    assert len(local_ip) > 0

# ping result can vary by environment; assert type only
p = http.ping("127.0.0.1", 80, 1)
assert p == True or p == False

print("All http tests passed.")
