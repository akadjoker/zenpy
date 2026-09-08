/* CPython host for the texmark: native.Texture is a C extension type. */
#include "host.h"

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <cstdio>
#include <cstdlib>

struct TexObject { PyObject_HEAD int id; };

static PyObject *tex_new(PyTypeObject *type, PyObject *args, PyObject *)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s", &path)) return nullptr;
    int id = host_load_texture(path);
    if (id == 0) { PyErr_Format(PyExc_RuntimeError, "Texture: cannot load '%s'", path); return nullptr; }
    TexObject *self = (TexObject *)type->tp_alloc(type, 0);
    if (!self) return nullptr;
    self->id = id;
    return (PyObject *)self;
}
static void tex_dealloc(PyObject *o)
{
    TexObject *self = (TexObject *)o;
    if (self->id) host_unload_texture(self->id);
    Py_TYPE(o)->tp_free(o);
}
static PyObject *tex_draw(PyObject *o, PyObject *args)
{
    double x, y;
    if (!PyArg_ParseTuple(args, "dd", &x, &y)) return nullptr;
    host_draw_texture(((TexObject *)o)->id, x, y);
    Py_RETURN_NONE;
}
static PyObject *tex_width(PyObject *o, PyObject *)  { return PyLong_FromLong(host_texture_width(((TexObject *)o)->id)); }
static PyObject *tex_height(PyObject *o, PyObject *) { return PyLong_FromLong(host_texture_height(((TexObject *)o)->id)); }

static PyMethodDef tex_methods[] = {
    { "draw", tex_draw, METH_VARARGS, nullptr },
    { "width", tex_width, METH_NOARGS, nullptr },
    { "height", tex_height, METH_NOARGS, nullptr },
    { nullptr, nullptr, 0, nullptr }
};

static PyTypeObject TexType = { PyVarObject_HEAD_INIT(nullptr, 0) };

static PyObject *p_draw_texture(PyObject *, PyObject *args)
{
    PyObject *tex;
    double x, y;
    if (!PyArg_ParseTuple(args, "O!dd", &TexType, &tex, &x, &y)) return nullptr;
    host_draw_texture(((TexObject *)tex)->id, x, y);
    Py_RETURN_NONE;
}
static PyObject *p_rand(PyObject *, PyObject *args)
{
    double lo, hi;
    if (!PyArg_ParseTuple(args, "dd", &lo, &hi)) return nullptr;
    return PyFloat_FromDouble(host_rand(lo, hi));
}
static PyObject *p_screen_width(PyObject *, PyObject *) { return PyLong_FromLong(host_screen_width()); }
static PyObject *p_screen_height(PyObject *, PyObject *) { return PyLong_FromLong(host_screen_height()); }

static PyMethodDef native_methods[] = {
    { "draw_texture", p_draw_texture, METH_VARARGS, nullptr },
    { "rand", p_rand, METH_VARARGS, nullptr },
    { "screen_width", p_screen_width, METH_NOARGS, nullptr },
    { "screen_height", p_screen_height, METH_NOARGS, nullptr },
    { nullptr, nullptr, 0, nullptr }
};
static PyModuleDef native_module = { PyModuleDef_HEAD_INIT, "native", nullptr, -1, native_methods, nullptr, nullptr, nullptr, nullptr };
static PyObject *PyInit_native()
{
    TexType.tp_name = "native.Texture";
    TexType.tp_basicsize = sizeof(TexObject);
    TexType.tp_flags = Py_TPFLAGS_DEFAULT;
    TexType.tp_new = tex_new;
    TexType.tp_dealloc = tex_dealloc;
    TexType.tp_methods = tex_methods;
    if (PyType_Ready(&TexType) < 0) return nullptr;
    PyObject *m = PyModule_Create(&native_module);
    if (!m) return nullptr;
    Py_INCREF(&TexType);
    PyModule_AddObject(m, "Texture", (PyObject *)&TexType);
    return m;
}

struct PyBoot { const char *src; bool booted; PyObject *add; PyObject *update; };

static bool py_boot(PyBoot *b)
{
    PyObject *main_mod = PyImport_AddModule("__main__");
    PyObject *globals = PyModule_GetDict(main_mod);
    PyObject *r = PyRun_String(b->src, Py_file_input, globals, globals);
    if (!r) { PyErr_Print(); return false; }
    Py_DECREF(r);
    b->add = PyDict_GetItemString(globals, "add_sprites");
    b->update = PyDict_GetItemString(globals, "update_all");
    if (!b->add || !b->update) { fprintf(stderr, "python: add_sprites/update_all not defined\n"); return false; }
    Py_INCREF(b->add);
    Py_INCREF(b->update);
    return true;
}

static bool py_call(PyObject *fn, const char *fmt, double v)
{
    PyObject *r = fmt[0] == 'i' ? PyObject_CallFunction(fn, "i", (int)v) : PyObject_CallFunction(fn, "d", v);
    if (!r) { PyErr_Print(); return false; }
    Py_DECREF(r);
    return true;
}

int main(int argc, char **argv)
{
    char *source = host_read_script(argv[0], "sprites.py");
    if (!source)
        return 1;

    PyImport_AppendInittab("native", &PyInit_native);
    Py_Initialize();

    PyBoot boot = { source, false, nullptr, nullptr };
    TexHost host;
    host.ud = &boot;
    host.add_sprites = [](void *ud, int n, double x, double y) -> bool {
        PyBoot *b = (PyBoot *)ud;
        if (!b->booted)
        {
            if (!py_boot(b))
                return false;
            b->booted = true;
        }
        PyObject *r = PyObject_CallFunction(b->add, "idd", n, x, y);
        if (!r) { PyErr_Print(); return false; }
        Py_DECREF(r);
        return true;
    };
    host.update_all = [](void *ud, double dt) -> bool { return py_call(((PyBoot *)ud)->update, "d", dt); };

    int rc = host_run(argc, argv, "python", &host);
    Py_XDECREF(boot.add);
    Py_XDECREF(boot.update);
    Py_Finalize();
    free(source);
    return rc;
}
