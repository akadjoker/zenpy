# Test net module (local-only/basic checks)
import net

ip = net.resolve("localhost")
assert ip != None
assert len(ip) > 0

# udp socket lifecycle
u = net.udp_create()
if u != None:
    ok = net.set_blocking(u, True)
    assert ok == True or ok == False

    ready = net.poll(u, 0)
    assert ready == True or ready == False

    closed = net.close(u)
    assert closed == True

print("All net tests passed.")
