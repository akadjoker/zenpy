# Mirror of SRC_ATTR in tests/cpp/test_node_engine.cpp (the host defines Node/Node3D/.../log/is_valid).
class Probe(Node3D):
    def read_x(self):
        return self.position.x
    def assign_whole(self):
        self.position = Vec3(9, 9, 9)
        return 1
    def read_after_assign(self):
        return self.position
    def call_after_assign(self):
        return self.position()
    def assign_x(self):
        self.position.x = 5.0
        return 1
    def aug_x(self):
        self.position.x += 1.0
        return 1
    def spin(self, dt):
        self.rotation = Vec3(0, dt, 0)
        return 1
    def cam_attr_write(self, cam):
        cam.fov = 33.0
        return 1
    def cam_attr_read(self, cam):
        return cam.fov
    def cam_getter(self, cam):
        return cam.fov()
