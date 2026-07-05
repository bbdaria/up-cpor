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
#include "PlanningModel/BeliefSolver.h"
#include "PlanningModel/KTTranslator.h"
#include "FF-v2.3/ff_wrapper.h"

static bool py_list_to_strings(PyObject* py_list, std::vector<std::string>& out);  // fwd decl

// 0. Module-level functions
// ff_solve(domain_path, problem_path) -> list[str]
// Runs the in-process Metric-FF classical planner and returns the plan as a list
// of operator strings ("OP arg arg", FF's upper-case names).
static PyObject* py_ff_solve(PyObject* self, PyObject* args) {
    const char* domain_path;
    const char* problem_path;
    if (!PyArg_ParseTuple(args, "ss", &domain_path, &problem_path)) return NULL;

    std::vector<std::string> plan;
    Py_BEGIN_ALLOW_THREADS
    plan = ff_solve(std::string(domain_path), std::string(problem_path));
    Py_END_ALLOW_THREADS

    PyObject* py_list = PyList_New(0);
    if (!py_list) return NULL;
    for (const auto& step : plan) {
        PyObject* s = PyUnicode_FromString(step.c_str());
        if (!s) { Py_DECREF(py_list); return NULL; }
        PyList_Append(py_list, s);
        Py_DECREF(s);
    }
    return py_list;
}

// ff_solve_strings(domain_str, problem_str) -> list[str]
// Same as ff_solve but parses the PDDL from in-memory strings (no temp files).
static PyObject* py_ff_solve_strings(PyObject* self, PyObject* args) {
    const char* domain_str;
    const char* problem_str;
    Py_ssize_t domain_len, problem_len;
    if (!PyArg_ParseTuple(args, "s#s#", &domain_str, &domain_len,
                          &problem_str, &problem_len)) return NULL;

    std::vector<std::string> plan;
    std::string dom(domain_str, domain_len);
    std::string prob(problem_str, problem_len);
    Py_BEGIN_ALLOW_THREADS
    plan = ff_solve_strings(dom, prob);
    Py_END_ALLOW_THREADS

    PyObject* py_list = PyList_New(0);
    if (!py_list) return NULL;
    for (const auto& step : plan) {
        PyObject* s = PyUnicode_FromString(step.c_str());
        if (!s) { Py_DECREF(py_list); return NULL; }
        PyList_Append(py_list, s);
        Py_DECREF(s);
    }
    return py_list;
}

// Parse a Python list of (str, bool) tuples.
static bool parse_literal_list(PyObject* py_list, std::vector<std::pair<std::string, bool>>& out) {
    if (!PyList_Check(py_list)) { PyErr_SetString(PyExc_TypeError, "expected a list"); return false; }
    for (Py_ssize_t i = 0; i < PyList_Size(py_list); i++) {
        PyObject* item = PyList_GetItem(py_list, i);
        const char* name; int pol;
        if (!PyArg_ParseTuple(item, "sp", &name, &pol)) return false;
        out.emplace_back(std::string(name), pol != 0);
    }
    return true;
}

