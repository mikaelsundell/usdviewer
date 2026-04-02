#include "application.h"
#include "session.h"

#include <pxr/external/boost/python.hpp>
#include <pxr/pxr.h>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace usdviewer;

static UsdStageRefPtr
getCurrentStage()
{
    Session* s = session();
    return s ? s->stage() : UsdStageRefPtr();
}

PXR_BOOST_PYTHON_MODULE(_usdviewer_native)
{
    using namespace PXR_BOOST_PYTHON_NAMESPACE;
    def("getCurrentStage", &getCurrentStage);
}

extern "C" PyObject*
PyInit__usdviewer_native_wrapper()
{
    return PyInit__usdviewer_native();
}
