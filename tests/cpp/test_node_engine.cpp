/*
** test_node_engine.cpp -- a faithful MOCK of a Godot-style node engine
** (Node / Node3D / Camera3D / MeshInstance3D / SceneTree, notifications,
** signals, queue_free, scripts attached to existing C++ nodes) bound to ZenPy
** with zen_bind.hpp, to measure what works, what is awkward and what is
** missing for the "class Player(Node3D): def _ready / def _process" model.
**
** Normal run:     bin/test_node_engine
** Benchmark run:  bin/test_node_engine --bench N FRAMES      (default 10000 100)
**
** The scripts are embedded as raw strings and mirrored (same text) under
** tests/manual/node_engine/ (one .py per script) for reading.
**
** Host-side design notes:
**  - ScriptInstance/attach(): a script CLASS is attached to a node the engine
**    already owns, with vm.make_instance_borrowed(script_class, node): no native
**    ctor (no second node) and the instance is "borrowed", so the VM never runs
**    the class dtor on the engine's node.
**  - Ownership: bound with zen_bind's plain .dtor() on Node (delete).  A node a
**    script creates (Bullet()) is owned by its wrapper until the tree takes it:
**    add_child() flips the wrapper to borrowed (Host::adopt).  Nodes the engine
**    creates are only ever wrapped borrowed (instance_for/attach), so the GC can
**    never delete them.  Node::gc_owned is the engine-side mirror of that flag
**    (queue_free() on an orphan must not double free what the wrapper owns).
**  - Identity: Host::objectdb (Node* -> zen::Ref) is the ObjectDB; every bound
**    getter that returns a node goes through Host::instance_for().
**  - Freed nodes: ~Node -> Host::forget() unrefs the instance and sets its
**    native_data to nullptr, so stale script references fail with an error
**    instead of dereferencing freed memory.
**  - Properties: position/rotation/scale/fov/frames (data members) and
**    name/processing/mesh (accessor pairs) plus read-only inside_tree/child_count
**    are native properties: `self.position`, `self.position = v`,
**    `self.position.x += dt` reach the C++ member (see test_attributes).
*/

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"
#include "debug.h"
#include "zen_bind.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace zen;
using zen::bind::def_class;
using zen::bind::def_fn;
using zen::bind::ClassOf;

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) printf("  %-66s", name)
#define PASS() do { printf("OK\n"); g_tests_passed++; } while (0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); g_tests_failed++; } while (0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while (0)
#define FINDING(...) do { printf("      finding: "); printf(__VA_ARGS__); printf("\n"); } while (0)

/* runtime errors land here instead of stderr */
static char g_err[4096];
static void capture_err(const char *s, int len, void *)
{
    size_t have = strlen(g_err);
    size_t room = sizeof(g_err) - 1 - have;
    size_t n = (size_t)len < room ? (size_t)len : room;
    memcpy(g_err + have, s, n);
    g_err[have + n] = 0;
}
static std::string first_line(const char *s) { return std::string(s, strcspn(s, "\n")); }

static std::vector<std::string> g_log;
static void host_log(const char *s) { g_log.push_back(s); }
static void host_noop() {}
static int count_log(const char *s)
{
    int n = 0;
    for (auto &l : g_log) n += (l == s);
    return n;
}
static int index_log(const char *s)
{
    for (size_t i = 0; i < g_log.size(); i++) if (g_log[i] == s) return (int)i;
    return -1;
}

/* =========================================================
** The mock engine (plain C++, no knowledge of ZenPy except the hooks)
** ========================================================= */
struct Node;
struct Host;
struct SceneTree;
static Host *g_host = nullptr;
static SceneTree *g_tree = nullptr;
static void (*g_on_node_free)(Node *) = nullptr;
static bool script_dispatch_ready(Node *);
static bool script_dispatch_notification(Node *, int);

enum { NOTIFICATION_READY = 13, NOTIFICATION_PARENTED = 18, NOTIFICATION_UNPARENTED = 19 };

struct ScriptInstance
{
    Value inst = val_nil(); /* kept alive by Host::objectdb */
    int ready_slot = -1, process_slot = -1, notif_slot = -1;
    bool has_ready = false, has_process = false, has_notif = false;
    bool faulted = false;
};

struct SignalConn
{
    std::string name;
    zen::Ref cb;
};

static int g_node_allocs = 0, g_node_frees = 0;

struct Node
{
    std::string name;
    Node *parent = nullptr;
    std::vector<Node *> children;
    bool processing = false;
    bool ready_done = false;
    bool in_tree = false;
    bool gc_owned = false; /* created from script, not yet in a tree */
    bool queued = false;
    ScriptInstance *script = nullptr;
    std::vector<SignalConn> conns;

    Node() { g_node_allocs++; }
    virtual ~Node();
    virtual ObjClass *engineClass() const; /* bound class of this C++ type */
    virtual void _ready() {}
    virtual void _process(float) {}

    void addChild(Node *c);
    void removeChild(Node *c);
    Node *getChild(size_t i) const { return i < children.size() ? children[i] : nullptr; }
    size_t getChildCount() const { return children.size(); }
    Node *getParent() const { return parent; }
    Node *findChild(const char *n) const
    {
        for (Node *c : children)
            if (c->name == n) return c;
        for (Node *c : children)
            if (Node *r = c->findChild(n)) return r;
        return nullptr;
    }
    Node *getNode(const char *path) const
    {
        const Node *cur = this;
        std::string p = path;
        size_t i = 0;
        while (cur && i <= p.size())
        {
            size_t j = p.find('/', i);
            std::string part = p.substr(i, j == std::string::npos ? std::string::npos : j - i);
            if (part == "..") cur = cur->parent;
            else if (!part.empty() && part != ".")
            {
                const Node *nx = nullptr;
                for (Node *c : cur->children) if (c->name == part) { nx = c; break; }
                cur = nx;
            }
            if (j == std::string::npos) break;
            i = j + 1;
        }
        return (Node *)cur;
    }
    void setProcess(bool b) { processing = b; }
    bool isProcessing() const { return processing; }
    bool isInsideTree() const { return in_tree; }
    bool isReady() const { return ready_done; }
    const char *getName() const { return name.c_str(); }
    void setName(const char *n) { name = n; }
    void queueFree();

    /* Godot: notification() runs the script's _notification and the C++ handler */
    void notification(int what)
    {
        script_dispatch_notification(this, what);
        notificationHandler(what);
    }
    virtual void notificationHandler(int what)
    {
        if (what == NOTIFICATION_READY && !script_dispatch_ready(this))
            _ready();
    }
    void emit(const char *signal);
    void connectSignal(const char *signal, zen::Ref cb) { conns.push_back({signal, std::move(cb)}); }
};

struct Node3D : Node
{
    Vec3f position{0, 0, 0}, rotation{0, 0, 0}, scale{1, 1, 1};
    ObjClass *engineClass() const override;
    void translate(Vec3f d) { position.x += d.x; position.y += d.y; position.z += d.z; }
    Vec3f globalPosition() const
    {
        Vec3f p = position;
        for (Node *n = parent; n; n = n->parent)
            if (Node3D *n3 = dynamic_cast<Node3D *>(n))
            {
                p.x += n3->position.x; p.y += n3->position.y; p.z += n3->position.z;
            }
        return p;
    }
};

struct Camera3D : Node3D
{
    float fov = 70;
    ObjClass *engineClass() const override;
};

struct MeshInstance3D : Node3D
{
    std::string mesh;
    int frames = 0;
    ObjClass *engineClass() const override;
    void _process(float) override { frames++; }
    const char *getMesh() const { return mesh.c_str(); }
    void setMesh(const char *m) { mesh = m; }
};

/* a C++ node with real per-frame C++ work, for the benchmark baseline */
struct Mover3D : Node3D
{
    Mover3D() { processing = true; }
    void _process(float dt) override { position.x += dt; }
};