// kt_translate(actions, uncertain, tags, known_true, goal) -> (domain_str, problem_str)
//   actions: list of (name, is_sensing, observe_or_None, pre[(f,pol)], add[str], del[str])
//   uncertain: list[str]; tags: list[list[str]]; known_true: list[str]; goal: list[(f,pol)]
static PyObject* py_kt_translate(PyObject* self, PyObject* args) {
    PyObject *py_actions, *py_uncertain, *py_tags, *py_known, *py_goal;
    if (!PyArg_ParseTuple(args, "OOOOO", &py_actions, &py_uncertain, &py_tags, &py_known, &py_goal))
        return NULL;

    std::vector<KTAction> actions;
    if (!PyList_Check(py_actions)) { PyErr_SetString(PyExc_TypeError, "actions must be a list"); return NULL; }
    for (Py_ssize_t i = 0; i < PyList_Size(py_actions); i++) {
        PyObject *name, *is_sensing, *observe, *pre, *add, *del, *cond = NULL;
        if (!PyArg_ParseTuple(PyList_GetItem(py_actions, i), "OOOOOO|O",
                              &name, &is_sensing, &observe, &pre, &add, &del, &cond))
            return NULL;
        KTAction a;
        a.name = PyUnicode_AsUTF8(name);
        a.is_sensing = PyObject_IsTrue(is_sensing) == 1;
        a.observe = (observe == Py_None) ? "" : PyUnicode_AsUTF8(observe);
        if (!parse_literal_list(pre, a.pre)) return NULL;
        if (!py_list_to_strings(add, a.add)) return NULL;
        if (!py_list_to_strings(del, a.del)) return NULL;
        if (cond != NULL) {
            if (!PyList_Check(cond)) { PyErr_SetString(PyExc_TypeError, "cond must be a list"); return NULL; }
            for (Py_ssize_t j = 0; j < PyList_Size(cond); j++) {
                PyObject *cond_lits_py, *fluent_py, *is_add_py;
                if (!PyArg_ParseTuple(PyList_GetItem(cond, j), "OOO",
                                      &cond_lits_py, &fluent_py, &is_add_py))
                    return NULL;
                std::vector<std::pair<std::string, bool>> cond_lits;
                if (!parse_literal_list(cond_lits_py, cond_lits)) return NULL;
                std::string fluent = PyUnicode_AsUTF8(fluent_py);
                bool is_add = PyObject_IsTrue(is_add_py) == 1;
                a.cond.emplace_back(std::move(cond_lits), std::move(fluent), is_add);
            }
        }
        actions.push_back(std::move(a));
    }

    std::vector<std::string> uncertain, known_true;
    if (!py_list_to_strings(py_uncertain, uncertain)) return NULL;
    if (!py_list_to_strings(py_known, known_true)) return NULL;

    std::vector<std::vector<std::string>> tags;
    if (!PyList_Check(py_tags)) { PyErr_SetString(PyExc_TypeError, "tags must be a list"); return NULL; }
    for (Py_ssize_t i = 0; i < PyList_Size(py_tags); i++) {
        std::vector<std::string> t;
        if (!py_list_to_strings(PyList_GetItem(py_tags, i), t)) return NULL;
        tags.push_back(std::move(t));
    }

    std::vector<std::pair<std::string, bool>> goal;
    if (!parse_literal_list(py_goal, goal)) return NULL;

    auto result = kt_translate(actions, uncertain, tags, known_true, goal);
    return Py_BuildValue("ss", result.first.c_str(), result.second.c_str());
}

// static PyObject* py_kt_translate(PyObject* self, PyObject* args) {
//     // PyObject *py_actions, *py_uncertain, *py_tags, *py_known, *py_goal;
//     // if (!PyArg_ParseTuple(args, "OOOOO", &py_actions, &py_uncertain, &py_tags, &py_known, &py_goal))
//     //     return NULL;
//     PyObject *name, *is_sensing, *observe, *pre, *add, *del, *cond = NULL;
//     if (!PyArg_ParseTuple(PyList_GetItem(py_actions, i), "OOOOOO|O",
//                         &name, &is_sensing, &observe, &pre, &add, &del, &cond))
//         return NULL;

//     if (cond != NULL) {
//         for (Py_ssize_t j = 0; j < PyList_Size(cond); j++) {
//             PyObject *cond_lits_py, *fluent_py, *is_add_py;
//             PyArg_ParseTuple(PyList_GetItem(cond, j), "OOO", &cond_lits_py, &fluent_py, &is_add_py);
//             std::vector<std::pair<std::string, bool>> cond_lits;
//             parse_literal_list(cond_lits_py, cond_lits);
//             a.cond.emplace_back(std::move(cond_lits), PyUnicode_AsUTF8(fluent_py), PyObject_IsTrue(is_add_py) == 1);
//         }
//     }

//     std::vector<KTAction> actions;
//     if (!PyList_Check(py_actions)) { PyErr_SetString(PyExc_TypeError, "actions must be a list"); return NULL; }
//     for (Py_ssize_t i = 0; i < PyList_Size(py_actions); i++) {
//         PyObject *name, *is_sensing, *observe, *pre, *add, *del;
//         if (!PyArg_ParseTuple(PyList_GetItem(py_actions, i), "OOOOOO",
//                               &name, &is_sensing, &observe, &pre, &add, &del))
//             return NULL;
//         KTAction a;
//         a.name = PyUnicode_AsUTF8(name);
//         a.is_sensing = PyObject_IsTrue(is_sensing) == 1;
//         a.observe = (observe == Py_None) ? "" : PyUnicode_AsUTF8(observe);
//         if (!parse_literal_list(pre, a.pre)) return NULL;
//         if (!py_list_to_strings(add, a.add)) return NULL;
//         if (!py_list_to_strings(del, a.del)) return NULL;
//         actions.push_back(std::move(a));
//     }

