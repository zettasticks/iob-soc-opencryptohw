#ROOT_DIR=.
#include $(ROOT_DIR)/config.mk

CORE := iob_soc_opencryptolinux

SIMULATOR ?= verilator
BOARD ?= AES-KU040-DB-G

DISABLE_LINT:=1

LIB_DIR:=submodules/IOBSOC/submodules/LIB
include $(LIB_DIR)/setup.mk

INIT_MEM ?= 0
RUN_LINUX ?= 0
USE_EXTMEM := 1
BOOT_FLOW ?= CONSOLE_TO_EXTMEM

ifeq ($(INIT_MEM),1)
SETUP_ARGS += INIT_MEM
endif

setup:
	make build-setup SETUP_ARGS="$(SETUP_ARGS)"

sim-test:
	nix-shell --run "make clean"
	nix-shell --run "make setup INIT_MEM=1"
	nix-shell --run "make -C ../iob_soc_o* sim-run"
	nix-shell --run "make clean"
	nix-shell --run "make setup INIT_MEM=0"
	nix-shell --run "make -C ../iob_soc_o* sim-run SIMULATOR=verilator"

fpga-run:
	nix-shell --run 'make clean setup INIT_MEM=$(INIT_MEM) USE_EXTMEM=$(USE_EXTMEM)'
	nix-shell --run 'make -C ../$(CORE)_V*/ fpga-fw-build BOARD=$(BOARD) RUN_LINUX=$(RUN_LINUX)'
	make -C ../$(CORE)_V*/ fpga-run BOARD=$(BOARD) RUN_LINUX=$(RUN_LINUX) BOOT_FLOW=$(BOOT_FLOW)

fpga-build:
	nix-shell --run 'make clean setup INIT_MEM=$(INIT_MEM) USE_EXTMEM=$(USE_EXTMEM)'
	nix-shell --run 'make -C ../$(CORE)_V*/ fpga-fw-build BOARD=$(BOARD) RUN_LINUX=$(RUN_LINUX)'
	make -C ../$(CORE)_V*/ fpga-build BOARD=$(BOARD) RUN_LINUX=$(RUN_LINUX) 

fpga-connect:
	nix-shell --run 'make -C ../$(CORE)_V*/ fpga-fw-build BOARD=$(BOARD) RUN_LINUX=$(RUN_LINUX)'
	# Should run under 'bash', running with 'fish' as a shell gives an error
	make -C ../$(CORE)_V*/ fpga-run BOARD=$(BOARD) RUN_LINUX=$(RUN_LINUX) BOOT_FLOW=$(BOOT_FLOW)

fpga-test:
	make clean setup fpga-run INIT_MEM=0

#
# FUSESOC TARGETS
#
FUSESOC_DIR=fusesoc
fusesoc-setup:
	make -C $(FUSESOC_DIR) setup ALGORITHM=$(ALGORITHM)

fusesoc-sim-setup:
	make -C $(FUSESOC_DIR) sim-setup ALGORITHM=$(ALGORITHM)

fusesoc-sim-build:
	make -C $(FUSESOC_DIR) sim-build ALGORITHM=$(ALGORITHM)
		
fusesoc-sim-run:
	make -C $(FUSESOC_DIR) sim-run ALGORITHM=$(ALGORITHM)

fusesoc-fpga-setup:
	make -C $(FUSESOC_DIR) fpga-setup ALGORITHM=$(ALGORITHM)

fusesoc-fpga-build:
	make -C $(FUSESOC_DIR) fpga-build ALGORITHM=$(ALGORITHM)
		
fusesoc-fpga-run: fusesoc-fpga-build
	make -C $(FUSESOC_DIR) fpga-run ALGORITHM=$(ALGORITHM)

fusesoc-clean:
	make -C $(FUSESOC_DIR) clean ALGORITHM=$(ALGORITHM)

#
# OPENLANE TARGETS
#
OPENLANE_FLOW_DIR=$(HW_DIR)/asic/openlane
openlane-setup:
	make -C $(OPENLANE_FLOW_DIR) setup
openlane-run:
	make -C $(OPENLANE_FLOW_DIR) run
openlane-clean:
	make -C $(OPENLANE_FLOW_DIR) clean
openlane-post-synth-sim:
	make -C $(OPENLANE_FLOW_DIR)/simulation test OPENLANE_SIM_TYPE=post-synth
openlane-post-layout-sim:
	make -C $(OPENLANE_FLOW_DIR)/simulation test OPENLANE_SIM_TYPE=post-layout
openlane-sim-clean:
	make -C $(OPENLANE_FLOW_DIR)/simulation clean

#
# GENERATE SPINALHDL VERILOG SOURCES
#
gen-spinal-sources:
	make -C $(HW_DIR) gen-spinal-sources

