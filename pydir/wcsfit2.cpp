#include "pybind11/pybind11.h"
#include "Instrument.h"
#include "FitSubroutines.h"

namespace py = pybind11;
using namespace pybind11::literals;

using InstrumentVector = std::vector<std::unique_ptr<Instrument>>;

PYBIND11_MAKE_OPAQUE(InstrumentVector);


template <typename T>
void declareBounds(py::module & mod, std::string suffix) {
    py::class_<Bounds<T>> cls(mod, ("Bounds" + suffix).c_str());
    cls.def(py::init<T, T, T, T>(), "x1"_a, "x2"_a, "y1"_a, "y2"_a);
}

void declareSphericalCoords(py::module & mod) {
    py::class_<astrometry::SphericalCoords> cls(mod, "SphericalCoords");
    cls.def(py::init<double, double>(), "lon"_a, "lat"_a);
}

void declareFields(py::module & mod) {
    py::class_<Fields> cls(mod, "Fields");
    cls.def(
        py::init<std::vector<std::string>, std::vector<double>, std::vector<double>, std::vector<double>>(),
        "names"_a,
        "ra"_a,
        "dec"_a,
        "epochs"_a
    );
}

// We declare a vector of unique_ptr to Instrument to Python rather than
// Instrument itself because Instrument isn't copyable and unique_ptr isn't
// either; this makes communicating with Python difficult because Python can't
// ever guarantee we'll be given the only reference to an object.  So instead
// we never actually let Python hold an Instrument object directly.
void declareInstrumentVector(py::module & mod) {
    py::class_<InstrumentVector> cls(mod, "InstrumentVector");
    cls.def(py::init<>());
    cls.def(
        "add",
        [](InstrumentVector & self, std::string name, std::string band, py::dict devices) {
            std::unique_ptr<Instrument> instrument(new Instrument(name));
            instrument->band = band;
            for (auto item : devices) {
                auto device_name = py::cast<std::string>(item.first);
                auto domain = py::cast<Bounds<double>>(item.second);
                instrument->addDevice(device_name, domain);
            }
        },
        "name"_a, "band"_a, "devices"_a,
        "Add a new instrument to the vector.\n"
        "\n"
        "'devices' is a dict mapping detector name to the bounding box of \n"
        "that detector.\n"
    );
}


PYBIND11_MODULE(wcsfit, m) {
    declareBounds<double>(m, "D");
    declareSphericalCoords(m);
    declareFields(m);
    declareInstrumentVector(m);
}
