// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell

#include "application.h"
#include "session.h"

#undef slots
#include <Python.h>
#define slots Q_SLOTS

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace usdviewer;

static PyModuleDef pysessionModule = { PyModuleDef_HEAD_INIT,
                                       "usdviewer",
                                       "Python interface for usdviewer Session",
                                       -1,
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       nullptr };

typedef struct {
    PyObject_HEAD Session* session;
} PySessionObject;

static void
PySession_dealloc(PySessionObject* self)
{
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject*
PySession_new(PyTypeObject* type, PyObject*, PyObject*)
{
    PySessionObject* self = (PySessionObject*)type->tp_alloc(type, 0);
    if (self) {
        self->session = session();  // global accessor
        qDebug() << "[PySession_new] session ptr =" << self->session;
        if (!self->session) {
            qWarning() << "[PySession_new] ERROR: session() returned nullptr";
        }
    }
    return (PyObject*)self;
}

static PyObject*
PySession_load(PySessionObject* self, PyObject* args)
{
    const char* filename;
    if (!PyArg_ParseTuple(args, "s", &filename))
        return nullptr;

    bool ok = self->session->loadFromFile(QString::fromUtf8(filename));
    return PyBool_FromLong(ok);
}

static PyObject*
PySession_save(PySessionObject* self, PyObject* args)
{
    const char* filename;
    if (!PyArg_ParseTuple(args, "s", &filename))
        return nullptr;

    bool ok = self->session->saveToFile(QString::fromUtf8(filename));
    return PyBool_FromLong(ok);
}

static PyObject*
PySession_filename(PySessionObject* self)
{
    QString name = self->session->filename();
    return PyUnicode_FromString(name.toUtf8().constData());
}

static PyObject*
PySession_isLoaded(PySessionObject* self)
{
    return PyBool_FromLong(self->session->isLoaded());
}

static PyObject*
PySession_close(PySessionObject* self)
{
    self->session->close();
    Py_RETURN_NONE;
}

static PyObject*
PySession_stage(PySessionObject*)
{
    Session* s = session();
    if (!s) {
        PyErr_SetString(PyExc_RuntimeError, "Session not available");
        return nullptr;
    }

    UsdStageRefPtr stage = s->stage();
    if (!stage)
        Py_RETURN_NONE;

    std::string identifier = stage->GetRootLayer()->GetIdentifier();
    PyObject* usdModule = PyImport_ImportModule("pxr.Usd");
    if (!usdModule) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to import pxr.Usd");
        return nullptr;
    }

    PyObject* stageClass = PyObject_GetAttrString(usdModule, "Stage");
    Py_DECREF(usdModule);

    if (!stageClass) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to access Usd.Stage");
        return nullptr;
    }

    PyObject* openFn = PyObject_GetAttrString(stageClass, "Open");
    Py_DECREF(stageClass);

    if (!openFn || !PyCallable_Check(openFn)) {
        PyErr_SetString(PyExc_RuntimeError, "Usd.Stage.Open not callable");
        Py_XDECREF(openFn);
        return nullptr;
    }
    PyObject* arg = PyUnicode_FromString(identifier.c_str());
    PyObject* pyStage = PyObject_CallFunctionObjArgs(openFn, arg, nullptr);

    Py_DECREF(arg);
    Py_DECREF(openFn);

    if (!pyStage) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to open stage in Python");
        return nullptr;
    }
    return pyStage;
}

static PyObject*
PySession_selection(PySessionObject* self)
{
    // todo: hook to SelectionModel
    Py_RETURN_NONE;
}

static PyObject*
PySession_paths(PySessionObject* self)
{
    // todo: return selected paths
    Py_RETURN_NONE;
}

static PyMethodDef PySession_methods[]
    = { { "load", (PyCFunction)PySession_load, METH_VARARGS, "Load a USD stage" },
        { "save", (PyCFunction)PySession_save, METH_VARARGS, "Save the stage" },
        { "filename", (PyCFunction)PySession_filename, METH_NOARGS, "Get current filename" },
        { "isLoaded", (PyCFunction)PySession_isLoaded, METH_NOARGS, "Check if stage is loaded" },
        { "close", (PyCFunction)PySession_close, METH_NOARGS, "Close the stage" },

        { "stage", (PyCFunction)PySession_stage, METH_NOARGS, "Get USD stage" },
        { "selection", (PyCFunction)PySession_selection, METH_NOARGS, "Get selection" },
        { "paths", (PyCFunction)PySession_paths, METH_NOARGS, "Get selected paths" },

        { nullptr } };

static PyTypeObject PySessionType = {
    PyVarObject_HEAD_INIT(nullptr, 0).tp_name = "usdviewer.Session",
    .tp_basicsize = sizeof(PySessionObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PySession_new,
    .tp_dealloc = (destructor)PySession_dealloc,
    .tp_methods = PySession_methods,
};

PyMODINIT_FUNC
PyInit_usdviewer(void)
{
    PyObject* module;

    if (PyType_Ready(&PySessionType) < 0)
        return nullptr;

    module = PyModule_Create(&pysessionModule);
    if (!module)
        return nullptr;

    Py_INCREF(&PySessionType);
    if (PyModule_AddObject(module, "Session", (PyObject*)&PySessionType) < 0) {
        Py_DECREF(&PySessionType);
        Py_DECREF(module);
        return nullptr;
    }
    return module;
}

extern "C" PyObject*
PyInit_usdviewer_wrapper()
{
    return PyInit_usdviewer();
}
