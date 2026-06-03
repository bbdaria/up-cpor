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
#include "PlanningModel/Action.h" 
#include "PlanningModel/BeliefState.h"

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
static PyTypeObject PyActionType = { PyVarObject_HEAD_INIT(NULL, 0) };
static PyTypeObject PyBeliefStateType = { PyVarObject_HEAD_INIT(NULL, 0) };

typedef struct { PyObject_HEAD std::shared_ptr<Predicate>* cpp_obj; } PyPredicate;
typedef struct { PyObject_HEAD std::shared_ptr<PredicateNode>* cpp_obj; } PyPredicateNode;
typedef struct { PyObject_HEAD std::shared_ptr<AndNode>* cpp_obj; } PyAndNode;
typedef struct { PyObject_HEAD std::shared_ptr<OrNode>* cpp_obj; } PyOrNode;
typedef struct { PyObject_HEAD std::shared_ptr<NotNode>* cpp_obj; } PyNotNode;
typedef struct { PyObject_HEAD std::shared_ptr<Action>* cpp_obj; } PyAction;
typedef struct { PyObject_HEAD std::shared_ptr<BeliefState>* cpp_obj; } PyBeliefState;

// 3. Init Functions (Allocation)
static int PyPredicate_init(PyPredicate* self, PyObject* args, PyObject* kwds) { 
    const char* name; 
    if (!PyArg_ParseTuple(args, "s", &name)) return -1; 
    self->cpp_obj = new std::shared_ptr<Predicate>(std::make_shared<Predicate>(std::string(name))); 
    return 0; 
}

static int PyAndNode_init(PyAndNode* self, PyObject* args, PyObject* kwds) { self->cpp_obj = new std::shared_ptr<AndNode>(std::make_shared<AndNode>()); return 0; }
static int PyOrNode_init(PyOrNode* self, PyObject* args, PyObject* kwds) { self->cpp_obj = new std::shared_ptr<OrNode>(std::make_shared<OrNode>()); return 0; }
static int PyNotNode_init(PyNotNode* self, PyObject* args, PyObject* kwds) { self->cpp_obj = new std::shared_ptr<NotNode>(std::make_shared<NotNode>()); return 0; }
static int PyAction_init(PyAction* self, PyObject* args, PyObject* kwds) {
    const char* name;
    if (!PyArg_ParseTuple(args, "s", &name)) return -1;
    self->cpp_obj = new std::shared_ptr<Action>(std::make_shared<Action>(std::string(name)));
    return 0;
}

// 4. Helper: Extract generic formula pointer SAFELY
std::shared_ptr<Formula> extract_formula(PyObject* self) {
    if (!self) return nullptr;
    if (PyObject_TypeCheck(self, &PyPredicateNodeType)) {
        auto obj = ((PyPredicateNode*)self)->cpp_obj;
        return obj ? *obj : nullptr;
    }
    if (PyObject_TypeCheck(self, &PyAndNodeType)) {
        auto obj = ((PyAndNode*)self)->cpp_obj;
        return obj ? *obj : nullptr;
    }
    if (PyObject_TypeCheck(self, &PyOrNodeType)) {
        auto obj = ((PyOrNode*)self)->cpp_obj;
        return obj ? *obj : nullptr;
    }
    if (PyObject_TypeCheck(self, &PyNotNodeType)) {
        auto obj = ((PyNotNode*)self)->cpp_obj;
        return obj ? *obj : nullptr;
    }
    return nullptr;
}

// 5. AST Node Methods
static PyObject* ASTNode_is_true(PyObject* self, PyObject* args) {
    PyObject* py_list;
    if (!PyArg_ParseTuple(args, "O", &py_list)) return NULL;
    
    PredicateSet c_state;
    if (PyList_Check(py_list)) {
        for (Py_ssize_t i = 0; i < PyList_Size(py_list); i++) {
            PyObject* item = PyList_GetItem(py_list, i);
            if (PyObject_TypeCheck(item, &PyPredicateType)) {
                PyPredicate* w = (PyPredicate*)item;
                if (w->cpp_obj && *w->cpp_obj) c_state.insert(*w->cpp_obj);
            }
        }
    }
    
    auto formula = extract_formula(self);
    if (!formula) { PyErr_SetString(PyExc_RuntimeError, "C++ Node uninitialized"); return NULL; }
    return formula->is_true(c_state) ? Py_True : Py_False;
}
static PyObject* Predicate_get_name(PyPredicate* self, PyObject* args) {
    if (!self->cpp_obj || !*self->cpp_obj) { 
        PyErr_SetString(PyExc_RuntimeError, "Predicate uninitialized"); 
        return NULL; 
    }
    std::string name = (*self->cpp_obj)->get_name();
    return PyUnicode_FromString(name.c_str());
}

