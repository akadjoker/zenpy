# Mirror of SRC_SIGNAL in tests/cpp/test_node_engine.cpp (the host defines Node/Node3D/.../log/is_valid).
class Listener(Node):
    count = 0
    def hook(self, btn):
        btn.connect("pressed", self.on_pressed)
    def unhook(self, btn):
        return btn.disconnect("pressed", self.on_pressed)
    def on_pressed(self):
        self.count += 1
    def get_count(self):
        return self.count

class Emitter(Node):
    def fire(self):
        self.emit("pressed")
