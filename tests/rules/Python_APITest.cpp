// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2009 Alistair Riddoch
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA


#ifdef NDEBUG
#undef NDEBUG
#else
#define CYPHESIS_DEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include <Python.h>

#include "../python_testers.h"

#include "rules/python/Python_API.h"
#include "rules/python/Python_Script_Utils.h"

#include "../stubs/navigation/stubAwareness.h"
#include "../stubs/navigation/stubSteering.h"

#include <cassert>
#include <rules/python/CyPy_Rules.h>
#include "rules/python/CyPy_Atlas.h"
#include "rules/python/CyPy_Physics.h"
#include "rules/python/CyPy_Common.h"


int main()
{

    init_python_api({&CyPy_Atlas::init,
                     &CyPy_Physics::init,
                     &CyPy_Common::init,
                     &CyPy_Rules::init});

    auto amod = Get_PyModule("notamodule");
    assert(amod.isNull());

    amod = Get_PyModule("http.server");
    assert(!amod.isNull());
    Py::Module mod(amod.ptr());
    auto cls = Get_PyClass(mod, "BaseHTTPServer", "noclassbythisname");
    assert(cls.isNull());
    cls = Get_PyClass(mod, "BaseHTTPServer", "HTTPServer");
    assert(!cls.isNull() && cls.isCallable());
    cls = Get_PyClass(mod, "BaseHTTPServer", "__all__");
    assert(cls.isNull());
    // We don't actually get a class back, because apparantly classes in
    // the library don't inherit from object yet.

    run_python_string("print('hello')");
    run_python_string("import sys");
    run_python_string("sys.stdout.write('hello')");
    expect_python_error("sys.stdout.write(1)", PyExc_TypeError);
    run_python_string("sys.stderr.write('hello')");
    expect_python_error("sys.stderr.write(1)", PyExc_TypeError);

    run_python_string("from common import log");
    run_python_string("log.debug('foo')");
    run_python_string("log.thinking('foo')");

    run_python_string("import rules");

    run_python_string("l=rules.Location()");
    run_python_string("rules.isLocation(l)");
    run_python_string("rules.isLocation(1)");
    run_python_string("l1=rules.Location()");
    run_python_string("l2=rules.Location()");

    run_python_string("import physics");
    expect_python_error("physics.distance_to()", PyExc_IndexError);
    expect_python_error("physics.square_distance()", PyExc_IndexError);
    expect_python_error("physics.square_horizontal_distance()",
                        PyExc_IndexError);
    run_python_string("physics.distance_to(l1, l2)");
    run_python_string("physics.square_distance(l1, l2)");
    run_python_string("physics.square_horizontal_distance(l1, l2)");
    expect_python_error("physics.distance_to('1', l2)", PyExc_TypeError);
    expect_python_error("physics.square_distance('1', l2)", PyExc_TypeError);
    expect_python_error("physics.square_horizontal_distance('1', l2)",
                        PyExc_TypeError);


    shutdown_python_api();
    return 0;
}
