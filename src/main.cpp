#include <pybind11/pybind11.h>
#include <pybind11/stl.h> 
#include "PlanningModel/Domain.h"

namespace py = pybind11;

PYBIND11_MODULE(cpor_engine, m) {
    m.doc() = "C++ Backend for CPOR Planner";

    py::class_<PlanningAction>(m, "PlanningAction")
        .def(py::init<>()) 
        .def_readwrite("name", &PlanningAction::name)
        .def_readwrite("is_sensing", &PlanningAction::is_sensing)
        .def_readwrite("preconditions", &PlanningAction::preconditions)
        .def_readwrite("effects", &PlanningAction::effects)
        .def_readwrite("observed_fluents", &PlanningAction::observed_fluents)
        .def("debug_print", &PlanningAction::debug_print);

    py::class_<ProblemContext>(m, "ProblemContext")
        .def(py::init<>())
        .def_readwrite("problem_name", &ProblemContext::problem_name)
        .def("add_action", &ProblemContext::add_action)
        .def("debug_print", &ProblemContext::debug_print);
}