//     std::vector<std::string> uncertain, known_true;
//     if (!py_list_to_strings(py_uncertain, uncertain)) return NULL;
//     if (!py_list_to_strings(py_known, known_true)) return NULL;

//     std::vector<std::vector<std::string>> tags;
//     if (!PyList_Check(py_tags)) { PyErr_SetString(PyExc_TypeError, "tags must be a list"); return NULL; }
//     for (Py_ssize_t i = 0; i < PyList_Size(py_tags); i++) {
//         std::vector<std::string> t;
//         if (!py_list_to_strings(PyList_GetItem(py_tags, i), t)) return NULL;
//         tags.push_back(std::move(t));
//     }

//     std::vector<std::pair<std::string, bool>> goal;
//     if (!parse_literal_list(py_goal, goal)) return NULL;

//     auto result = kt_translate(actions, uncertain, tags, known_true, goal);
//     return Py_BuildValue("ss", result.first.c_str(), result.second.c_str());
// }

static PyMethodDef cpor_engine_functions[] = {
    {"ff_solve", (PyCFunction)py_ff_solve, METH_VARARGS,
     "ff_solve(domain_path, problem_path) -> list[str]: run Metric-FF, return the plan."},
    {"ff_solve_strings", (PyCFunction)py_ff_solve_strings, METH_VARARGS,
     "ff_solve_strings(domain_str, problem_str) -> list[str]: run Metric-FF on in-memory PDDL."},
    {"kt_translate", (PyCFunction)py_kt_translate, METH_VARARGS,
     "kt_translate(actions, uncertain, tags, known_true, goal) -> (domain, problem): KT translation."},
    {NULL, NULL, 0, NULL}
};

// 1. Module Definition
static struct PyModuleDef cpor_engine_module = {
    PyModuleDef_HEAD_INIT, "cpor_engine", "CPOR C++ Logical Extension Module", -1,
    cpor_engine_functions
};

// 2. Global Type Definitions
static PyTypeObject PyPredicateType = { PyVarObject_HEAD_INIT(NULL, 0) };
static PyTypeObject PyPredicateNodeType = { PyVarObject_HEAD_INIT(NULL, 0) };
static PyTypeObject PyAndNodeType = { PyVarObject_HEAD_INIT(NULL, 0) };
static PyTypeObject PyOrNodeType = { PyVarObject_HEAD_INIT(NULL, 0) };
static PyTypeObject PyNotNodeType = { PyVarObject_HEAD_INIT(NULL, 0) };
static PyTypeObject PyBeliefSolverType = { PyVarObject_HEAD_INIT(NULL, 0) };

typedef struct { PyObject_HEAD std::shared_ptr<Predicate>* cpp_obj; } PyPredicate;
typedef struct { PyObject_HEAD std::shared_ptr<PredicateNode>* cpp_obj; } PyPredicateNode;
typedef struct { PyObject_HEAD std::shared_ptr<AndNode>* cpp_obj; } PyAndNode;
typedef struct { PyObject_HEAD std::shared_ptr<OrNode>* cpp_obj; } PyOrNode;
typedef struct { PyObject_HEAD std::shared_ptr<NotNode>* cpp_obj; } PyNotNode;
typedef struct { PyObject_HEAD std::shared_ptr<BeliefSolver>* cpp_obj; } PyBeliefSolver;

