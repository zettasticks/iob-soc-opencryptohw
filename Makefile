CORE := iob_soc_opencryptohw

SIMULATOR ?= verilator
BOARD ?= AES-KU040-DB-G

DISABLE_LINT:=1

LIB_DIR:=submodules/IOBSOC/submodules/LIB
include $(LIB_DIR)/setup.mk

INIT_MEM ?= 0
VCD ?= 0
USE_EXTMEM := 1

ifeq ($(INIT_MEM),1)
SETUP_ARGS += INIT_MEM
endif

setup:
	make build-setup SETUP_ARGS="$(SETUP_ARGS)"

setup_pc:
	+nix-shell --run "make build-setup SETUP_ARGS='$(SETUP_ARGS) PC_EMUL'"

pc-emul:
	nix-shell --run "make clean"
	nix-shell --run "make setup_pc && make -C ../iob_soc_o* pc-emul-run"

sim-run:
	nix-shell --run "make clean"
	nix-shell --run "make setup INIT_MEM=1 SIMULATOR=$(SIMULATOR) VCD=$(VCD) && make -C ../iob_soc_o* sim-run INIT_MEM=1 SIMULATOR=$(SIMULATOR) VCD=$(VCD)"

sim-test:
	nix-shell --run "make clean"
	nix-shell --run "make setup INIT_MEM=1"
	nix-shell --run "make -C ../iob_soc_o* sim-run"
	nix-shell --run "make clean"
	nix-shell --run "make setup INIT_MEM=0"
	nix-shell --run "make -C ../iob_soc_o* sim-run SIMULATOR=verilator"

fpga-run:
	nix-shell --run 'make clean setup INIT_MEM=$(INIT_MEM) USE_EXTMEM=$(USE_EXTMEM)'
	nix-shell --run 'make -C ../$(CORE)_V*/ fpga-fw-build BOARD=$(BOARD)'
	make -C ../$(CORE)_V*/ fpga-run BOARD=$(BOARD)

fpga-run-only:
	cp -r software/ ../$(CORE)_V*/
	make -C ../$(CORE)_V*/ fpga-fw-build fpga-run BOARD=$(BOARD)

fpga-build:
	nix-shell --run 'make clean setup INIT_MEM=$(INIT_MEM) USE_EXTMEM=$(USE_EXTMEM)'
	nix-shell --run 'make -C ../$(CORE)_V*/ fpga-fw-build BOARD=$(BOARD)'
	make -C ../$(CORE)_V*/ fpga-build BOARD=$(BOARD) 

fpga-test:
	make clean setup fpga-run INIT_MEM=0

test-all:
	make sim-test
	make fpga-test BOARD=CYCLONEV-GT-DK
	make fpga-test BOARD=AES-KU040-DB-G
	make clean && make setup && make -C ../iob_soc_opencryptohw_V*/ doc-test

doc:
	mkdir ../iob_soc_opencryptohw_V0.70
	nix-shell --run "doxygen"

.PHONY: sim-test fpga-test test-all doc