struct SceneTree
{
    Node *root = nullptr;
    std::vector<Node *> queued_free;
    long process_calls = 0;

    SceneTree()
    {
        root = new Node();
        root->name = "root";
        root->in_tree = true;
    }
    ~SceneTree() { delete root; }

    /* children first, then the node (Godot order) */
    void propagateReady(Node *n)
    {
        for (size_t i = 0; i < n->children.size(); i++)
            propagateReady(n->children[i]);
        if (!n->ready_done)
        {
            n->ready_done = true;
            n->notification(NOTIFICATION_READY);
        }
    }
    void start() { propagateReady(root); }

    void walk(Node *n, float dt);
    void process(float dt)
    {
        walk(root, dt);
        flush();
    }
    void flush();
};

/* ---- script dispatch (the Godot "is overridden" test lives in the slots) ---- */
struct Host
{
    VM &vm;
    std::unordered_map<Node *, zen::Ref> objectdb; /* Node* -> script instance */
    int sel_ready, sel_process, sel_notif;
    long errors = 0;
    std::string last_error;

    explicit Host(VM &v) : vm(v)
    {
        sel_ready = vm.intern_selector("_ready", 6);
        sel_process = vm.intern_selector("_process", 8);
        sel_notif = vm.intern_selector("_notification", 13);
    }

    /* The same script Value for the same node, always. */
    Value instance_for(Node *n)
    {
        if (!n) return val_nil();
        auto it = objectdb.find(n);
        if (it != objectdb.end()) return it->second.get();
        ObjClass *k = n->engineClass();
        Value v = vm.make_instance_borrowed(k, n); /* the engine owns n */
        objectdb.emplace(n, zen::Ref(vm, v));
        return v;
    }

    static bool overridden(const ObjClass *k, int slot)
    {
        return slot >= 0 && slot < k->vtable_size && is_closure(k->vtable[slot]);
    }

    void resolve_slots(ScriptInstance *si, ObjClass *k)
    {
        si->ready_slot = sel_ready;
        si->process_slot = sel_process;
        si->notif_slot = sel_notif;
        si->has_ready = overridden(k, sel_ready);
        si->has_process = overridden(k, sel_process);
        si->has_notif = overridden(k, sel_notif);
    }

    /* Godot way: the C++ node exists, a script class is attached to it. */
    ScriptInstance *attach(Node *n, ObjClass *script_class)
    {
        bool derives = false;
        for (ObjClass *c = script_class; c; c = c->parent)
            if (c == n->engineClass()) derives = true;
        if (!derives)
        {
            last_error = "script class does not derive from the node's engine class";
            return nullptr;
        }
        /* NOT make_instance(): that would run the native ctor (a second node) */
        Value v = vm.make_instance_borrowed(script_class, n);
        objectdb.erase(n); /* replaces a lazily created plain wrapper (identity break, see test) */
        objectdb.emplace(n, zen::Ref(vm, v));
        return finish_script(n, v, script_class);
    }

    /* A script-created node (Bullet() -> instance already exists, native ctor
    ** already allocated the node): record it in the ObjectDB and resolve slots. */
    ScriptInstance *adopt(Node *n, Value v)
    {
        as_instance(v)->borrowed = true; /* the tree owns n from now on: the wrapper must not delete it */
        if (objectdb.find(n) == objectdb.end())
            objectdb.emplace(n, zen::Ref(vm, v));
        if (n->script) return n->script;
        return finish_script(n, v, as_instance(v)->klass);
    }

    ScriptInstance *finish_script(Node *n, Value v, ObjClass *k)
    {
        delete n->script;
        ScriptInstance *si = new ScriptInstance();
        si->inst = v;
        resolve_slots(si, k);
        n->script = si;
        if (si->has_process) n->processing = true; /* Godot: defining _process enables it */
        return si;
    }

    void forget(Node *n)
    {
        auto it = objectdb.find(n);
        if (it == objectdb.end()) return;
        Value v = it->second.get();
        if (is_instance(v))
            as_instance(v)->native_data = nullptr; /* tombstone: stale refs error out */
        objectdb.erase(it);
    }

    /* There is no public VM::clear_error().  invoke() never clears had_error_;
    ** call_fn() and run() clear it on entry.  So the host clears it by calling a
    ** no-op native through call_fn -- a workaround, see the report. */
    void reset_error()
    {
        if (!vm.had_error()) return;
        Value noop = vm.get_global("__noop");
        vm.call_fn(noop, nullptr, 0);
    }

    void note_error(Node *n, ScriptInstance *si)
    {
        errors++;
        si->faulted = true;
        n->processing = false; /* policy: a faulted script stops being processed */
        last_error = first_line(g_err);
        reset_error(); /* otherwise every later invoke() in the frame also "fails" */
    }

    /* Invoke one script method; false when the script errored. */
    bool call(Node *n, ScriptInstance *si, int slot, Value *args, int nargs)
    {
        /* invoke(slot) is the hot path: call_fn(closure) needs a bound-method Value for self
        ** (it treats the closure arity as excluding self) -> an allocation per call. */
        vm.invoke(si->inst, slot, args, nargs);
        if (vm.had_error())
        {
            note_error(n, si);
            return false;
        }
        return true;
    }
};

static bool script_dispatch_ready(Node *n)
{
    ScriptInstance *si = n->script;
    if (!si || !si->has_ready || si->faulted) return false;
    g_host->call(n, si, si->ready_slot, nullptr, 0);
    return true;
}
static bool script_dispatch_notification(Node *n, int what)
{
    ScriptInstance *si = n->script;
    if (!si || !si->has_notif || si->faulted) return false;
    Value a[1] = {val_int(what)};
    g_host->call(n, si, si->notif_slot, a, 1);
    return true;
}

Node::~Node()
{
    g_node_frees++;
    for (Node *c : children) delete c;
    if (g_on_node_free) g_on_node_free(this);
    delete script;
}
ObjClass *Node::engineClass() const { return ClassOf<Node>::klass; }
ObjClass *Node3D::engineClass() const { return ClassOf<Node3D>::klass; }
ObjClass *Camera3D::engineClass() const { return ClassOf<Camera3D>::klass; }
ObjClass *MeshInstance3D::engineClass() const { return ClassOf<MeshInstance3D>::klass; }

static void enter_tree(Node *n)
{
    n->in_tree = true;
    for (Node *k : n->children) enter_tree(k);
}
void Node::addChild(Node *c)
{
    c->parent = this;
    c->gc_owned = false; /* the tree owns it from now on */
    children.push_back(c);
    if (in_tree) enter_tree(c);
    c->notification(NOTIFICATION_PARENTED);
    if (in_tree && ready_done) g_tree->propagateReady(c);
}
void Node::removeChild(Node *c)
{
    auto it = std::find(children.begin(), children.end(), c);
    if (it != children.end())
    {
        children.erase(it);
        c->parent = nullptr;
        c->notification(NOTIFICATION_UNPARENTED);
    }
}
void Node::queueFree()
{
    if (gc_owned) return; /* orphan: the wrapper's GC owns it (see report) */
    if (!queued)
    {
        queued = true;
        g_tree->queued_free.push_back(this);
    }
}
void Node::emit(const char *signal)
{
    std::vector<Value> targets;
    for (auto &c : conns)
        if (c.name == signal) targets.push_back(c.cb.get());
    for (Value cb : targets)
    {
        g_host->vm.call_fn(cb, nullptr, 0);
        if (g_host->vm.had_error())
        {
            g_host->errors++;
            g_host->last_error = first_line(g_err);
            g_host->reset_error();
        }
    }
}

