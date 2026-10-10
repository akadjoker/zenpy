# Mirror of SRC_SCENE in tests/cpp/test_node_engine.cpp (the host defines Node/Node3D/.../log/is_valid).
class Player(Node3D):
    speed = 2.0
    def _ready(self):
        self.set_name("player")
        self.hits = 0
        self.marker = 42
        self.cam = self.find_child("cam")
        self.nchildren = self.get_child_count()
        log("ready:" + self.get_name() + " children=" + str(self.get_child_count()) + " parent=" + self.get_parent().get_name())
    def _process(self, dt):
        self.set_position(self.position() + Vec3(self.speed * dt, 0, 0))
        self.hits += 1
        if self.cam != None:
            self.cam.set_fov(60.0 + self.position().x)
    def gun_tag(self):
        return self.find_child("gun").tag
    def pos(self):
        return self.position()
    def root_name(self):
        return self.get_parent().get_name()
    def speed_value(self):
        return self.speed

class Gun(MeshInstance3D):
    def _ready(self):
        self.tag = 7
        self.seen = False
        log("ready:" + self.get_name())
        log("ident:" + str(self.get_parent() == player_inst))
    def _process(self, dt):
        if self.seen == False:
            self.seen = True
            log("marker:" + str(self.get_parent().marker))
        log("process:gun")
