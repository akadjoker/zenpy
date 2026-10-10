# Mirror of SRC_BENCH in tests/cpp/test_node_engine.cpp (the host defines Node/Node3D/.../log/is_valid).
class BMove(Node3D):
    def _process(self, dt):
        self.set_position(self.position() + Vec3(dt, 0, 0))

class BNone(Node3D):
    speed = 1.0

class BEmpty(Node3D):
    def _process(self, dt):
        pass

class Finder(Node3D):
    def find_loop(self, n):
        for i in range(n):
            c = self.find_child("cam")
    def parent_loop(self, n):
        for i in range(n):
            c = self.get_parent()
    def count_loop(self, n):
        for i in range(n):
            c = self.get_child_count()

class Sub(Node):
    count = 0
    def hook(self, btn):
        btn.connect("pressed", self.on_pressed)
    def on_pressed(self):
        self.count += 1