void SceneTree::walk(Node *n, float dt)
{
    if (n->processing && !n->queued)
    {
        process_calls++;
        ScriptInstance *si = n->script;
        if (si && si->has_process)
        {
            if (!si->faulted)
            {
                Value a[1] = {val_float(dt)};
                g_host->call(n, si, si->process_slot, a, 1);
            }
        }
        else
            n->_process(dt); /* no script, or the script does not override _process */
    }
    for (size_t i = 0; i < n->children.size(); i++)
        walk(n->children[i], dt);
}
void SceneTree::flush()
{
    std::vector<Node *> q;
    q.swap(queued_free);
    for (Node *n : q)
    {
        if (n->parent) n->parent->removeChild(n);
        delete n; /* recursive; ~Node -> Host::forget for every descendant */
    }
}

/* =========================================================
** Binding layer (what the host has to write)
** ========================================================= */
static Value n_get_parent(VM *, Node *s) { return g_host->instance_for(s->parent); }
static Value n_get_child(VM *, Node *s, int i) { return g_host->instance_for(s->getChild((size_t)i)); }
static Value n_find_child(VM *, Node *s, const char *n) { return g_host->instance_for(s->findChild(n)); }
static Value n_get_node(VM *, Node *s, const char *p) { return g_host->instance_for(s->getNode(p)); }
static void n_add_child(VM *vm, Node *s, Value child)
{
    if (!is_instance(child) || !as_instance(child)->native_data ||
        !zen::bind::detail::instance_is(child, ClassOf<Node>::klass))
    {
        vm->runtime_error("add_child: argument must be a live Node");
        return;
    }
    Node *c = zen_instance_data<Node>(child);
    if (c == s || c->parent)
    {
        vm->runtime_error("add_child: node '%s' already has a parent", c->getName());
        return;
    }
    g_host->adopt(c, child); /* script-created: ObjectDB + slots; before ready fires */
    s->addChild(c);
}
static void n_remove_child(VM *, Node *s, Node *c) { s->removeChild(c); }
static void n_queue_free(VM *, Node *s) { s->queueFree(); }
static void n_emit(VM *, Node *s, const char *sig) { s->emit(sig); }
static void n_connect(VM *vm, Node *s, const char *sig, Value cb) { s->connectSignal(sig, zen::Ref(*vm, cb)); }
static bool same_callable(Value a, Value b)
{
    if (is_bound_method(a) && is_bound_method(b))
        return as_bound_method(a)->receiver.as.obj == as_bound_method(b)->receiver.as.obj &&
               as_bound_method(a)->method.as.obj == as_bound_method(b)->method.as.obj;
    return is_obj(a) && is_obj(b) && a.as.obj == b.as.obj;
}
static bool n_disconnect(VM *, Node *s, const char *sig, Value cb)
{
    for (size_t i = 0; i < s->conns.size(); i++)
        if (s->conns[i].name == sig && same_callable(s->conns[i].cb.get(), cb))
        {
            s->conns.erase(s->conns.begin() + (long)i);
            return true;
        }
    return false;
}
static bool is_valid(Value v) { return is_instance(v) && as_instance(v)->native_data != nullptr; }

static Node *make_node() { Node *n = new Node(); n->gc_owned = true; return n; }
static Node3D *make_node3d() { Node3D *n = new Node3D(); n->gc_owned = true; return n; }
static Camera3D *make_camera() { Camera3D *n = new Camera3D(); n->gc_owned = true; return n; }
static MeshInstance3D *make_mesh() { MeshInstance3D *n = new MeshInstance3D(); n->gc_owned = true; return n; }

static void bind_engine(VM &vm)
{
    def_class<Node>(vm, "Node")
            .ctor<&make_node>()
            .dtor() /* owns script-created nodes; engine nodes are only wrapped borrowed */
            .property<&Node::getName, &Node::setName>("name")
            .property<&Node::isProcessing, &Node::setProcess>("processing")
            .getter<&Node::isInsideTree>("inside_tree")
            .getter<&Node::getChildCount>("child_count")
            .method<&Node::getName>("get_name")
            .method<&Node::setName>("set_name")
            .method<&n_add_child>("add_child")
            .method<&n_remove_child>("remove_child")
            .method<&n_get_child>("get_child")
            .method<&Node::getChildCount>("get_child_count")
            .method<&n_get_parent>("get_parent")
            .method<&n_find_child>("find_child")
            .method<&n_get_node>("get_node")
            .method<&Node::setProcess>("set_process")
            .method<&Node::isProcessing>("is_processing")
            .method<&Node::isInsideTree>("is_inside_tree")
            .method<&Node::isReady>("is_ready")
            .method<&Node::notification>("notification")
            .method<&n_queue_free>("queue_free")
            .method<&n_connect>("connect")
            .method<&n_disconnect>("disconnect")
            .method<&n_emit>("emit")
            .end();
    def_class<Node3D>(vm, "Node3D")
        .parent("Node")
        .ctor<&make_node3d>()
        .property<&Node3D::position>("position")
        .property<&Node3D::rotation>("rotation")
        .property<&Node3D::scale>("scale")
        .method<&Node3D::translate>("translate")
        .method<&Node3D::globalPosition>("global_position")
        .end();
    def_class<Camera3D>(vm, "Camera3D")
        .parent("Node3D")
        .ctor<&make_camera>()
        .property<&Camera3D::fov>("fov")
        .end();
    def_class<MeshInstance3D>(vm, "MeshInstance3D")
        .parent("Node3D")
        .ctor<&make_mesh>()
        .property<&MeshInstance3D::getMesh, &MeshInstance3D::setMesh>("mesh")
        .property<&MeshInstance3D::frames>("frames")
        .end();
    def_fn<&host_log>(vm, "log");
    def_fn<&host_noop>(vm, "__noop");
    def_fn<&is_valid>(vm, "is_valid");
}

/* One VM + host + tree; destruction order matters (tree before host before VM). */
struct Env
{
    VM vm;
    std::unique_ptr<Host> host;
    std::unique_ptr<SceneTree> tree;

    Env()
    {
        ZenCallbacks cb = zen_default_callbacks();
        cb.print_err = capture_err;
        vm.set_callbacks(cb);
        g_err[0] = 0;
        g_log.clear();
        vm.open_lib_globals(&zen_lib_base);
        bind_engine(vm);
        host.reset(new Host(vm));
        g_host = host.get();
        g_on_node_free = [](Node *n) { if (g_host) g_host->forget(n); };
        tree.reset(new SceneTree());
        g_tree = tree.get();
    }
    ~Env()
    {
        tree.reset(); /* deletes every node, forgets every instance */
        g_tree = nullptr;
        host.reset();
        g_host = nullptr;
        g_on_node_free = nullptr;
    }
    bool load(const char *src)
    {
        Compiler compiler;
        ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, src, "<node_engine>");
        if (!fn) return false;
        vm.run(fn);
        return !vm.had_error();
    }
    ObjClass *klass(const char *name)
    {
        Value v = vm.get_global(name);
        return is_class(v) ? as_class(v) : nullptr;
    }
    template <typename N> N *add(Node *parent, const char *name, const char *script = nullptr)
    {
        N *n = new N();
        n->name = name;
        parent->addChild(n);
        if (script) host->attach(n, klass(script));
        return n;
    }
};

/* =========================================================
** Scripts (mirrored in tests/manual/node_engine/ (one .py per script))
** ========================================================= */
static const char *SRC_SCENE = R"ZEN(
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
)ZEN";

static const char *SRC_SPAWN = R"ZEN(
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
)ZEN";

static const char *SRC_SIGNAL = R"ZEN(
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
)ZEN";

static const char *SRC_ERROR = R"ZEN(
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
)ZEN";

static const char *SRC_NOTIFY = R"ZEN(
class Watcher(Node3D):
    def _notification(self, what):
        log("notif:" + str(what))
)ZEN";

static const char *SRC_VEC = R"ZEN(
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
)ZEN";

static const char *SRC_ATTR = R"ZEN(
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
)ZEN";