static PyMethodDef Predicate_methods[] = {
    {"get_name", (PyCFunction)Predicate_get_name, METH_NOARGS, "Get the predicate's name"},
    {NULL}
};

static PyMethodDef ASTNode_methods[] = {{"is_true", (PyCFunction)ASTNode_is_true, METH_VARARGS, NULL}, {NULL}};

// 6. Action Methods (Now protected against Segfaults)
static PyObject* Action_set_precondition(PyAction* self, PyObject* args) {
    PyObject* py_formula;
    if (!PyArg_ParseTuple(args, "O", &py_formula)) return NULL;
    if (!self->cpp_obj || !*self->cpp_obj) { PyErr_SetString(PyExc_RuntimeError, "Action uninitialized"); return NULL; }
    
    auto formula = extract_formula(py_formula);
    (*self->cpp_obj)->precondition = formula;
    Py_RETURN_NONE;
}

static PyObject* Action_add_effect(PyAction* self, PyObject* args) {
    PyObject* py_pred; int is_add;
    if (!PyArg_ParseTuple(args, "Op", &py_pred, &is_add)) return NULL;
    
    if (!self->cpp_obj || !*self->cpp_obj) { PyErr_SetString(PyExc_RuntimeError, "Action uninitialized"); return NULL; }

    if (PyObject_TypeCheck(py_pred, &PyPredicateType)) {
        PyPredicate* p = (PyPredicate*)py_pred;
        if (!p->cpp_obj || !*p->cpp_obj) {
            PyErr_SetString(PyExc_RuntimeError, "Predicate uninitialized"); return NULL;
        }
        auto pred = *(p->cpp_obj);
        if (is_add) (*self->cpp_obj)->add_effects.push_back(pred);
        else (*self->cpp_obj)->del_effects.push_back(pred);
    } else {
        PyErr_SetString(PyExc_TypeError, "Expected a Predicate object");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Action_set_observe(PyAction* self, PyObject* args) {
    PyObject* py_pred;
    if (!PyArg_ParseTuple(args, "O", &py_pred)) return NULL;
    if (!self->cpp_obj || !*self->cpp_obj) { PyErr_SetString(PyExc_RuntimeError, "Action uninitialized"); return NULL; }

    if (PyObject_TypeCheck(py_pred, &PyPredicateType)) {
        PyPredicate* p = (PyPredicate*)py_pred;
        if (p->cpp_obj && *p->cpp_obj) {
            (*self->cpp_obj)->observe = *(p->cpp_obj);
        }
    }
    Py_RETURN_NONE;
}

static PyObject* Action_is_applicable(PyAction* self, PyObject* args) {
    PyObject* py_list;
    if (!PyArg_ParseTuple(args, "O", &py_list)) return NULL;
    if (!self->cpp_obj || !*self->cpp_obj) { PyErr_SetString(PyExc_RuntimeError, "Action uninitialized"); return NULL; }

    PredicateSet c_state;
    if (PyList_Check(py_list)) {
        for (Py_ssize_t i = 0; i < PyList_Size(py_list); i++) {
            PyObject* item = PyList_GetItem(py_list, i);
            if (PyObject_TypeCheck(item, &PyPredicateType)) {
                PyPredicate* p = (PyPredicate*)item;
                if (p->cpp_obj && *p->cpp_obj) c_state.insert(*(p->cpp_obj));
            }
        }
    }
    
    bool allowed = (*self->cpp_obj)->is_applicable(c_state);
    return allowed ? Py_True : Py_False;
}

static PyObject* Action_apply(PyAction* self, PyObject* args) {
    PyObject* py_list;
    if (!PyArg_ParseTuple(args, "O", &py_list)) return NULL;
    if (!self->cpp_obj || !*self->cpp_obj) { PyErr_SetString(PyExc_RuntimeError, "Action uninitialized"); return NULL; }

    PredicateSet c_state;
    if (PyList_Check(py_list)) {
        for (Py_ssize_t i = 0; i < PyList_Size(py_list); i++) {
            PyObject* item = PyList_GetItem(py_list, i);
            if (PyObject_TypeCheck(item, &PyPredicateType)) {
                PyPredicate* p = (PyPredicate*)item;
                if (p->cpp_obj && *p->cpp_obj) c_state.insert(*(p->cpp_obj));
            }
        }
    }
    
    (*self->cpp_obj)->apply(c_state);
    
    PyObject* new_py_list = PyList_New(0);
    for (const auto& pred : c_state) {
        PyObject* py_str = PyUnicode_FromString(pred->get_name().c_str());
        PyObject* init_args = PyTuple_Pack(1, py_str);
        PyObject* py_pred = PyObject_CallObject((PyObject*)&PyPredicateType, init_args);
        Py_DECREF(init_args);
        Py_DECREF(py_str);

        if (py_pred) {
            PyList_Append(new_py_list, py_pred);
            Py_DECREF(py_pred);
        }
    }
    
    return new_py_list;
}

static int PyBeliefState_init(PyBeliefState* self, PyObject* args, PyObject* kwds) {
    self->cpp_obj = new std::shared_ptr<BeliefState>(std::make_shared<BeliefState>());
    return 0;
}

static void PyBeliefState_dealloc(PyBeliefState* self) {
    if (self->cpp_obj) { delete self->cpp_obj; self->cpp_obj = nullptr; }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* BeliefState_add_observed(PyBeliefState* self, PyObject* args) {
    PyObject* py_pred;
    if (!PyArg_ParseTuple(args, "O", &py_pred)) return NULL;
    if (!self->cpp_obj || !*self->cpp_obj) { PyErr_SetString(PyExc_RuntimeError, "BeliefState uninitialized"); return NULL; }
    
    if (PyObject_TypeCheck(py_pred, &PyPredicateType)) {
        PyPredicate* p = (PyPredicate*)py_pred;
        bool added = (*self->cpp_obj)->add_observed(*(p->cpp_obj));
        return added ? Py_True : Py_False;
    }
    PyErr_SetString(PyExc_TypeError, "Expected a Predicate object");
    return NULL;
}

static PyObject* BeliefState_get_observed(PyBeliefState* self, PyObject* args) {
    if (!self->cpp_obj || !*self->cpp_obj) { PyErr_SetString(PyExc_RuntimeError, "BeliefState uninitialized"); return NULL; }
    
    const auto& obs = (*self->cpp_obj)->get_observed();
    PyObject* new_py_list = PyList_New(0);
    
    for (const auto& pred : obs) {
        PyObject* py_str = PyUnicode_FromString(pred->get_name().c_str());
        PyObject* init_args = PyTuple_Pack(1, py_str);
        PyObject* py_pred = PyObject_CallObject((PyObject*)&PyPredicateType, init_args);
        Py_DECREF(init_args);
        Py_DECREF(py_str);

        if (py_pred) {
            PyList_Append(new_py_list, py_pred);
            Py_DECREF(py_pred);
        }
    }
    return new_py_list;
}

static PyMethodDef BeliefState_methods[] = {
    {"add_observed", (PyCFunction)BeliefState_add_observed, METH_VARARGS, "Add an observed predicate"},
    {"get_observed", (PyCFunction)BeliefState_get_observed, METH_NOARGS, "Get list of observed predicates"},
    {NULL}
};

static PyMethodDef Action_methods[] = {
    {"set_precondition", (PyCFunction)Action_set_precondition, METH_VARARGS, "Set AST precondition"},
    {"add_effect", (PyCFunction)Action_add_effect, METH_VARARGS, "Add effect (predicate, is_add)"},
    {"set_observe", (PyCFunction)Action_set_observe, METH_VARARGS, "Set predicate for sensing"},
    {"is_applicable", (PyCFunction)Action_is_applicable, METH_VARARGS, "Check if allowed in state"},
    {"apply", (PyCFunction)Action_apply, METH_VARARGS, "Apply effects and return new state"},
    {NULL}
};

// 7. Deallocators (Safe deletion)
static void PyPredicate_dealloc(PyPredicate* self) { if (self->cpp_obj) { delete self->cpp_obj; self->cpp_obj = nullptr; } Py_TYPE(self)->tp_free((PyObject*)self); }
static void PyPredicateNode_dealloc(PyPredicateNode* self) { if (self->cpp_obj) { delete self->cpp_obj; self->cpp_obj = nullptr; } Py_TYPE(self)->tp_free((PyObject*)self); }
static void PyAndNode_dealloc(PyAndNode* self) { if (self->cpp_obj) { delete self->cpp_obj; self->cpp_obj = nullptr; } Py_TYPE(self)->tp_free((PyObject*)self); }
static void PyOrNode_dealloc(PyOrNode* self) { if (self->cpp_obj) { delete self->cpp_obj; self->cpp_obj = nullptr; } Py_TYPE(self)->tp_free((PyObject*)self); }
static void PyNotNode_dealloc(PyNotNode* self) { if (self->cpp_obj) { delete self->cpp_obj; self->cpp_obj = nullptr; } Py_TYPE(self)->tp_free((PyObject*)self); }
static void PyAction_dealloc(PyAction* self) { if (self->cpp_obj) { delete self->cpp_obj; self->cpp_obj = nullptr; } Py_TYPE(self)->tp_free((PyObject*)self); }

// 8. Initialization
PyMODINIT_FUNC PyInit_cpor_engine(void) {
    // BUG FIX: Added Py_TPFLAGS_DEFAULT to everything to fix Python 3 init bug
    PyPredicateType.tp_name = "cpor_engine.Predicate"; PyPredicateType.tp_basicsize = sizeof(PyPredicate); PyPredicateType.tp_dealloc = (destructor)PyPredicate_dealloc; PyPredicateType.tp_init = (initproc)PyPredicate_init; PyPredicateType.tp_new = PyType_GenericNew; PyPredicateType.tp_flags = Py_TPFLAGS_DEFAULT; PyPredicateType.tp_methods = Predicate_methods;
    PyPredicateNodeType.tp_name = "cpor_engine.PredicateNode"; PyPredicateNodeType.tp_basicsize = sizeof(PyPredicateNode); PyPredicateNodeType.tp_dealloc = (destructor)PyPredicateNode_dealloc; PyPredicateNodeType.tp_methods = ASTNode_methods; PyPredicateNodeType.tp_new = PyType_GenericNew; PyPredicateNodeType.tp_flags = Py_TPFLAGS_DEFAULT;
    PyAndNodeType.tp_name = "cpor_engine.AndNode"; PyAndNodeType.tp_basicsize = sizeof(PyAndNode); PyAndNodeType.tp_dealloc = (destructor)PyAndNode_dealloc; PyAndNodeType.tp_methods = ASTNode_methods; PyAndNodeType.tp_init = (initproc)PyAndNode_init; PyAndNodeType.tp_new = PyType_GenericNew; PyAndNodeType.tp_flags = Py_TPFLAGS_DEFAULT;
    PyOrNodeType.tp_name = "cpor_engine.OrNode"; PyOrNodeType.tp_basicsize = sizeof(PyOrNode); PyOrNodeType.tp_dealloc = (destructor)PyOrNode_dealloc; PyOrNodeType.tp_methods = ASTNode_methods; PyOrNodeType.tp_init = (initproc)PyOrNode_init; PyOrNodeType.tp_new = PyType_GenericNew; PyOrNodeType.tp_flags = Py_TPFLAGS_DEFAULT;
    PyNotNodeType.tp_name = "cpor_engine.NotNode"; PyNotNodeType.tp_basicsize = sizeof(PyNotNode); PyNotNodeType.tp_dealloc = (destructor)PyNotNode_dealloc; PyNotNodeType.tp_methods = ASTNode_methods; PyNotNodeType.tp_init = (initproc)PyNotNode_init; PyNotNodeType.tp_new = PyType_GenericNew; PyNotNodeType.tp_flags = Py_TPFLAGS_DEFAULT;
    PyActionType.tp_name = "cpor_engine.Action"; PyActionType.tp_basicsize = sizeof(PyAction); PyActionType.tp_dealloc = (destructor)PyAction_dealloc; PyActionType.tp_methods = Action_methods; PyActionType.tp_init = (initproc)PyAction_init; PyActionType.tp_new = PyType_GenericNew; PyActionType.tp_flags = Py_TPFLAGS_DEFAULT;
    PyBeliefStateType.tp_name = "cpor_engine.BeliefState"; PyBeliefStateType.tp_basicsize = sizeof(PyBeliefState); PyBeliefStateType.tp_dealloc = (destructor)PyBeliefState_dealloc; PyBeliefStateType.tp_methods = BeliefState_methods; PyBeliefStateType.tp_init = (initproc)PyBeliefState_init; PyBeliefStateType.tp_new = PyType_GenericNew; PyBeliefStateType.tp_flags = Py_TPFLAGS_DEFAULT;
    if (PyType_Ready(&PyPredicateType) < 0 || PyType_Ready(&PyPredicateNodeType) < 0 ||
        PyType_Ready(&PyAndNodeType) < 0 || PyType_Ready(&PyOrNodeType) < 0 || 
        PyType_Ready(&PyNotNodeType) < 0 || PyType_Ready(&PyActionType) < 0 || PyType_Ready(&PyBeliefStateType) < 0) return NULL;

    PyObject* m = PyModule_Create(&cpor_engine_module);
    if (!m) return NULL;

    Py_INCREF(&PyPredicateType); PyModule_AddObject(m, "Predicate", (PyObject *)&PyPredicateType);
    Py_INCREF(&PyPredicateNodeType); PyModule_AddObject(m, "PredicateNode", (PyObject *)&PyPredicateNodeType);
    Py_INCREF(&PyAndNodeType); PyModule_AddObject(m, "AndNode", (PyObject *)&PyAndNodeType);
    Py_INCREF(&PyOrNodeType); PyModule_AddObject(m, "OrNode", (PyObject *)&PyOrNodeType);
    Py_INCREF(&PyNotNodeType); PyModule_AddObject(m, "NotNode", (PyObject *)&PyNotNodeType);
    Py_INCREF(&PyActionType); PyModule_AddObject(m, "Action", (PyObject *)&PyActionType);
    Py_INCREF(&PyBeliefStateType); PyModule_AddObject(m, "BeliefState", (PyObject *)&PyBeliefStateType);

    return m;
}