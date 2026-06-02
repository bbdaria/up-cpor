#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "LogicalUtilities/Predicate.h"
#include "LogicalUtilities/PredicateNode.h"
#include "LogicalUtilities/AndNode.h"
#include "LogicalUtilities/OrNode.h"
#include "LogicalUtilities/NotNode.h"

// 1. Module Definition
static struct PyModuleDef cpor_engine_module = {
    PyModuleDef_HEAD_INIT, "cpor_engine", "CPOR C++ Logical Extension Module", -1, NULL
};

// 2. Global Type Definitions
static PyTypeObject PyPredicateType = { PyVarObject_HEAD_INIT(NULL, 0) };
static PyTypeObject PyPredicateNodeType = { PyVarObject_HEAD_INIT(NULL, 0) };
static PyTypeObject PyAndNodeType = { PyVarObject_HEAD_INIT(NULL, 0) };
static PyTypeObject PyOrNodeType = { PyVarObject_HEAD_INIT(NULL, 0) };
static PyTypeObject PyNotNodeType = { PyVarObject_HEAD_INIT(NULL, 0) };

typedef struct { PyObject_HEAD std::shared_ptr<Predicate>* cpp_obj; } PyPredicate;
typedef struct { PyObject_HEAD std::shared_ptr<PredicateNode>* cpp_obj; } PyPredicateNode;
typedef struct { PyObject_HEAD std::shared_ptr<AndNode>* cpp_obj; } PyAndNode;
typedef struct { PyObject_HEAD std::shared_ptr<OrNode>* cpp_obj; } PyOrNode;
typedef struct { PyObject_HEAD std::shared_ptr<NotNode>* cpp_obj; } PyNotNode;

// 3. Init Functions (Allocation)
static int PyAndNode_init(PyAndNode* self, PyObject* args, PyObject* kwds) { self->cpp_obj = new std::shared_ptr<AndNode>(std::make_shared<AndNode>()); return 0; }
static int PyOrNode_init(PyOrNode* self, PyObject* args, PyObject* kwds) { self->cpp_obj = new std::shared_ptr<OrNode>(std::make_shared<OrNode>()); return 0; }
static int PyNotNode_init(PyNotNode* self, PyObject* args, PyObject* kwds) { self->cpp_obj = new std::shared_ptr<NotNode>(std::make_shared<NotNode>()); return 0; }

// 4. Helper: Extract generic formula pointer
std::shared_ptr<Formula> extract_formula(PyObject* self) {
    if (!self) return nullptr;
    if (PyObject_TypeCheck(self, &PyPredicateNodeType)) return *(((PyPredicateNode*)self)->cpp_obj);
    if (PyObject_TypeCheck(self, &PyAndNodeType)) return *(((PyAndNode*)self)->cpp_obj);
    if (PyObject_TypeCheck(self, &PyOrNodeType)) return *(((PyOrNode*)self)->cpp_obj);
    if (PyObject_TypeCheck(self, &PyNotNodeType)) return *(((PyNotNode*)self)->cpp_obj);
    return nullptr;
}

// 5. The shared 'is_true' method
static PyObject* ASTNode_is_true(PyObject* self, PyObject* args) {
    PyObject* py_list;
    if (!PyArg_ParseTuple(args, "O", &py_list)) return NULL;
    
    std::unordered_set<std::shared_ptr<Predicate>> c_state;
    if (PyList_Check(py_list)) {
        for (Py_ssize_t i = 0; i < PyList_Size(py_list); i++) {
            PyObject* item = PyList_GetItem(py_list, i);
            if (PyObject_TypeCheck(item, &PyPredicateType)) {
                PyPredicate* w = (PyPredicate*)item;
                if (w->cpp_obj) c_state.insert(*w->cpp_obj);
            }
        }
    }
    
    auto formula = extract_formula(self);
    if (!formula) { PyErr_SetString(PyExc_RuntimeError, "C++ Node uninitialized"); return NULL; }
    return formula->is_true(c_state) ? Py_True : Py_False;
}

