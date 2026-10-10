# Mirror of SRC_NOTIFY in tests/cpp/test_node_engine.cpp (the host defines Node/Node3D/.../log/is_valid).
class Watcher(Node3D):
    def _notification(self, what):
        log("notif:" + str(what))