static const char *SRC_DIS = R"ZEN(
class DisProbe(Node3D):
    def aug(self, dt):
        self.position.x += dt
    def whole(self, dt):
        self.position = self.position + Vec3(dt, 0, 0)
    def call(self):
        return self.position()
    def typed(self, cam: Camera3D):
        cam.fov = 40.0
        return cam.fov
)ZEN";

static const char *SRC_BENCH = R"ZEN(
class BMove(Node3D):
    def _process(self, dt):
        self.set_position(self.position() + Vec3(dt, 0, 0))

class BAttr(Node3D):
    def _process(self, dt):
        self.position = self.position + Vec3(dt, 0, 0)

class BAugX(Node3D):
    def _process(self, dt):
        self.position.x += dt

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
)ZEN";

/* =========================================================
** helpers
** ========================================================= */
static bool near_to(Value v, double want)
{
    return is_float(v) && std::fabs(v.as.number - want) < 1e-4;
}
/* the host's call sequence for an out-of-band script call */
static bool call_ok(Env &e, Value inst, const char *m, Value *out = nullptr, Value *args = nullptr, int n = 0)
{
    e.host->reset_error();
    g_err[0] = 0;
    Value r = e.vm.invoke(inst, m, args, n);
    if (out) *out = r;
    return !e.vm.had_error();
}

static double now_ms()
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
static long rss_kb()
{
    FILE *f = fopen("/proc/self/statm", "r");
    long a = 0, b = 0;
    if (f)
    {
        if (fscanf(f, "%ld %ld", &a, &b) != 2) b = 0;
        fclose(f);
    }
    return b * 4;
}

/* =========================================================
** Correctness tests
** ========================================================= */
static void test_scene()
{
    printf("-- scene / ready / process / identity --\n");
    Env e;
    e.vm.def_global("player_inst", val_nil()); /* must exist BEFORE the script that reads it is compiled */
    TEST("scene script compiles and runs");
    CHECK(e.load(SRC_SCENE), "scene script failed to compile/run");

    /* root > world(Node) > player(Node3D "p0", Player) > cam(Camera3D), gun(MeshInstance3D, Gun)
    **                    > extra(Node, no script) */
    Node *world = e.add<Node>(e.tree->root, "world");
    Node3D *player = e.add<Node3D>(world, "p0", "Player");
    Camera3D *cam = e.add<Camera3D>(player, "cam");
    MeshInstance3D *gun = e.add<MeshInstance3D>(player, "gun", "Gun");
    e.add<Node>(world, "extra");
    e.vm.set_global("player_inst", e.host->instance_for(player));

    TEST("attach() kept the Player instance alive and in the ObjectDB");
    Value pv = e.host->instance_for(player);
    CHECK(is_instance(pv) && as_instance(pv)->native_data == player &&
              as_instance(pv)->klass == e.klass("Player") && player->script && player->script->has_process &&
              player->script->has_ready && player->processing,
          "attach did not set up the instance / slots");

    TEST("class-body default speed = 2.0 is an instance default");
    Value sp;
    bool ok = call_ok(e, pv, "speed_value", &sp);
    CHECK(ok && near_to(sp, 2.0), "class body default not visible on an instance built with new_instance");

    e.tree->start();
    TEST("_ready ran once per node, children first (Godot order)");
    CHECK(e.tree->root->ready_done && count_log("ready:player children=2 parent=world") == 1 &&
              count_log("ready:gun") == 1 && index_log("ready:gun") < index_log("ready:player children=2 parent=world"),
          "ready order/count wrong");
    TEST("inherited C++ methods from _ready (get_child_count, get_parent().get_name())");
    CHECK(count_log("ready:player children=2 parent=world") == 1, "inherited call results wrong");
    TEST("set_name from script reached the C++ node");
    CHECK(player->name == "player", "set_name did not reach C++");

    bool ident_logged = count_log("ident:True") == 1;
    g_log.clear();
    for (int i = 0; i < 3; i++) e.tree->process(0.5f);
    TEST("no _ready re-run during process; _process ran each frame (script)");
    CHECK(count_log("ready:gun") == 0 && count_log("process:gun") == 3 && e.host->errors == 0,
          "ready re-ran or process count wrong");
    TEST("script _process moved the C++ position (method form, Vec3 + operator)");
    CHECK(std::fabs(player->position.x - 3.0f) < 1e-4, "position.x != 3.0");
    TEST("script wrote a C++ data member via cam.set_fov (property setter)");
    CHECK(std::fabs(cam->fov - 63.0f) < 1e-4, "fov wrong");
    FINDING("fov=%g position.x=%g err=%s", cam->fov, player->position.x, e.host->last_error.c_str());

    TEST("identity: get_parent() == the instance the engine attached");
    CHECK(ident_logged, "get_parent() returned a different wrapper");
    TEST("field set in _ready on the parent read later through get_parent()");
    CHECK(count_log("marker:42") == 1, "marker field not visible");
    TEST("field set in gun _ready read through find_child");
    Value tag;
    ok = call_ok(e, pv, "gun_tag", &tag);
    CHECK(ok && is_int(tag) && tag.as.integer == 7, "gun.tag lost");
    TEST("find_child() on a plain C++ node: stable lazily created wrapper, right class");
    Value a = e.host->instance_for(cam), b = e.host->instance_for(cam);
    CHECK(a.as.obj == b.as.obj && as_instance(a)->klass == ClassOf<Camera3D>::klass, "wrapper not stable / wrong class");
    (void)gun;

    TEST("get_node relative path returns the same wrapper");
    CHECK(e.host->instance_for(player->getNode("cam")).as.obj == a.as.obj, "path lookup identity");

    TEST("host-side vm.invoke(inst, \"get_name\") on a bound native method");
    e.host->reset_error();
    g_err[0] = 0;
    Value nm = e.vm.invoke(a, "get_name", nullptr, 0);
    bool inv_ok = !e.vm.had_error() && is_string(nm);
    FINDING("invoke() of a zen_bind native by name: %s (%s)", inv_ok ? "works" : "FAILS",
            inv_ok ? safe_string_chars(nm) : first_line(g_err).c_str());
    e.host->reset_error();
    CHECK(true, "-");

    TEST("stale script reference after C++ delete errors (no crash)");
    Ref keepRef(e.vm, a);
    player->removeChild(cam);
    delete cam;
    g_err[0] = 0;
    e.vm.invoke(keepRef.get(), "get_name", nullptr, 0);
    CHECK(e.vm.had_error() && !is_valid(keepRef.get()), "no error on stale instance");
    FINDING("stale call message: %s", first_line(g_err).c_str());
}

