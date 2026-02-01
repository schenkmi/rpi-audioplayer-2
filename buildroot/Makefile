PLATFORM?=RPI3_64
TEST:=test
BUILDROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
NPROC:=$(shell nproc)

########################################
# BUILDROOT
########################################
BUILDROOT_VERSION=2025.11.1
BUILDROOT_XZ=buildroot-$(BUILDROOT_VERSION).tar.xz
BUILDROOT_SRC_DIR=$(BUILDROOT_DIR)/buildroot-$(BUILDROOT_VERSION)
BUILDROOT_INSTALL_DIR=$(BUILDROOT_SRC_DIR)/output
LOCAL_NAME=$(BUILDROOT_XZ)
BUILDROOT_EXTERNAL=../buildroot-external
BUILDROOT_PATCH_LIST=../patches/0001-package-libnpupnp-6.2.3.patch \
                     ../patches/0001-package-libupnpp-1.0.3.patch \
                     ../patches/0001-package-upmpdcli-1.9.7.patch

ifneq (,$(filter $(PLATFORM),RPI3_64))
  BUILDROOTCFG=rpi3_64_defconfig
  BUILDROOT_TC_TAG=toolchain-rpi3_64
  BUILDROOT_TC_DIR=$(BUILDROOT_DIR)/$(BUILDROOT_TC_TAG)
endif
ifneq (,$(filter $(PLATFORM),RPI4_64))
  BUILDROOTCFG=rpi4_64_defconfig
  BUILDROOT_TC_TAG=toolchain-rpi4_64
  BUILDROOT_TC_DIR=$(BUILDROOT_DIR)/$(BUILDROOT_TC_TAG)
endif

EXE=$(BUILDROOT_XZ)

all: config build sdk install

patchbuildroot:
	@echo === start of target $@ for $(EXE) ==================
	@for p in  $(BUILDROOT_PATCH_LIST); do          \
	  echo "--> apply patch $$p" ; 					\
	  cd $(BUILDROOT_SRC_DIR) && patch -p1 < $$p ; 	\
	done
	@echo === end of target $@ for $(EXE) ====================

.PHONY: config
config:
	@echo === start of target $@ for $(EXE) ==================
	$(TEST) -d $(BUILDROOT_SRC_DIR) || (cd $(BUILDROOT_DIR) && tar xJvf $(BUILDROOT_XZ) && $(MAKE) patchbuildroot)
	$(TEST) -e $(BUILDROOT_SRC_DIR)/.config || (cd $(BUILDROOT_SRC_DIR) && make O=../$(BUILDROOT_TC_TAG) BR2_EXTERNAL=$(BUILDROOT_EXTERNAL) $(BUILDROOTCFG))
	@echo === end of target $@ for $(EXE) ====================

.PHONY: re-config
re-config:
	@echo === start of target $@ for $(EXE) ==================
	$(TEST) -d $(BUILDROOT_SRC_DIR) || (cd $(BUILDROOT_DIR) && tar xJvf $(BUILDROOT_XZ))
	cd $(BUILDROOT_SRC_DIR) && make O=../$(BUILDROOT_TC_TAG) BR2_EXTERNAL=$(BUILDROOT_EXTERNAL) $(BUILDROOTCFG)
	@echo === end of target $@ for $(EXE) ====================

.PHONY: clean-config
clean-config:
	@echo === start of target $@ for $(EXE) ==================
	$(TEST) -d $(BUILDROOT_SRC_DIR) || (cd $(BUILDROOT_DIR) && tar xJvf $(BUILDROOT_XZ))
	cd $(BUILDROOT_SRC_DIR) && make O=../$(BUILDROOT_TC_TAG) BR2_EXTERNAL=$(BUILDROOT_EXTERNAL) clean
	cd $(BUILDROOT_SRC_DIR) && make O=../$(BUILDROOT_TC_TAG) BR2_EXTERNAL=$(BUILDROOT_EXTERNAL) $(BUILDROOTCFG)
	@echo === end of target $@ for $(EXE) ====================

.PHONY: menuconfig
menuconfig:
	@echo === start of target $@ for $(EXE) ==================
	cd $(BUILDROOT_SRC_DIR) && make LD_LIBRARY_PATH="" O=../$(BUILDROOT_TC_TAG) BR2_EXTERNAL=$(BUILDROOT_EXTERNAL) menuconfig
	@echo === end of target $@ for $(EXE) ====================

.PHONY: linux-menuconfig
linux-menuconfig:
	@echo === start of target $@ for $(EXE) ==================
	cd $(BUILDROOT_SRC_DIR) && make LD_LIBRARY_PATH="" O=../$(BUILDROOT_TC_TAG) BR2_EXTERNAL=$(BUILDROOT_EXTERNAL) linux-menuconfig
	@echo === end of target $@ for $(EXE) ====================

.PHONY: update-defconfig
update-defconfig:
	@echo === start of target $@ for $(EXE) ==================
	cd $(BUILDROOT_SRC_DIR) && make O=../$(BUILDROOT_TC_TAG) BR2_EXTERNAL=$(BUILDROOT_EXTERNAL) update-defconfig
	@echo === end of target $@ for $(EXE) ====================

.PHONY: build
build:
	@echo === start of target $@ for $(EXE) ==================
	cd $(BUILDROOT_SRC_DIR) && make -j $(NPROC) LD_LIBRARY_PATH="" O=../$(BUILDROOT_TC_TAG) BR2_EXTERNAL=$(BUILDROOT_EXTERNAL)
	@echo === end of target $@ for $(EXE) ====================

.PHONY: install
install:
	@echo === start of target $@ for $(EXE) ==================
	$(TEST) ! -f $(BUILDROOT_TC_DIR)/images/sdcard.img || cp $(BUILDROOT_TC_DIR)/images/sdcard.img $(BUILDROOT_DIR)/../$(PLATFORM)-sdcard.img
	@cd $(BUILDROOT_TC_DIR)/images; TC_ARTIFACT=`ls *-buildroot.tar.gz | head -1`; \
	if [ -f "$$TC_ARTIFACT" ]; then \
		echo "copy toolchain $$TC_ARTIFACT"; \
		cp $(BUILDROOT_TC_DIR)/images/$$TC_ARTIFACT $(BUILDROOT_DIR)/../$(PLATFORM)-$$TC_ARTIFACT; \
	fi
	@echo === end of target $@ for $(EXE) ====================

.PHONY: sdk
sdk:
	@echo === start of target $@ for $(EXE) ==================
	cd $(BUILDROOT_SRC_DIR) && make LD_LIBRARY_PATH="" O=../$(BUILDROOT_TC_TAG) BR2_EXTERNAL=$(BUILDROOT_EXTERNAL) $@
	@echo === end of target $@ for $(EXE) ====================

.PHONY: clean
clean:
	@echo === start of target $@ for $(EXE) ==================
	$(TEST) ! -d $(BUILDROOT_SRC_DIR) || rm -rf $(BUILDROOT_SRC_DIR)
	@echo === end of target $@ for $(EXE) ====================
