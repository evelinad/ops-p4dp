################################################################
#
#        Copyright 2013, Big Switch Networks, Inc. 
#        Copyright 2013, Barefoot Networks, Inc. 
# 
# Licensed under the Eclipse Public License, Version 1.0 (the
# "License"); you may not use this file except in compliance
# with the License. You may obtain a copy of the License at
# 
#        http://www.eclipse.org/legal/epl-v10.html
# 
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
# either express or implied. See the License for the specific
# language governing permissions and limitations under the
# License.
#
################################################################

#
# The root of of our repository is here:
#
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

#
# Resolve submodule dependencies. 
# Please keep alphabetized

ifndef SUBMODULE_BM
  ifdef SUBMODULES
    SUBMODULE_BM := $(SUBMODULES)/bm
  else
    SUBMODULE_BM := $(ROOT)/submodules/bm
    SUBMODULES_LOCAL += bm
  endif
endif

ifndef SUBMODULE_P4C_BM
  ifdef SUBMODULES
    SUBMODULE_P4C_BM := $(SUBMODULES)/p4c-bm
  else
    SUBMODULE_P4C_BM := $(ROOT)/submodules/p4c-bm
    SUBMODULES_LOCAL += p4c-bm
  endif
endif

export SUBMODULE_BM
export SUBMODULE_P4C_BM

MODULE_DIRS := $(ROOT)/modules

.show-submodules:
	@echo bm @ $(SUBMODULE_BM)
	@echo p4c_bm @ $(SUBMODULE_P4C_BM)