static void test_attach_apis()
{
    printf("-- building a script instance around an EXISTING C++ node --\n");
    {
        Env e;
        e.load("class S(Node3D):\n    def f(self):\n        return 1\n");
        Node3D *n = e.add<Node3D>(e.tree->root, "n");
        int allocs0 = g_node_allocs, frees0 = g_node_frees;
        TEST("make_instance_borrowed: no native ctor, no second node");
        Value bv = e.vm.make_instance_borrowed(e.klass("S"), n);
        ObjInstance *inst = as_instance(bv);
        CHECK(g_node_allocs == allocs0 && inst->native_data == n && inst->borrowed && inst->klass == e.klass("S"),
              "make_instance_borrowed allocated a node / did not mark the instance");
        /* the wrapper is unreferenced now: both collections free it. Before the borrowed flag the
        ** Node dtor (delete) would have taken the engine's node down with it. */
        TEST("a borrowed wrapper dying does not delete the engine's node (GC ran twice)");
        bv = val_nil();
        e.vm.collect();
        e.vm.collect();
        CHECK(g_node_frees == frees0 && e.tree->root->getChild(0) == n, "the GC deleted the engine node");
        TEST("vm.make_instance(klass) still runs the native ctor and OWNS its node");
        int a1 = g_node_allocs;
        Value v = e.vm.make_instance(e.klass("S"));
        Node *thrown = is_instance(v) ? (Node *)as_instance(v)->native_data : nullptr;
        bool ran_ctor = g_node_allocs == a1 + 1 && thrown != nullptr && thrown != n && !as_instance(v)->borrowed;
        CHECK(ran_ctor, "make_instance did not call the native ctor");
        FINDING("make_instance() allocates a fresh Node3D owned by the wrapper (deleted by the GC at collection, or "
                "once the host marks the wrapper borrowed after handing the node to the tree)");
        if (thrown) { as_instance(v)->native_data = nullptr; delete thrown; }
    }
    {
        /* a dtor on the native base: owned instances run it, borrowed ones never do */
        struct Probe { int id; };
        static int probe_dtors = 0;
        struct PR { static void d(VM *, void *) { probe_dtors++; } };
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        vm.def_class("Probe").ctor([](VM *, int, Value *) -> void * { return nullptr; }).dtor(&PR::d).end();
        Compiler compiler;
        ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, "class PS(Probe):\n    def f(self):\n        return 1\n", "<p>");
        vm.run(fn);
        ObjClass *ps = as_class(vm.get_global("PS"));
        TEST("a borrowed script SUBCLASS instance never fires the native base dtor");
        Probe engine_owned{1};
        int before = probe_dtors;
        {
            vm.make_instance_borrowed(ps, &engine_owned);
        }
        vm.collect();
        vm.collect();
        CHECK(probe_dtors == before, "dtor ran on engine memory");
        TEST("an owned instance (new_instance + native_data) still fires it once");
        {
            ObjInstance *inst = new_instance(&vm.get_gc(), ps);
            inst->native_data = &engine_owned;
        }
        vm.collect();
        vm.collect();
        CHECK(probe_dtors == before + 1, "dtor did not run for an owned instance");
    }
}

static void test_spawn()
{
    printf("-- script-created nodes / queue_free / stale refs --\n");
    Env e;
    TEST("script compiles and runs");
    CHECK(e.load(SRC_SPAWN), "spawn script failed");
    Node3D *sp = e.add<Node3D>(e.tree->root, "sp", "Spawner");
    e.tree->start();
    Value spv = e.host->instance_for(sp);
    TEST("script-created nodes were added to the tree and readied");
    CHECK(sp->getChildCount() == 2 && count_log("ready:bullet") == 1 && count_log("ready:spawner") == 1 &&
              index_log("ready:spawner") < index_log("ready:bullet"),
          "children not created in _ready");
    FINDING("log order: ready:spawner is logged by the parent's _ready, which is running while children are created, "
            "so children are readied immediately inside add_child (Godot does the same when the parent is ready)");
    TEST("script-created child ownership moved to the tree (gc_owned cleared)");
    CHECK(!sp->children[0]->gc_owned && !sp->children[1]->gc_owned, "still gc_owned");
    TEST("Bullet wrapper (script class on a script-created node) has the right class");
    Value f;
    CHECK(call_ok(e, spv, "bullet_alive", &f) && is_bool(f) && f.as.boolean &&
              as_instance(e.host->instance_for(sp->children[1]))->klass == e.klass("Bullet"),
          "bullet wrapper identity/class wrong");
    ObjInstance *spi = as_instance(spv);
    bool same_ok = false;
    for (int i = 0; i < spi->klass->num_fields; i++)
        if (strcmp(spi->klass->field_names[i]->chars, "same") == 0)
            same_ok = is_bool(spi->fields[i]) && spi->fields[i].as.boolean;
    TEST("identity of script-created children via get_child() inside the script");
    CHECK(same_ok, "get_child() returned a different wrapper than the script object");

    int mesh_frames_before = ((MeshInstance3D *)sp->children[0])->frames;
    for (int i = 0; i < 2; i++) e.tree->process(0.016f);
    TEST("script-created C++-processed node (MeshInstance3D) processed each frame");
    CHECK(((MeshInstance3D *)sp->children[0])->frames == mesh_frames_before + 2, "plain node not processed");
    TEST("no script errors during these frames");
    CHECK(e.host->errors == 0, "script errors during process");
    TEST("Bullet alive before its 3rd _process");
    CHECK(sp->getChildCount() == 2, "bullet freed too early");
    int frees0 = g_node_frees;
    e.tree->process(0.016f);
    TEST("queue_free from script removes the node after the frame");
    CHECK(sp->getChildCount() == 1 && g_node_frees == frees0 + 1, "bullet not freed");
    TEST("stale reference: is_valid() false, method call errors cleanly");
    Value alive;
    bool a_ok = call_ok(e, spv, "bullet_alive", &alive);
    bool stale_err = !call_ok(e, spv, "stale_call");
    CHECK(a_ok && is_bool(alive) && !alive.as.boolean && stale_err, "stale access not detected");
    FINDING("stale message: %s", first_line(g_err).c_str());
    TEST("double add_child reports an error");
    bool dbl = !call_ok(e, spv, "double_add");
    CHECK(dbl && strstr(g_err, "already has a parent"), "no error on double add");
    FINDING("message: %s", first_line(g_err).c_str());

}

static void test_orphans()
{
    /* ownership of an orphan script-created node: GC deletes it
    ** (a separate function: the mock's g_host/g_tree globals allow one live Env) */
    printf("-- ownership of script-created orphans --\n");
    TEST("orphan script node (never added) is deleted by the GC via the class dtor");
    Env e2;
    e2.load("def make():\n    for i in range(50):\n        n = Node3D()\n        n.set_name(\"orphan\")\n    return 1\n");
    int allocs = g_node_allocs, frees = g_node_frees;
    e2.vm.call_global("make", nullptr, 0);
    e2.vm.collect();
    e2.vm.collect();
    CHECK(g_node_allocs - allocs == 50 && g_node_frees - frees >= 49, "orphans leaked or not freed");
}

static void test_signals()
{
    printf("-- signals --\n");
    Env e;
    TEST("script compiles and runs");
    CHECK(e.load(SRC_SIGNAL), "signal script failed");
    Node *btn = e.add<Node>(e.tree->root, "btn", "Emitter");
    Node *l1 = e.add<Node>(e.tree->root, "l1", "Listener");
    Node *l2 = e.add<Node>(e.tree->root, "l2", "Listener");
    e.tree->start();
    Value bv = e.host->instance_for(btn), v1 = e.host->instance_for(l1), v2 = e.host->instance_for(l2);

    TEST("emit with no listeners is a no-op");
    btn->emit("pressed");
    CHECK(!e.vm.had_error() && e.host->errors == 0, "error emitting without listeners");

    Value args[1] = {bv};
    call_ok(e, v1, "hook", nullptr, args, 1);
    call_ok(e, v2, "hook", nullptr, args, 1);
    TEST("C++ emit calls the bound script methods");
    btn->emit("pressed");
    btn->emit("pressed");
    Value c1, c2;
    call_ok(e, v1, "get_count", &c1);
    call_ok(e, v2, "get_count", &c2);
    CHECK(is_int(c1) && c1.as.integer == 2 && is_int(c2) && c2.as.integer == 2 && btn->conns.size() == 2,
          "listener counts wrong");
    TEST("script-side emit (btn.fire() -> self.emit)");
    call_ok(e, bv, "fire");
    call_ok(e, v1, "get_count", &c1);
    CHECK(is_int(c1) && c1.as.integer == 3, "script emit did not reach listeners");
    TEST("disconnect with a FRESH bound method matches the stored one");
    Value args1[1] = {bv};
    Value dr;
    call_ok(e, v1, "unhook", &dr, args1, 1);
    btn->emit("pressed");
    call_ok(e, v1, "get_count", &c1);
    call_ok(e, v2, "get_count", &c2);
    CHECK(is_bool(dr) && dr.as.boolean && c1.as.integer == 3 && c2.as.integer == 4 && btn->conns.size() == 1,
          "disconnect failed");
    TEST("signal listener survives GC (held with vm.ref)");
    e.vm.collect();
    e.vm.collect();
    btn->emit("pressed");
    call_ok(e, v2, "get_count", &c2);
    CHECK(c2.as.integer == 5 && e.host->errors == 0, "listener collected");
    TEST("deleting the emitter node releases its refs");
    int refs = e.vm.ref_count();
    e.tree->root->removeChild(btn);
    delete btn;
    CHECK(e.vm.ref_count() < refs, "refs not released");
}

