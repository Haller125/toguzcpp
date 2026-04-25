#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <sstream>

#include "../src/toguznative.h"

namespace nb = nanobind;

NB_MODULE(toguz, m) {
    nb::class_<ToguzNative>(m, "ToguzNative")
        .def(nb::init<>())
        .def("move", &ToguzNative::move)
        .def("get_legal_moves", &ToguzNative::get_legal_moves)
        .def("is_atsyrau", &ToguzNative::is_atsyrau)
        .def("who_is_winner", &ToguzNative::who_is_winner)
        .def("__str__", [](const ToguzNative& game) {
            std::ostringstream oss; 
            oss << game;
            return oss.str();
        }); 
}