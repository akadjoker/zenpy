/* CPython host for the raylib bunnymark. */
#include "host.h"

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <cstdio>
#include <cstdlib>

static PyObject *p_draw_bunny(PyObject *, PyObject *args)
{
    double x, y;
    if (!PyArg_ParseTuple(args, "dd", &x, &y)) return nullptr;
    host_draw_bunny(x, y);
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
    { "draw_bunny", p_draw_bunny, METH_VARARGS, nullptr },
    { "rand", p_rand, METH_VARARGS, nullptr },
    { "screen_width", p_screen_width, METH_NOARGS, nullptr },
    { "screen_height", p_screen_height, METH_NOARGS, nullptr },
    { nullptr, nullptr, 0, nullptr }
};
static PyModuleDef native_module = { PyModuleDef_HEAD_INIT, "native", nullptr, -1, native_methods, nullptr, nullptr, nullptr, nullptr };
static PyObject *PyInit_native() { return PyModule_Create(&native_module); }

struct PyBoot
{
    const char *src;
    bool booted;
    PyObject *add;
    PyObject *update;
};

static bool py_boot(PyBoot *b)
{
    PyObject *main_mod = PyImport_AddModule("__main__");
    PyObject *globals = PyModule_GetDict(main_mod);
    PyObject *r = PyRun_String(b->src, Py_file_input, globals, globals);
    if (!r)
    {
        PyErr_Print();
        return false;
    }
    Py_DECREF(r);
    b->add = PyDict_GetItemString(globals, "add_bunnies");
    b->update = PyDict_GetItemString(globals, "update_all");
    if (!b->add || !b->update)
    {
        fprintf(stderr, "python: add_bunnies/update_all not defined\n");
        return false;
    }
    Py_INCREF(b->add);
    Py_INCREF(b->update);
    return true;
}

static bool py_call(PyObject *fn, const char *fmt, double v)
{
    PyObject *r = fmt[0] == 'i' ? PyObject_CallFunction(fn, "i", (int)v)
                                : PyObject_CallFunction(fn, "d", v);
    if (!r)
    {
        PyErr_Print();
        return false;
    }
    Py_DECREF(r);
    return true;
}

int main(int argc, char **argv)
{
    char *source = host_read_script(argv[0], "bunny.py");
    if (!source)
        return 1;

    PyImport_AppendInittab("native", &PyInit_native);
    Py_Initialize();

    PyBoot boot = { source, false, nullptr, nullptr };
    BunnyHost host;
    host.ud = &boot;
    host.add_bunnies = [](void *ud, int n) -> bool {
        PyBoot *b = (PyBoot *)ud;
        if (!b->booted)
        {
            if (!py_boot(b))
                return false;
            b->booted = true;
        }
        return py_call(b->add, "i", n);
    };
    host.update_all = [](void *ud, double dt) -> bool { return py_call(((PyBoot *)ud)->update, "d", dt); };

    int rc = host_run(argc, argv, "python", &host);
    Py_XDECREF(boot.add);
    Py_XDECREF(boot.update);
    Py_Finalize();
    free(source);
    return rc;
}