static void test_errors()
{
    printf("-- script error isolation --\n");
    Env e;
    TEST("script compiles and runs");
    CHECK(e.load(SRC_ERROR), "error script failed");
    Node3D *bad = e.add<Node3D>(e.tree->root, "bad", "Bad");
    Node3D *c1 = e.add<Node3D>(e.tree->root, "c1", "Counter");
    Node3D *c2 = e.add<Node3D>(e.tree->root, "c2", "Counter");
    e.tree->start();
    g_err[0] = 0;
    e.tree->process(0.1f);
    TEST("one script error per frame: later nodes still processed in the same frame");
    Value n1, n2;
    call_ok(e, e.host->instance_for(c1), "count", &n1);
    call_ok(e, e.host->instance_for(c2), "count", &n2);
    CHECK(e.host->errors == 1 && is_int(n1) && n1.as.integer == 1 && is_int(n2) && n2.as.integer == 1,
          "later nodes skipped or error not counted");
    FINDING("error text: %s", e.host->last_error.c_str());
    TEST("faulted node stops being processed; the others keep running");
    for (int i = 0; i < 3; i++) e.tree->process(0.1f);
    call_ok(e, e.host->instance_for(c1), "count", &n1);
    CHECK(e.host->errors == 1 && is_int(n1) && n1.as.integer == 4, "faulted node kept erroring / counters wrong");
    TEST("had_error() semantics after a failed invoke()");
    g_err[0] = 0;
    e.vm.invoke(e.host->instance_for(bad), "_process", nullptr, 0); /* x.boom() on None, no dt arg */
    bool after_fail = e.vm.had_error();
    e.vm.invoke(e.host->instance_for(c1), "count", nullptr, 0);
    bool after_good = e.vm.had_error();
    Value dtv[1] = {val_float(0.1)};
    e.vm.call_fn(e.host->instance_for(c1), dtv, 0); /* call_fn on a non-callable: error? */
    FINDING("failed invoke -> had_error=%d; next GOOD invoke -> had_error=%d (invoke never clears it); "
            "call_fn()/run() clear it on entry; there is no public reset",
            after_fail, after_good);
    CHECK(after_fail, "expected the failing invoke to set had_error");
}

static void test_vec_and_notify()
{
    printf("-- Vec3 round trip / global_position / notifications --\n");
    Env e;
    TEST("scripts compile and run");
    CHECK(e.load(SRC_VEC) && e.load(SRC_NOTIFY), "scripts failed");
    Node3D *a = e.add<Node3D>(e.tree->root, "a");
    a->position = Vec3f{1, 0, 0};
    Node3D *b = e.add<Node3D>(a, "b", "Vec");
    b->position = Vec3f{0, 2, 0};
    Value bv = e.host->instance_for(b);
    e.tree->start();
    TEST("script set_position/set_scale reach the C++ members component-wise");
    call_ok(e, bv, "set_it");
    CHECK(b->position.x == 1.5f && b->position.y == 2.5f && b->position.z == 3.5f && b->scale.x == 2 &&
              b->scale.z == 2,
          "round trip script->C++");
    TEST("host writes position, script reads it component-wise");
    b->position = Vec3f{7, 8, 9};
    Value y;
    call_ok(e, bv, "read_y", &y);
    CHECK(near_to(y, 8.0), "round trip C++->script");
    TEST("global_position() of a nested node");
    Value gp;
    call_ok(e, bv, "gp", &gp);
    CHECK(is_vec3(gp) && as_vec3(gp).x == 8 && as_vec3(gp).y == 8 && as_vec3(gp).z == 9, "global position wrong");
    TEST("translate(Vec3) from script");
    call_ok(e, bv, "move");
    CHECK(b->position.x == 8 && b->position.y == 9 && b->position.z == 10, "translate wrong");

    TEST("notification(int): script _notification (PARENTED on add_child)");
    g_log.clear();
    Node3D *w = new Node3D();
    w->name = "w";
    e.host->attach(w, e.klass("Watcher"));
    e.tree->root->addChild(w);
    CHECK(count_log("notif:18") == 1, "PARENTED not delivered");
    TEST("notification(READY) reaches _notification before the C++ handler");
    e.tree->propagateReady(w);
    CHECK(count_log("notif:13") == 1, "READY not delivered");
}

static void probe(Env &e, Value inst, const char *label, const char *method, Value *args = nullptr, int n = 0)
{
    e.host->reset_error();
    g_err[0] = 0;
    Value r = e.vm.invoke(inst, method, args, n);
    if (e.vm.had_error())
        printf("      attr probe %-34s -> ERROR: %s\n", label, first_line(g_err).c_str());
    else if (is_vec3(r))
        printf("      attr probe %-34s -> ok, Vec3(%g,%g,%g)\n", label, as_vec3(r).x, as_vec3(r).y, as_vec3(r).z);
    else if (is_float(r))
        printf("      attr probe %-34s -> ok, float %g\n", label, r.as.number);
    else
        printf("      attr probe %-34s -> ok (%s)\n", label, zen::bind::detail::type_name_of(r));
}

static bool has_field(ObjClass *k, const char *name)
{
    for (int i = 0; i < k->num_fields; i++)
        if (k->field_names[i] && strcmp(k->field_names[i]->chars, name) == 0) return true;
    return false;
}

