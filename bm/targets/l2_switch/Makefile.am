SUBDIRS = . learn_client

AM_CPPFLAGS += \
-I$(top_srcdir)/modules/bm_sim/include \
-I$(top_srcdir)/modules/bm_runtime/include
bin_PROGRAMS = l2_switch
l2_switch_SOURCES = l2_switch.cpp primitives.cpp
l2_switch_LDADD = $(top_builddir)/modules/bm_runtime/libbmruntime.la $(top_builddir)/modules/bm_sim/libbmsim.la $(top_builddir)/modules/bf_lpm_trie/libbflpmtrie.la $(top_builddir)/thrift_src/libbmthrift.la $(top_builddir)/modules/BMI/libbmi.la $(top_builddir)/third_party/jsoncpp/libjson.la -lboost_system -lboost_thread -lthrift -lboost_program_options
l2_switch_LDFLAGS = -pthread
