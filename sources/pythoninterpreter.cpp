// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell

#include "pythoninterpreter.h"
#include <QDebug>
#include <QPointer>

#undef slots
#include <Python.h>
#define slots Q_SLOTS

extern "C" PyObject*
PyInit_usdviewer_wrapper();

namespace usdviewer {

// ------------------------------------------------------------
// Private
// ------------------------------------------------------------

class PythonInterpreterPrivate {
public:
    PythonInterpreterPrivate();
    ~PythonInterpreterPrivate();

    void init();
    void release();

    QString executeScript(const QString& script) const;
    QString pythonError() const;

public:
    struct Data {
        QPointer<PythonInterpreter> interpreter;
        PyObject* globals = nullptr;
        PyObject* locals = nullptr;
        bool initialized = false;
    };
    Data d;
};

PythonInterpreterPrivate::PythonInterpreterPrivate() {}

PythonInterpreterPrivate::~PythonInterpreterPrivate() { release(); }

// ------------------------------------------------------------
// Init
// ------------------------------------------------------------

void
PythonInterpreterPrivate::init()
{
    if (d.initialized)
        return;

    qDebug() << "[Python] Initializing interpreter";
    
    if (PyImport_AppendInittab("usdviewer", PyInit_usdviewer_wrapper) == -1) {
        qFatal("Failed to add usdviewer module");
    }
    Py_Initialize();
    PyRun_SimpleString("print('[Python] Interpreter started')");

    // todo: this is temporary, will be replaced
    PyRun_SimpleString(R"(
import sys
sys.path.insert(0, "/Volumes/Build/pipeline/3rdparty/build/macosx/arm64.debug/lib/python")
print("[Python] sys.path:", sys.path)
)");

    PyObject* mainModule = PyImport_AddModule("__main__");
    PyObject* mainDict = PyModule_GetDict(mainModule);

    d.globals = mainDict;
    Py_INCREF(d.globals);

    d.locals = d.globals;
    Py_INCREF(d.locals);

    PyObject* module = PyImport_ImportModule("usdviewer");
    if (!module) {
        PyErr_Print();
        qWarning() << "[Python] Failed to import usdviewer module";
        return;
    }

    qDebug() << "[Python] usdviewer module imported";

    PyObject* cls = PyObject_GetAttrString(module, "Session");
    if (!cls || !PyCallable_Check(cls)) {
        PyErr_Print();
        qWarning() << "[Python] Failed to get Session class";
        Py_XDECREF(cls);
        Py_DECREF(module);
        return;
    }
    PyObject* instance = PyObject_CallObject(cls, nullptr);
    if (!instance) {
        PyErr_Print();
        qWarning() << "[Python] Failed to create Session instance";
        Py_DECREF(cls);
        Py_DECREF(module);
        return;
    }

    PyDict_SetItemString(d.globals, "session", instance);

    Py_DECREF(instance);
    Py_DECREF(cls);
    Py_DECREF(module);

    PyObject* key = PyUnicode_FromString("session");
    int has = PyDict_Contains(d.globals, key);
    Py_DECREF(key);

    qDebug() << "[Python] session injected =" << has;

    PyRun_SimpleString(R"(
print("[Python] globals after init:", list(globals().keys()))
print("[Python] session exists:", 'session' in globals())
)");

    d.initialized = true;
}

void
PythonInterpreterPrivate::release()
{
    if (!d.initialized)
        return;

    qDebug() << "[Python] Releasing interpreter";

    PyGILState_STATE gil = PyGILState_Ensure();

    Py_XDECREF(d.globals);
    Py_XDECREF(d.locals);

    d.globals = nullptr;
    d.locals = nullptr;

    PyGILState_Release(gil);

    Py_Finalize();

    d.initialized = false;
}

QString
PythonInterpreterPrivate::executeScript(const QString& script) const
{
    if (!d.initialized)
        return "Python not initialized";

    QString output;
    PyGILState_STATE gil = PyGILState_Ensure();
    try {
        QByteArray scriptBytes = script.toUtf8();
        const char* code = scriptBytes.constData();
        PyObject* result = PyRun_String(code, Py_file_input, d.globals, d.locals);

        if (result) {
            if (result != Py_None) {
                PyObject* str = PyObject_Str(result);
                if (str) {
                    output = QString::fromUtf8(PyUnicode_AsUTF8(str));
                    Py_DECREF(str);
                }
            }
            Py_DECREF(result);
        }
        else {
            output = pythonError();
        }
    } catch (const std::exception& e) {
        output = QString("[C++ Exception] ") + e.what();
    } catch (...) {
        output = "[C++ Exception] Unknown error";
    }
    PyGILState_Release(gil);
    return output;
}

QString
PythonInterpreterPrivate::pythonError() const
{
    PyObject *type = nullptr, *value = nullptr, *traceback = nullptr;

    PyErr_Fetch(&type, &value, &traceback);
    PyErr_NormalizeException(&type, &value, &traceback);

    QString msg;
    if (type) {
        PyObject* s = PyObject_Str(type);
        if (s) {
            msg += QString::fromUtf8(PyUnicode_AsUTF8(s));
            Py_DECREF(s);
        }
        Py_DECREF(type);
    }

    if (value) {
        msg += ": ";
        PyObject* s = PyObject_Str(value);
        if (s) {
            msg += QString::fromUtf8(PyUnicode_AsUTF8(s));
            Py_DECREF(s);
        }
        Py_DECREF(value);
    }

    if (traceback) {
        msg += "\nTraceback:\n";

        PyObject* tbModule = PyImport_ImportModule("traceback");
        if (tbModule) {
            PyObject* fmt = PyObject_GetAttrString(tbModule, "format_tb");
            if (fmt && PyCallable_Check(fmt)) {
                PyObject* list = PyObject_CallFunctionObjArgs(fmt, traceback, nullptr);
                if (list) {
                    PyObject* str = PyUnicode_Join(PyUnicode_FromString(""), list);
                    if (str) {
                        msg += QString::fromUtf8(PyUnicode_AsUTF8(str));
                        Py_DECREF(str);
                    }
                    Py_DECREF(list);
                }
            }
            Py_XDECREF(fmt);
            Py_DECREF(tbModule);
        }

        Py_DECREF(traceback);
    }

    return msg;
}

PythonInterpreter::PythonInterpreter(QObject* parent)
    : QObject(parent)
    , p(new PythonInterpreterPrivate())
{
    p->d.interpreter = this;
    p->init();
}

PythonInterpreter::~PythonInterpreter() = default;

QString
PythonInterpreter::executeScript(const QString& script) const
{
    return p->executeScript(script);
}

}  // namespace usdviewer
