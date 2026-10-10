# Mirror of SRC_SPAWN in tests/cpp/test_node_engine.cpp (the host defines Node/Node3D/.../log/is_valid).
class Bullet(MeshInstance3D):
    def _ready(self):
        self.age = 0
        log("ready:" + self.get_name())
    def _process(self, dt):
        self.age += 1
        if self.age >= 3:
            self.queue_free()

class Spawner(Node3D):
    def _ready(self):
        log("ready:spawner")
        c = MeshInstance3D()
        c.set_name("plain")
        c.set_mesh("cube.obj")
        c.set_process(True)
        self.add_child(c)
        b = Bullet()
        b.set_name("bullet")
        self.add_child(b)
        self.bullet = b
        self.plain = c
        self.same = (self.get_child(0) == c) and (self.get_child(1) == b)
    def bullet_alive(self):
        return is_valid(self.bullet)
    def plain_frames(self):
        return self.plain.frames()
    def stale_call(self):
        return self.bullet.get_name()
    def double_add(self):
        self.add_child(self.plain)
