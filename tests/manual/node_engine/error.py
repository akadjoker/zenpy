# Mirror of SRC_ERROR in tests/cpp/test_node_engine.cpp (the host defines Node/Node3D/.../log/is_valid).
class Bad(Node3D):
    def _process(self, dt):
        x = None
        x.boom()

class Counter(Node3D):
    def _ready(self):
        self.n = 0
    def _process(self, dt):
        self.n += 1
    def count(self):
        return self.n
