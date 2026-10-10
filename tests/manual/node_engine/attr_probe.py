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
    def cam_typed(self, cam: Camera3D):
        cam.fov += 2.0
        return cam.fov
    def bad_type(self):
        self.position = 5
    def bad_fov(self, cam):
        cam.fov = "wide"
    def ro_write(self):
        self.inside_tree = False
    def ro_write_other(self, n):
        n.child_count = 3
    def counts(self):
        return self.child_count
    def rename(self):
        self.name = "renamed"
        return self.name
    def toggle(self):
        self.processing = not self.processing
        return self.processing
    def mesh_set(self, m):
        m.mesh = "tree.obj"
        return m.mesh
    def extra_field(self):
        self.tag = 11
        return self.tag
    def as_value(self):
        f = self.position
        return f

class Sub(Probe):
    def sub_move(self, dt):
        self.position.x += dt
        return self.position.x

class Hero(Sub):
    def hero_set(self):
        self.position = self.position + Vec3(1, 1, 1)
        self.scale.y = 3.0
        return self.scale

class Wide(Camera3D):
    fov = 5.0
    def read(self):
        return self.fov
