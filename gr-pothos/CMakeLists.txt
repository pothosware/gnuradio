# Copyright 2015-2015 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

########################################################################
# Setup dependencies
########################################################################
include(GrPython)

GR_PYTHON_CHECK_MODULE("python >= 2.7"     sys          "sys.version.split()[0] >= '2.7'"           PYTHON_MIN_VER_FOUND)
GR_PYTHON_CHECK_MODULE("Cheetah >= 2.0.0"  Cheetah      "Cheetah.Version >= '2.0.0'"                CHEETAH_FOUND)

########################################################################
# Register component
########################################################################
include(GrComponent)
if(NOT CMAKE_CROSSCOMPILING)
    set(gr_pothos_python_deps
        PYTHON_MIN_VER_FOUND
        CHEETAH_FOUND
    )
endif(NOT CMAKE_CROSSCOMPILING)

GR_REGISTER_COMPONENT("gr-pothos" ENABLE_POTHOS
    ENABLE_GNURADIO_RUNTIME
    ENABLE_PYTHON
    ${gr_pothos_python_deps}
)
########################################################################
# Begin conditional configuration
########################################################################
if(ENABLE_POTHOS)

########################################################################
# Setup CPack components
########################################################################
include(GrPackage)
CPACK_COMPONENT("pothos_python"
    DISPLAY_NAME "Pothos"
    DESCRIPTION  "GNU Radio Pothos Utils"
    DEPENDS      "runtime_python"
)

########################################################################
# Install python files
########################################################################
GR_PYTHON_INSTALL(
    FILES
        __init__.py
        CppHeaderParser.py
        GrPothosParser.py
        xmltodict.py
    DESTINATION ${GR_PYTHON_DIR}/gnuradio/pothos
    COMPONENT "pothos_python"
)

INSTALL(
    FILES
        registration.tmpl.cpp
    DESTINATION ${GR_PYTHON_DIR}/gnuradio/pothos
    COMPONENT "pothos_python"
)

GR_PYTHON_INSTALL(
    PROGRAMS GrPothosUtil
    DESTINATION ${GR_RUNTIME_DIR}
    COMPONENT "pothos_python"
)

INSTALL(
    FILES cmake/CMakeLists.txt
    DESTINATION share/cmake/gr-pothos
    COMPONENT "pothos_python"
)

endif(ENABLE_POTHOS)