// 3. Init Functions (Allocation)
static int PyPredicate_init(PyPredicate* self, PyObject* args, PyObject* kwds) {
    const char* name;
    if (!PyArg_ParseTuple(args, "s", &name)) return -1;
    self->cpp_obj = new std::shared_ptr<Predicate>(std::make_shared<Predicate>(std::string(name)));
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
    if (formula->is_true(c_state)) Py_RETURN_TRUE;
    else Py_RETURN_FALSE;
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

static int PyPredicateNode_init(PyPredicateNode* self, PyObject* args, PyObject* kwds) {
    PyObject* py_pred;
    if (!PyArg_ParseTuple(args, "O", &py_pred)) return -1;
    if (!PyObject_TypeCheck(py_pred, &PyPredicateType)) {
        PyErr_SetString(PyExc_TypeError, "PredicateNode requires a Predicate");
        return -1;
    }
    PyPredicate* p = (PyPredicate*)py_pred;
    if (!p->cpp_obj || !*p->cpp_obj) {
        PyErr_SetString(PyExc_RuntimeError, "Predicate uninitialized");
        return -1;
    }
    self->cpp_obj = new std::shared_ptr<PredicateNode>(
        std::make_shared<PredicateNode>(*(p->cpp_obj)));
    return 0;
}
static int PyOrNode_init(PyOrNode* self, PyObject* args, PyObject* kwds) {
    PyObject* py_list = nullptr;
    if (!PyArg_ParseTuple(args, "|O", &py_list)) return -1;  // optional arg

    FormulaList children;
    if (py_list && PyList_Check(py_list)) {
        for (Py_ssize_t i = 0; i < PyList_Size(py_list); i++) {
            PyObject* item = PyList_GetItem(py_list, i);
            auto f = extract_formula(item);
            if (f) children.push_back(f);
        }
    }
    self->cpp_obj = new std::shared_ptr<OrNode>(std::make_shared<OrNode>(children));
    return 0;
}

static int PyNotNode_init(PyNotNode* self, PyObject* args, PyObject* kwds) {
    PyObject* py_child = nullptr;
    if (!PyArg_ParseTuple(args, "|O", &py_child)) return -1;
    std::shared_ptr<Formula> child = py_child ? extract_formula(py_child) : nullptr;
    self->cpp_obj = new std::shared_ptr<NotNode>(std::make_shared<NotNode>(child));
    return 0;
}

static int PyAndNode_init(PyAndNode* self, PyObject* args, PyObject* kwds) {
    PyObject* py_list = nullptr;
    if (!PyArg_ParseTuple(args, "|O", &py_list)) return -1;  // optional arg

    FormulaList children;
    if (py_list && PyList_Check(py_list)) {
        for (Py_ssize_t i = 0; i < PyList_Size(py_list); i++) {
            PyObject* item = PyList_GetItem(py_list, i);
            auto f = extract_formula(item);
            if (f) children.push_back(f);
        }
    }
    self->cpp_obj = new std::shared_ptr<AndNode>(std::make_shared<AndNode>(children));
    return 0;
}

// 6b. BeliefSolver (Z3-backed reasoning over initial-state constraints)
static bool py_list_to_strings(PyObject* py_list, std::vector<std::string>& out) {
    if (!PyList_Check(py_list)) {
        PyErr_SetString(PyExc_TypeError, "Expected a list of strings");
        return false;
    }
    Py_ssize_t n = PyList_Size(py_list);
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject* item = PyList_GetItem(py_list, i);
        if (!PyUnicode_Check(item)) {
            PyErr_SetString(PyExc_TypeError, "Expected a list of strings");
            return false;
        }
        out.emplace_back(PyUnicode_AsUTF8(item));
    }
    return true;
}

static int PyBeliefSolver_init(PyBeliefSolver* self, PyObject* args, PyObject* kwds) {
    self->cpp_obj = new std::shared_ptr<BeliefSolver>(std::make_shared<BeliefSolver>());
    return 0;
}