static PyMethodDef ASTNode_methods[] = {{"is_true", (PyCFunction)ASTNode_is_true, METH_VARARGS, NULL}, {NULL}};

// 6. Deallocators
static void PyPredicate_dealloc(PyPredicate* self) { delete self->cpp_obj; Py_TYPE(self)->tp_free((PyObject*)self); }
static void PyPredicateNode_dealloc(PyPredicateNode* self) { delete self->cpp_obj; Py_TYPE(self)->tp_free((PyObject*)self); }
static void PyAndNode_dealloc(PyAndNode* self) { delete self->cpp_obj; Py_TYPE(self)->tp_free((PyObject*)self); }
static void PyOrNode_dealloc(PyOrNode* self) { delete self->cpp_obj; Py_TYPE(self)->tp_free((PyObject*)self); }
static void PyNotNode_dealloc(PyNotNode* self) { delete self->cpp_obj; Py_TYPE(self)->tp_free((PyObject*)self); }

// 7. Initialization
PyMODINIT_FUNC PyInit_cpor_engine(void) {
    PyPredicateType.tp_name = "cpor_engine.Predicate"; PyPredicateType.tp_basicsize = sizeof(PyPredicate); PyPredicateType.tp_dealloc = (destructor)PyPredicate_dealloc; PyPredicateType.tp_new = PyType_GenericNew;
    
    PyPredicateNodeType.tp_name = "cpor_engine.PredicateNode"; PyPredicateNodeType.tp_basicsize = sizeof(PyPredicateNode); PyPredicateNodeType.tp_dealloc = (destructor)PyPredicateNode_dealloc; PyPredicateNodeType.tp_methods = ASTNode_methods; PyPredicateNodeType.tp_new = PyType_GenericNew;
    
    PyAndNodeType.tp_name = "cpor_engine.AndNode"; PyAndNodeType.tp_basicsize = sizeof(PyAndNode); PyAndNodeType.tp_dealloc = (destructor)PyAndNode_dealloc; PyAndNodeType.tp_methods = ASTNode_methods; PyAndNodeType.tp_init = (initproc)PyAndNode_init; PyAndNodeType.tp_new = PyType_GenericNew;
    PyOrNodeType.tp_name = "cpor_engine.OrNode"; PyOrNodeType.tp_basicsize = sizeof(PyOrNode); PyOrNodeType.tp_dealloc = (destructor)PyOrNode_dealloc; PyOrNodeType.tp_methods = ASTNode_methods; PyOrNodeType.tp_init = (initproc)PyOrNode_init; PyOrNodeType.tp_new = PyType_GenericNew;
    PyNotNodeType.tp_name = "cpor_engine.NotNode"; PyNotNodeType.tp_basicsize = sizeof(PyNotNode); PyNotNodeType.tp_dealloc = (destructor)PyNotNode_dealloc; PyNotNodeType.tp_methods = ASTNode_methods; PyNotNodeType.tp_init = (initproc)PyNotNode_init; PyNotNodeType.tp_new = PyType_GenericNew;

    if (PyType_Ready(&PyPredicateType) < 0 || PyType_Ready(&PyPredicateNodeType) < 0 ||
        PyType_Ready(&PyAndNodeType) < 0 || PyType_Ready(&PyOrNodeType) < 0 || PyType_Ready(&PyNotNodeType) < 0) return NULL;

    PyObject* m = PyModule_Create(&cpor_engine_module);
    if (!m) return NULL;

    Py_INCREF(&PyPredicateType); PyModule_AddObject(m, "Predicate", (PyObject *)&PyPredicateType);
    Py_INCREF(&PyPredicateNodeType); PyModule_AddObject(m, "PredicateNode", (PyObject *)&PyPredicateNodeType);
    Py_INCREF(&PyAndNodeType); PyModule_AddObject(m, "AndNode", (PyObject *)&PyAndNodeType);
    Py_INCREF(&PyOrNodeType); PyModule_AddObject(m, "OrNode", (PyObject *)&PyOrNodeType);
    Py_INCREF(&PyNotNodeType); PyModule_AddObject(m, "NotNode", (PyObject *)&PyNotNodeType);
    return m;
}