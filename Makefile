VERBOSE := 1

TOPSRC := $(CURDIR)

P4C_BM_DIR := $(TOPSRC)/submodules/p4c-bm

PD_MK_DIR := $(P4C_BM_DIR)/pd_mk

BMV2_TARGET_NAME := switch
BMV2_TARGET_DIR := $(TOPSRC)/targets/$(BMV2_TARGET_NAME)/bmv2

USR_DIR := $(DESTDIR)$(prefix)
INC_DIR := $(USR_DIR)/include
LIB_DIR := $(USR_DIR)/lib
SBIN_DIR := $(USR_DIR)/sbin
SHARE_DIR := $(USR_DIR)/share/p4

export VERBOSE
export TOPSRC
export PD_MK_DIR

all: build
	@echo Done.

build:
	$(MAKE) -j1 -C $(BMV2_TARGET_DIR) bm

install:
	install -m 0755 -o root -g root -d $(SBIN_DIR)
	install -m 0755 -o root -g root \
		$(BMV2_TARGET_DIR)/$(BMV2_TARGET_NAME)_bmv2 \
		$(SBIN_DIR)/

	install -m 0755 -o root -g root -d $(USR_DIR)
	tar -C $(BMV2_TARGET_DIR)/build/bmv2_pd -cf - include lib | \
		tar -C $(USR_DIR)/ -xvpf -

	install -m 0755 -o root -g root -d $(SHARE_DIR)
	install -m 0644 -o root -g root \
		$(BMV2_TARGET_DIR)/$(BMV2_TARGET_NAME)_bmv2.json \
		$(SHARE_DIR)/

clean:
	$(MAKE) -C $(TOPSRC)/submodules/bm $@
	$(MAKE) -C $(TOPSRC)/submodules/p4c-bm/pd_mk $@
	$(MAKE) -C $(BMV2_TARGET_DIR) $@

distclean: clean
	$(MAKE) -C $(TOPSRC)/submodules/bm $@
	$(MAKE) -C $(TOPSRC)/submodules/p4c-bm/pd_mk $@

.PHONY: all build clean distclean