static void PyBeliefSolver_dealloc(PyBeliefSolver* self) {
    if (self->cpp_obj) { delete self->cpp_obj; self->cpp_obj = nullptr; }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* BeliefSolver_add_oneof(PyBeliefSolver* self, PyObject* args) {
    PyObject* py_list;
    if (!PyArg_ParseTuple(args, "O", &py_list)) return NULL;
    std::vector<std::string> lits;
    if (!py_list_to_strings(py_list, lits)) return NULL;
    (*self->cpp_obj)->add_oneof(lits);
    Py_RETURN_NONE;
}

static PyObject* BeliefSolver_add_clause(PyBeliefSolver* self, PyObject* args) {
    PyObject* py_list;
    if (!PyArg_ParseTuple(args, "O", &py_list)) return NULL;
    std::vector<std::string> lits;
    if (!py_list_to_strings(py_list, lits)) return NULL;
    (*self->cpp_obj)->add_clause(lits);
    Py_RETURN_NONE;
}

static PyObject* BeliefSolver_set_unknown(PyBeliefSolver* self, PyObject* args) {
    PyObject* py_list;
    if (!PyArg_ParseTuple(args, "O", &py_list)) return NULL;
    std::vector<std::string> names;
    if (!py_list_to_strings(py_list, names)) return NULL;
    (*self->cpp_obj)->set_unknown(names);
    Py_RETURN_NONE;
}

static PyObject* BeliefSolver_propagate(PyBeliefSolver* self, PyObject* args) {
    PyObject* py_list;
    if (!PyArg_ParseTuple(args, "O", &py_list)) return NULL;
    std::vector<std::string> facts;
    if (!py_list_to_strings(py_list, facts)) return NULL;

    auto result = (*self->cpp_obj)->propagate(facts);
    if (!result.has_value()) Py_RETURN_NONE;  // contradiction

    PyObject* out = PyList_New(0);
    if (!out) return NULL;
    for (const auto& f : *result) {
        PyObject* s = PyUnicode_FromString(f.c_str());
        if (!s) { Py_DECREF(out); return NULL; }
        PyList_Append(out, s);
        Py_DECREF(s);
    }
    return out;
}

static PyObject* BeliefSolver_is_consistent(PyBeliefSolver* self, PyObject* args) {
    PyObject* py_list;
    if (!PyArg_ParseTuple(args, "O", &py_list)) return NULL;
    std::vector<std::string> facts;
    if (!py_list_to_strings(py_list, facts)) return NULL;
    if ((*self->cpp_obj)->is_consistent(facts)) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static PyObject* BeliefSolver_complete(PyBeliefSolver* self, PyObject* args) {
    PyObject* py_list;
    if (!PyArg_ParseTuple(args, "O", &py_list)) return NULL;
    std::vector<std::string> facts;
    if (!py_list_to_strings(py_list, facts)) return NULL;

    auto result = (*self->cpp_obj)->complete(facts);
    if (!result.has_value()) Py_RETURN_NONE;  // unsatisfiable

    PyObject* out = PyList_New(0);
    if (!out) return NULL;
    for (const auto& f : *result) {
        PyObject* s = PyUnicode_FromString(f.c_str());
        if (!s) { Py_DECREF(out); return NULL; }
        PyList_Append(out, s);
        Py_DECREF(s);
    }
    return out;
}

static PyMethodDef BeliefSolver_methods[] = {
    {"add_oneof", (PyCFunction)BeliefSolver_add_oneof, METH_VARARGS, "Add an exactly-one-of constraint (list of literals)"},
    {"add_clause", (PyCFunction)BeliefSolver_add_clause, METH_VARARGS, "Add a disjunctive (or) clause (list of literals)"},
    {"set_unknown", (PyCFunction)BeliefSolver_set_unknown, METH_VARARGS, "Register the initially-unknown fact names"},
    {"propagate", (PyCFunction)BeliefSolver_propagate, METH_VARARGS, "Closure of given literals, or None if inconsistent"},
    {"is_consistent", (PyCFunction)BeliefSolver_is_consistent, METH_VARARGS, "Whether the given literals are satisfiable"},
    {"complete", (PyCFunction)BeliefSolver_complete, METH_VARARGS, "One satisfying model (true vars) consistent with the given literals, or None"},
    {NULL}
};

// 7. Deallocators (Safe deletion)
static void PyPredicate_dealloc(PyPredicate* self) { if (self->cpp_obj) { delete self->cpp_obj; self->cpp_obj = nullptr; } Py_TYPE(self)->tp_free((PyObject*)self); }
static void PyPredicateNode_dealloc(PyPredicateNode* self) { if (self->cpp_obj) { delete self->cpp_obj; self->cpp_obj = nullptr; } Py_TYPE(self)->tp_free((PyObject*)self); }
static void PyAndNode_dealloc(PyAndNode* self) { if (self->cpp_obj) { delete self->cpp_obj; self->cpp_obj = nullptr; } Py_TYPE(self)->tp_free((PyObject*)self); }
static void PyOrNode_dealloc(PyOrNode* self) { if (self->cpp_obj) { delete self->cpp_obj; self->cpp_obj = nullptr; } Py_TYPE(self)->tp_free((PyObject*)self); }
static void PyNotNode_dealloc(PyNotNode* self) { if (self->cpp_obj) { delete self->cpp_obj; self->cpp_obj = nullptr; } Py_TYPE(self)->tp_free((PyObject*)self); }

// 8. Initialization
PyMODINIT_FUNC PyInit_cpor_engine(void) {
    // BUG FIX: Added Py_TPFLAGS_DEFAULT to everything to fix Python 3 init bug
    PyPredicateType.tp_name = "cpor_engine.Predicate"; PyPredicateType.tp_basicsize = sizeof(PyPredicate); PyPredicateType.tp_dealloc = (destructor)PyPredicate_dealloc; PyPredicateType.tp_init = (initproc)PyPredicate_init; PyPredicateType.tp_new = PyType_GenericNew; PyPredicateType.tp_flags = Py_TPFLAGS_DEFAULT; PyPredicateType.tp_methods = Predicate_methods;
    PyPredicateNodeType.tp_name = "cpor_engine.PredicateNode"; PyPredicateNodeType.tp_basicsize = sizeof(PyPredicateNode); PyPredicateNodeType.tp_dealloc = (destructor)PyPredicateNode_dealloc; PyPredicateNodeType.tp_methods = ASTNode_methods; PyPredicateNodeType.tp_new = PyType_GenericNew; PyPredicateNodeType.tp_flags = Py_TPFLAGS_DEFAULT; PyPredicateNodeType.tp_init = (initproc)PyPredicateNode_init;
    PyAndNodeType.tp_name = "cpor_engine.AndNode"; PyAndNodeType.tp_basicsize = sizeof(PyAndNode); PyAndNodeType.tp_dealloc = (destructor)PyAndNode_dealloc; PyAndNodeType.tp_methods = ASTNode_methods; PyAndNodeType.tp_init = (initproc)PyAndNode_init; PyAndNodeType.tp_new = PyType_GenericNew; PyAndNodeType.tp_flags = Py_TPFLAGS_DEFAULT;
    PyOrNodeType.tp_name = "cpor_engine.OrNode"; PyOrNodeType.tp_basicsize = sizeof(PyOrNode); PyOrNodeType.tp_dealloc = (destructor)PyOrNode_dealloc; PyOrNodeType.tp_methods = ASTNode_methods; PyOrNodeType.tp_init = (initproc)PyOrNode_init; PyOrNodeType.tp_new = PyType_GenericNew; PyOrNodeType.tp_flags = Py_TPFLAGS_DEFAULT;
    PyNotNodeType.tp_name = "cpor_engine.NotNode"; PyNotNodeType.tp_basicsize = sizeof(PyNotNode); PyNotNodeType.tp_dealloc = (destructor)PyNotNode_dealloc; PyNotNodeType.tp_methods = ASTNode_methods; PyNotNodeType.tp_init = (initproc)PyNotNode_init; PyNotNodeType.tp_new = PyType_GenericNew; PyNotNodeType.tp_flags = Py_TPFLAGS_DEFAULT;
    PyBeliefSolverType.tp_name = "cpor_engine.BeliefSolver"; PyBeliefSolverType.tp_basicsize = sizeof(PyBeliefSolver); PyBeliefSolverType.tp_dealloc = (destructor)PyBeliefSolver_dealloc; PyBeliefSolverType.tp_methods = BeliefSolver_methods; PyBeliefSolverType.tp_init = (initproc)PyBeliefSolver_init; PyBeliefSolverType.tp_new = PyType_GenericNew; PyBeliefSolverType.tp_flags = Py_TPFLAGS_DEFAULT;
    if (PyType_Ready(&PyPredicateType) < 0 || PyType_Ready(&PyPredicateNodeType) < 0 ||
        PyType_Ready(&PyAndNodeType) < 0 || PyType_Ready(&PyOrNodeType) < 0 ||
        PyType_Ready(&PyNotNodeType) < 0 || PyType_Ready(&PyBeliefSolverType) < 0) return NULL;

    PyObject* m = PyModule_Create(&cpor_engine_module);
    if (!m) return NULL;

    Py_INCREF(&PyPredicateType); PyModule_AddObject(m, "Predicate", (PyObject *)&PyPredicateType);
    Py_INCREF(&PyPredicateNodeType); PyModule_AddObject(m, "PredicateNode", (PyObject *)&PyPredicateNodeType);
    Py_INCREF(&PyAndNodeType); PyModule_AddObject(m, "AndNode", (PyObject *)&PyAndNodeType);
    Py_INCREF(&PyOrNodeType); PyModule_AddObject(m, "OrNode", (PyObject *)&PyOrNodeType);
    Py_INCREF(&PyNotNodeType); PyModule_AddObject(m, "NotNode", (PyObject *)&PyNotNodeType);
    Py_INCREF(&PyBeliefSolverType); PyModule_AddObject(m, "BeliefSolver", (PyObject *)&PyBeliefSolverType);

    return m;
}