/* The compiled shape (run `bin/test_node_engine --dis`; the zen CLI has no Node3D to ask):
**
**   def aug(self, dt):   self.position.x += dt
**     GETFIELD  R[2] = R[0].position        <- by name: the native property getter
**     GETFIELD  R[4] = R[2].x               <- field of the Vec3 value
**     ADD       R[4] = R[4] + R[1]
**     SETFIELD  R[2].x = R[4]               <- rewrites the copy in R[2]
**     SETFIELD  R[0].position = R[2]        <- write-back: the native property setter
**
**   self.position = v  ->  SETFIELD R[0].position = ...   (no SETFIELD_IDX, no class field)
**   self.position()    ->  INVOKE_VT_R ... position       (the method is still there)
** Neither `position` nor any other property is ever registered as a class field of Probe. */
static void test_attributes()
{
    printf("-- native properties: attribute syntax reaches the C++ member --\n");
    Env e;
    TEST("script compiles and runs");
    CHECK(e.load(SRC_ATTR), "attr script failed");
    Node3D *n = e.add<Node3D>(e.tree->root, "n", "Probe");
    Camera3D *cam = e.add<Camera3D>(n, "cam");
    n->position = Vec3f{1, 2, 3};
    Value nv = e.host->instance_for(n), cv = e.host->instance_for(cam);
    Value out;

    TEST("self.position / self.rotation are not class fields (no phantom slot shadowing the member)");
    CHECK(!has_field(e.klass("Probe"), "position") && !has_field(e.klass("Probe"), "rotation") &&
              !has_field(e.klass("Probe"), "scale") && !has_field(e.klass("Probe"), "fov") &&
              !has_field(e.klass("Hero"), "position"),
          "a property name became a script field");

    probe(e, nv, "self.position.x (read)", "read_x");
    TEST("self.position.x reads the C++ member");
    CHECK(call_ok(e, nv, "read_x", &out) && near_to(out, 1.0), "read_x wrong");
    probe(e, nv, "self.position.x = 5 (write)", "assign_x");
    TEST("self.position.x = 5.0 updates the C++ member (read-modify-write through the property)");
    CHECK(n->position.x == 5 && n->position.y == 2 && n->position.z == 3, "x not written");
    probe(e, nv, "self.position.x += 1", "aug_x");
    TEST("self.position.x += 1.0 updates the C++ member");
    CHECK(n->position.x == 6 && n->position.y == 2 && n->position.z == 3, "aug not written");
    probe(e, nv, "self.position = Vec3(9,9,9)", "assign_whole");
    TEST("self.position = Vec3(9,9,9) reaches the C++ member");
    CHECK(n->position.x == 9 && n->position.y == 9 && n->position.z == 9, "assignment did not reach C++");
    probe(e, nv, "self.position (read after)", "read_after_assign");
    probe(e, nv, "self.position() (call after)", "call_after_assign");
    TEST("self.position and self.position() agree with each other and with the member");
    Value r1, r2;
    n->position = Vec3f{4, 5, 6};
    CHECK(call_ok(e, nv, "read_after_assign", &r1) && call_ok(e, nv, "call_after_assign", &r2) && is_vec3(r1) &&
              is_vec3(r2) && as_vec3(r1).x == 4 && as_vec3(r2).y == 5 && as_vec3(r1).z == as_vec3(r2).z,
          "attribute and method disagree");
    TEST("the host writes the member, the script sees it on the next read (no cached copy)");
    n->position = Vec3f{7, 7, 7};
    CHECK(call_ok(e, nv, "read_x", &out) && near_to(out, 7.0), "stale read");
    Value dt[1] = {val_float(0.1)};
    probe(e, nv, "self.rotation = Vec3(..) (Spinner)", "spin", dt, 1);
    TEST("self.rotation = Vec3(0, dt, 0) reaches the C++ rotation");
    CHECK(std::fabs(n->rotation.y - 0.1f) < 1e-6 && n->rotation.x == 0, "rotation not written");

    Value ca[1] = {cv};
    probe(e, nv, "cam.fov = 33.0 (write)", "cam_attr_write", ca, 1);
    TEST("cam.fov = 33.0 writes the C++ float (property<&Camera3D::fov>)");
    CHECK(cam->fov == 33.0f, "fov not written");
    probe(e, nv, "cam.fov (read)", "cam_attr_read", ca, 1);
    probe(e, nv, "cam.fov() (getter call)", "cam_getter", ca, 1);
    TEST("cam.fov and cam.fov() both read the member");
    CHECK(call_ok(e, nv, "cam_attr_read", &r1, ca, 1) && call_ok(e, nv, "cam_getter", &r2, ca, 1) &&
              near_to(r1, 33.0) && near_to(r2, 33.0),
          "fov reads differ");
    TEST("typed receiver (cam: Camera3D): cam.fov += 2.0 goes through the property, not an index");
    CHECK(call_ok(e, nv, "cam_typed", &out, ca, 1) && near_to(out, 35.0) && cam->fov == 35.0f, "typed receiver wrong");

    TEST("a wrong value type is an error naming the setter, member unchanged");
    n->position = Vec3f{1, 1, 1};
    bool bad = !call_ok(e, nv, "bad_type") && strstr(g_err, "Vec3") != nullptr;
    FINDING("self.position = 5 -> %s", first_line(g_err).c_str());
    bool bad2 = !call_ok(e, nv, "bad_fov", nullptr, ca, 1) && strstr(g_err, "number") != nullptr;
    FINDING("cam.fov = \"wide\" -> %s", first_line(g_err).c_str());
    CHECK(bad && bad2 && n->position.x == 1 && cam->fov == 35.0f, "type errors not reported / member changed");

    TEST("read-only property (getter<&Node::isInsideTree>): writing is a clear error, no script field is created");
    bool ro = !call_ok(e, nv, "ro_write") && strstr(g_err, "'Probe.inside_tree' is read-only") != nullptr;
    FINDING("self.inside_tree = False -> %s", first_line(g_err).c_str());
    Value nargs[1] = {nv};
    bool ro2 = !call_ok(e, nv, "ro_write_other", nullptr, nargs, 1) && strstr(g_err, "'Probe.child_count' is read-only");
    CHECK(ro && ro2 && !has_field(e.klass("Probe"), "inside_tree") && !has_field(e.klass("Probe"), "child_count"),
          "read-only write not rejected / created a field");
    TEST("read-only property reads work (self.child_count == 1 child)");
    CHECK(call_ok(e, nv, "counts", &out) && is_int(out) && out.as.integer == 1, "child_count wrong");

    TEST("accessor-pair property: self.name = \"renamed\" reaches Node::setName, self.name reads getName");
    CHECK(call_ok(e, nv, "rename", &out) && is_string(out) && n->name == "renamed" &&
              strcmp(as_string(out)->chars, "renamed") == 0,
          "name pair wrong");
    TEST("accessor-pair property with bool: self.processing toggles Node::processing");
    bool before = n->processing;
    CHECK(call_ok(e, nv, "toggle", &out) && is_bool(out) && n->processing == !before && out.as.boolean == n->processing,
          "processing pair wrong");
    MeshInstance3D *mi = e.add<MeshInstance3D>(e.tree->root, "mi");
    Value ma[1] = {e.host->instance_for(mi)};
    TEST("accessor-pair const char * property: m.mesh = \"tree.obj\" / m.mesh");
    CHECK(call_ok(e, nv, "mesh_set", &out, ma, 1) && is_string(out) && mi->mesh == "tree.obj", "mesh pair wrong");

    TEST("unrelated attributes still create ordinary script fields");
    CHECK(call_ok(e, nv, "extra_field", &out) && is_int(out) && out.as.integer == 11 && has_field(e.klass("Probe"), "tag"),
          "plain field broken");
    TEST("a property read as a value is the property value, not a bound method");
    n->position = Vec3f{2, 4, 6};
    CHECK(call_ok(e, nv, "as_value", &out) && is_vec3(out) && as_vec3(out).z == 6, "bound method leaked");

    /* inheritance: Sub(Probe) and Hero(Sub) are script subclasses of a script subclass of Node3D */
    Node3D *sn = e.add<Node3D>(e.tree->root, "sn", "Sub");
    Node3D *hn = e.add<Node3D>(e.tree->root, "hn", "Hero");
    sn->position = Vec3f{1, 0, 0};
    hn->position = Vec3f{0, 0, 0};
    Value sv = e.host->instance_for(sn), hv = e.host->instance_for(hn);
    Value d2[1] = {val_float(0.5)};
    TEST("a script subclass inherits the property (Sub.sub_move: self.position.x += dt)");
    CHECK(call_ok(e, sv, "sub_move", &out, d2, 1) && near_to(out, 1.5) && sn->position.x == 1.5f, "Sub did not reach C++");
    TEST("two levels down (Hero): self.position = self.position + Vec3(1,1,1); self.scale.y = 3.0");
    CHECK(call_ok(e, hv, "hero_set", &out) && hn->position.x == 1 && hn->position.z == 1 && hn->scale.y == 3.0f &&
              hn->scale.x == 1 && is_vec3(out) && as_vec3(out).y == 3.0f,
          "Hero did not reach C++");
    TEST("the inherited property is also reachable from a base-class method on a Sub instance");
    CHECK(call_ok(e, sv, "read_x", &out) && near_to(out, 1.5), "inherited base method wrong");

    TEST("lookup order: a script field named like a property wins (Wide.fov = 5.0 class default)");
    Camera3D *wn = e.add<Camera3D>(e.tree->root, "wn", "Wide");
    Value wv = e.host->instance_for(wn);
    CHECK(call_ok(e, wv, "read", &out) && near_to(out, 5.0) && wn->fov == 70.0f, "field did not win");

    TEST("no script errors left over");
    CHECK(e.host->errors == 0, "unexpected host errors");
}

/* =========================================================
** Benchmarks
** ========================================================= */
static double best_of(int reps, int frames, SceneTree &t)
{
    double best = 1e18;
    for (int r = 0; r < reps; r++)
    {
        double t0 = now_ms();
        for (int f = 0; f < frames; f++) t.process(1.0f / 60);
        best = std::min(best, now_ms() - t0);
    }
    return best;
}

