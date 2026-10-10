# Mirror of SRC_VEC in tests/cpp/test_node_engine.cpp (the host defines Node/Node3D/.../log/is_valid).
class Vec(Node3D):
    def set_it(self):
        self.set_position(Vec3(1.5, 2.5, 3.5))
        self.set_scale(Vec3(2, 2, 2))
    def read_y(self):
        return self.position().y
    def gp(self):
        return self.global_position()
    def move(self):
        self.translate(Vec3(1, 1, 1))
