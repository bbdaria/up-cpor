#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "LogicalUtilities/Predicate.h"
#include "LogicalUtilities/PredicateNode.h"

extern PyTypeObject PyPredicateType;
extern PyTypeObject PyPredicateNodeType;

typedef struct {
    PyObject_HEAD
    std::shared_ptr<Predicate>* cpp_obj;
} PyPredicate;

static void PyPredicate_dealloc(PyPredicate* self) {
    if (self->cpp_obj) delete self->cpp_obj;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static int PyPredicate_init(PyPredicate* self, PyObject* args, PyObject* kwds) {
    const char* name;
    if (!PyArg_ParseTuple(args, "s", &name)) return -1;
    self->cpp_obj = new std::shared_ptr<Predicate>(std::make_shared<Predicate>(std::string(name)));
    return 0;
}

static PyObject* PyPredicate_get_name(PyPredicate* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->cpp_obj || !(*self->cpp_obj)) {
        PyErr_SetString(PyExc_RuntimeError, "Uninitialized Predicate");
        return NULL;
    }
    std::string name = (*self->cpp_obj)->get_name();
    return PyUnicode_FromString(name.c_str());
}

static PyMethodDef PyPredicate_methods[] = {
    {"get_name", (PyCFunction)PyPredicate_get_name, METH_NOARGS, "Get the predicate name"},
    {NULL}
};

PyTypeObject PyPredicateType = { PyVarObject_HEAD_INIT(NULL, 0) };

typedef struct {
    PyObject_HEAD
    std::shared_ptr<PredicateNode>* cpp_obj;
} PyPredicateNode;

static void PyPredicateNode_dealloc(PyPredicateNode* self) {
    if (self->cpp_obj) delete self->cpp_obj;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static int PyPredicateNode_init(PyPredicateNode* self, PyObject* args, PyObject* kwds) {
    PyObject* py_pred;
    if (!PyArg_ParseTuple(args, "O", &py_pred)) return -1;

    if (!PyObject_TypeCheck(py_pred, &PyPredicateType)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a cpor_engine.Predicate");
        return -1;
    }

    PyPredicate* wrapped_pred = (PyPredicate*)py_pred;
    self->cpp_obj = new std::shared_ptr<PredicateNode>(
        std::make_shared<PredicateNode>(*(wrapped_pred->cpp_obj))
    );
    return 0;
}

static PyObject* PyPredicateNode_is_true(PyPredicateNode* self, PyObject* args) {
    PyObject* py_list;
    if (!PyArg_ParseTuple(args, "O", &py_list)) return NULL;
    if (!PyList_Check(py_list)) {
        PyErr_SetString(PyExc_TypeError, "State must be a Python list of Predicates");
        return NULL;
    }

    std::unordered_set<std::shared_ptr<Predicate>> c_state;
    Py_ssize_t size = PyList_Size(py_list);
    for (Py_ssize_t i = 0; i < size; i++) {
        PyObject* item = PyList_GetItem(py_list, i);
        if (PyObject_TypeCheck(item, &PyPredicateType)) {
            PyPredicate* wrapped_item = (PyPredicate*)item;
            c_state.insert(*(wrapped_item->cpp_obj));
        }
    }

    bool result = (*self->cpp_obj)->is_true(c_state);
    if (result) { Py_RETURN_TRUE; } else { Py_RETURN_FALSE; }
}

static PyMethodDef PyPredicateNode_methods[] = {
    {"is_true", (PyCFunction)PyPredicateNode_is_true, METH_VARARGS, "Evaluate node against a state list"},
    {NULL}
};


PyTypeObject PyPredicateNodeType = { PyVarObject_HEAD_INIT(NULL, 0) };


static struct PyModuleDef cpor_engine_module = {
    PyModuleDef_HEAD_INIT,
    "cpor_engine",
    "CPOR C++ Logical Extension Module (Native C-API)",
    -1,
    NULL
};

PyMODINIT_FUNC PyInit_cpor_engine(void) {
    PyObject* m;


    PyPredicateType.tp_name = "cpor_engine.Predicate";
    PyPredicateType.tp_basicsize = sizeof(PyPredicate);
    PyPredicateType.tp_dealloc = (destructor)PyPredicate_dealloc;
    PyPredicateType.tp_flags = Py_TPFLAGS_DEFAULT;
    PyPredicateType.tp_doc = "A Grounded Planning Predicate";
    PyPredicateType.tp_methods = PyPredicate_methods;
    PyPredicateType.tp_init = (initproc)PyPredicate_init;
    PyPredicateType.tp_new = PyType_GenericNew;


    PyPredicateNodeType.tp_name = "cpor_engine.PredicateNode";
    PyPredicateNodeType.tp_basicsize = sizeof(PyPredicateNode);
    PyPredicateNodeType.tp_dealloc = (destructor)PyPredicateNode_dealloc;
    PyPredicateNodeType.tp_flags = Py_TPFLAGS_DEFAULT;
    PyPredicateNodeType.tp_doc = "AST Leaf Node";
    PyPredicateNodeType.tp_methods = PyPredicateNode_methods;
    PyPredicateNodeType.tp_init = (initproc)PyPredicateNode_init;
    PyPredicateNodeType.tp_new = PyType_GenericNew;


    if (PyType_Ready(&PyPredicateType) < 0) return NULL;
    if (PyType_Ready(&PyPredicateNodeType) < 0) return NULL;


    m = PyModule_Create(&cpor_engine_module);
    if (m == NULL) return NULL;


    Py_INCREF(&PyPredicateType);
    if (PyModule_AddObject(m, "Predicate", (PyObject *)&PyPredicateType) < 0) {
        Py_DECREF(&PyPredicateType);
        Py_DECREF(m);
        return NULL;
    }


    Py_INCREF(&PyPredicateNodeType);
    if (PyModule_AddObject(m, "PredicateNode", (PyObject *)&PyPredicateNodeType) < 0) {
        Py_DECREF(&PyPredicateNodeType);
        Py_DECREF(&PyPredicateType);
        Py_DECREF(m);
        return NULL;
    }

    return m;
}