static double bench_mode(int N, int frames, const char *script)
{
    Env e;
    e.load(SRC_BENCH);
    ObjClass *k = script ? e.klass(script) : nullptr;
    for (int i = 0; i < N; i++)
    {
        Mover3D *m = new Mover3D();
        m->name = "m";
        e.tree->root->addChild(m);
        if (k) e.host->attach(m, k);
    }
    e.tree->start();
    e.tree->process(0.016f); /* warm */
    double best = best_of(5, frames, *e.tree);
    if (e.host->errors) printf("   (!) %ld script errors in bench: %s\n", e.host->errors, e.host->last_error.c_str());
    return best / frames;
}

static void run_bench(int N, int frames)
{
    printf("=== node engine benchmark: N=%d nodes, %d frames, best of 5 ===\n", N, frames);

    /* 4. memory first, before the allocator has seen the other runs */
    {
        Env e;
        e.load(SRC_BENCH);
        long rss0 = rss_kb();
        size_t gc0 = e.vm.get_gc().bytes_allocated;
        for (int i = 0; i < N; i++)
        {
            Mover3D *m = new Mover3D();
            m->name = "m";
            e.tree->root->addChild(m);
        }
        long rss1 = rss_kb();
        size_t gc1 = e.vm.get_gc().bytes_allocated;
        ObjClass *k = e.klass("BMove");
        for (Node *c : e.tree->root->children) e.host->attach(c, k);
        long rss2 = rss_kb();
        size_t gc2 = e.vm.get_gc().bytes_allocated;
        printf("memory: %d C++ nodes: RSS +%ld KB, GC +%zu B | +%d script instances (BMove): RSS +%ld KB (%.0f B/inst), "
               "GC bytes +%zu (%.0f B/inst), vm refs=%d, collections=%zu\n",
               N, rss1 - rss0, gc1 - gc0, N, rss2 - rss1, (rss2 - rss1) * 1024.0 / N, gc2 - gc1,
               (double)(gc2 - gc1) / N, e.vm.ref_count(), e.vm.get_gc().collections);
    }

    /* 1. per-frame cost */
    double a = bench_mode(N, frames, nullptr);
    double b = bench_mode(N, frames, "BMove");
    double c = bench_mode(N, frames, "BNone");
    double d = bench_mode(N, frames, "BEmpty");
    double f_attr = bench_mode(N, frames, "BAttr");
    double g_aug = bench_mode(N, frames, "BAugX");
    printf("per-frame (ms/frame, %d nodes):\n", N);
    printf("  (a) all C++ _process, no script                       : %8.4f ms  %7.1f ns/node\n", a, a * 1e6 / N);
    printf("  (b) script _process set_position(position()+Vec3)     : %8.4f ms  %7.1f ns/node\n", b, b * 1e6 / N);
    printf("  (c) script attached, NO _process override (skips VM)  : %8.4f ms  %7.1f ns/node\n", c, c * 1e6 / N);
    printf("  (d) script _process, empty body                       : %8.4f ms  %7.1f ns/node\n", d, d * 1e6 / N);
    printf("  (e) script _process self.position = self.position + Vec3  : %8.4f ms  %7.1f ns/node\n", f_attr,
           f_attr * 1e6 / N);
    printf("  (f) script _process self.position.x += dt (attribute)  : %8.4f ms  %7.1f ns/node\n", g_aug,
           g_aug * 1e6 / N);
    printf("  derived: attribute form body (e-d) = %.1f ns/node vs method form (b-d) = %.1f ns/node\n",
           (f_attr - d) * 1e6 / N, (b - d) * 1e6 / N);
    printf("  derived: call overhead (d-c) = %.1f ns/node;  body cost (b-d) = %.1f ns/node\n", (d - c) * 1e6 / N,
           (b - d) * 1e6 / N);

    /* 2. find_child / identity loop cost */
    {
        Env e;
        e.load(SRC_BENCH);
        Node3D *f = e.add<Node3D>(e.tree->root, "finder", "Finder");
        e.add<Camera3D>(f, "cam");
        e.tree->start();
        Value fv = e.host->instance_for(f);
        Value n[1] = {val_int(1000)};
        auto timeit = [&](const char *m) {
            double best = 1e18;
            for (int r = 0; r < 5; r++)
            {
                double t0 = now_ms();
                for (int i = 0; i < 100; i++) e.vm.invoke(fv, m, n, 1);
                best = std::min(best, (now_ms() - t0) / 100);
            }
            return best;
        };
        double tf = timeit("find_loop"), tp = timeit("parent_loop"), tc = timeit("count_loop");
        printf("1000 calls per frame (ms for 1000 calls; ns/call):\n");
        printf("  self.find_child(\"cam\") + identity lookup : %8.4f ms  %7.1f ns/call\n", tf, tf * 1e6 / 1000);
        printf("  self.get_parent() + identity lookup       : %8.4f ms  %7.1f ns/call\n", tp, tp * 1e6 / 1000);
        printf("  self.get_child_count() (no wrapper)       : %8.4f ms  %7.1f ns/call  <- loop+native-call baseline\n", tc,
               tc * 1e6 / 1000);
        double t0 = now_ms();
        long sink = 0;
        for (int i = 0; i < 1000000; i++) sink += (long)(e.host->instance_for(f->findChild("cam")).as.obj != nullptr);
        double host_only = (now_ms() - t0) / 1000000 * 1e6;
        printf("  host only: findChild + instance_for       : %7.1f ns/call (sink %ld)\n", host_only, sink);
    }

    /* 3. signal fan-out */
    {
        Env e;
        e.load(SRC_BENCH);
        Node *btn = e.add<Node>(e.tree->root, "btn");
        Value bv = e.host->instance_for(btn);
        std::vector<Node *> subs;
        for (int i = 0; i < N; i++)
        {
            Node *s = e.add<Node>(e.tree->root, "sub", "Sub");
            subs.push_back(s);
            Value a[1] = {bv};
            e.vm.invoke(e.host->instance_for(s), "hook", a, 1);
        }
        double best = 1e18;
        for (int r = 0; r < 5; r++)
        {
            double t0 = now_ms();
            btn->emit("pressed");
            best = std::min(best, now_ms() - t0);
        }
        printf("signal: one emit to %d bound-method listeners: %.3f ms (%.0f ns/listener), errors=%ld, "
               "first listener count=%lld (5 emits)\n",
               N, best, best * 1e6 / N, e.host->errors,
               (long long)as_instance(e.host->instance_for(subs[0]))->fields[0].as.integer);
    }
}

/* --dis: the disassembly of the attribute forms (what `zen --dis-only` would
** print if the zen CLI had Node3D bound). Documented shape, see test_attributes(). */
static void run_dis()
{
    Env e;
    if (!e.load(SRC_DIS)) return;
    ObjClass *k = e.klass("DisProbe");
    for (int i = 0; k && i < k->vtable_size; i++)
        if (is_closure(k->vtable[i]) && as_closure(k->vtable[i])->func->name)
            disassemble_func(as_closure(k->vtable[i])->func, as_closure(k->vtable[i])->func->name->chars, &e.vm);
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--dis") == 0)
    {
        run_dis();
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--bench") == 0)
    {
        int N = argc >= 3 ? atoi(argv[2]) : 10000;
        int F = argc >= 4 ? atoi(argv[3]) : 100;
        run_bench(N, F);
        return 0;
    }

    printf("=== node engine mock (zen_bind) ===\n");
    test_scene();
    test_attach_apis();
    test_spawn();
    test_orphans();
    test_signals();
    test_errors();
    test_vec_and_notify();
    test_attributes();
    printf("\n%d passed, %d failed\n", g_tests_passed, g_tests_failed);
    return g_tests_failed ? 1 : 0;
}
