#########################################
#            Embedded targets           #
#########################################
ROOT_DIR ?=..
# Local embedded makefile settings for custom bootloader and firmware targets.

ifneq ($(shell grep -s "#define SIMULATION" src/bsp.h),)
SIMULATION=0
endif

#Function to obtain parameter named $(1) in verilog header file located in $(2)
#Usage: $(call GET_MACRO,<param_name>,<vh_path>)
GET_MACRO = $(shell grep "define $(1)" $(2) | rev | cut -d" " -f1 | rev)

#Function to obtain parameter named $(1) from iob_soc_opencryptohw_conf.vh
GET_IOB_SOC_OPENCRYPTOHW_CONF_MACRO = $(call GET_MACRO,IOB_SOC_OPENCRYPTOHW_$(1),../src/iob_soc_opencryptohw_conf.vh)

iob_soc_opencryptohw_boot.hex: ../../software/iob_soc_opencryptohw_boot.bin
	../../scripts/makehex.py $< $(call GET_IOB_SOC_OPENCRYPTOHW_CONF_MACRO,BOOTROM_ADDR_W) > $@

iob_soc_opencryptohw_firmware.hex: iob_soc_opencryptohw_firmware.bin
ifeq ($(USE_EXTMEM),1)
	../../scripts/makehex.py $< $(call GET_IOB_SOC_OPENCRYPTOHW_CONF_MACRO,MEM_ADDR_W) > $@
else
	../../scripts/makehex.py $< $(call GET_IOB_SOC_OPENCRYPTOHW_CONF_MACRO,SRAM_ADDR_W) > $@
endif
	../../scripts/hex_split.py iob_soc_opencryptohw_firmware .

iob_soc_opencryptohw_firmware.bin: ../../software/iob_soc_opencryptohw_firmware.bin
	cp $< $@

../../software/%.bin:
	make -C ../../ fw-build

UTARGETS+=build_iob_soc_opencryptohw_software

TEMPLATE_LDS=src/$@.lds

IOB_SOC_OPENCRYPTOHW_INCLUDES=-I. -Isrc -Isrc/crypto/McEliece -Isrc/crypto/McEliece/common

IOB_SOC_OPENCRYPTOHW_LFLAGS=-Wl,-Bstatic,-T,$(TEMPLATE_LDS),--strip-debug

# FIRMWARE SOURCES
IOB_SOC_OPENCRYPTOHW_FW_SRC=src/iob_soc_opencryptohw_firmware.S
IOB_SOC_OPENCRYPTOHW_FW_SRC+=src/iob_soc_opencryptohw_firmware.c
IOB_SOC_OPENCRYPTOHW_FW_SRC+=src/printf.c

IOB_SOC_OPENCRYPTOHW_FW_SRC+=src/versat_crypto.c
IOB_SOC_OPENCRYPTOHW_FW_SRC+=src/crypto/aes.c
IOB_SOC_OPENCRYPTOHW_FW_SRC+=src/versat_crypto_common_tests.c
ifeq ($(SIMULATION),1)
IOB_SOC_OPENCRYPTOHW_FW_SRC+=src/versat_simple_crypto_tests.c
IOB_SOC_OPENCRYPTOHW_FW_SRC+=$(wildcard src/crypto/McEliece/arena.c)
IOB_SOC_OPENCRYPTOHW_FW_SRC+=$(wildcard src/crypto/McEliece/common/sha2.c)
else
IOB_SOC_OPENCRYPTOHW_FW_SRC+=src/versat_crypto_tests.c
IOB_SOC_OPENCRYPTOHW_FW_SRC+=src/versat_mceliece.c
IOB_SOC_OPENCRYPTOHW_FW_SRC+=$(wildcard src/crypto/McEliece/*.c)
IOB_SOC_OPENCRYPTOHW_FW_SRC+=$(wildcard src/crypto/McEliece/common/*.c)
endif

# PERIPHERAL SOURCES
IOB_SOC_OPENCRYPTOHW_FW_SRC+=$(wildcard src/iob-*.c)
IOB_SOC_OPENCRYPTOHW_FW_SRC+=$(filter-out %_emul.c, $(wildcard src/*swreg*.c))

# BOOTLOADER SOURCES
IOB_SOC_OPENCRYPTOHW_BOOT_SRC+=src/iob_soc_opencryptohw_boot.S
IOB_SOC_OPENCRYPTOHW_BOOT_SRC+=src/iob_soc_opencryptohw_boot.c
IOB_SOC_OPENCRYPTOHW_BOOT_SRC+=$(filter-out %_emul.c, $(wildcard src/iob*uart*.c))
IOB_SOC_OPENCRYPTOHW_BOOT_SRC+=$(filter-out %_emul.c, $(wildcard src/iob*cache*.c))

build_iob_soc_opencryptohw_software: iob_soc_opencryptohw_firmware iob_soc_opencryptohw_boot

iob_soc_opencryptohw_firmware:
	make $@.elf INCLUDES="$(IOB_SOC_OPENCRYPTOHW_INCLUDES)" LFLAGS="$(IOB_SOC_OPENCRYPTOHW_LFLAGS) -Wl,-Map,$@.map" SRC="$(IOB_SOC_OPENCRYPTOHW_FW_SRC)" TEMPLATE_LDS="$(TEMPLATE_LDS)"

iob_soc_opencryptohw_boot:
	make $@.elf INCLUDES="$(IOB_SOC_OPENCRYPTOHW_INCLUDES)" LFLAGS="$(IOB_SOC_OPENCRYPTOHW_LFLAGS) -Wl,-Map,$@.map" SRC="$(IOB_SOC_OPENCRYPTOHW_BOOT_SRC)" TEMPLATE_LDS="$(TEMPLATE_LDS)"


.PHONY: build_iob_soc_opencryptohw_software iob_soc_opencryptohw_firmware iob_soc_opencryptohw_boot

#########################################
#         PC emulation targets          #
#########################################
# Local pc-emul makefile settings for custom pc emulation targets.

# SOURCES
EMUL_SRC+=src/iob_soc_opencryptohw_firmware.c
EMUL_SRC+=src/printf.c

EMUL_INCLUDES = $(IOB_SOC_OPENCRYPTOHW_INCLUDES)

EMUL_SRC+=src/versat_crypto.c
EMUL_SRC+=src/crypto/aes.c
EMUL_SRC+=src/versat_crypto_common_tests.c
ifeq ($(SIMULATION),1)
EMUL_SRC+=src/versat_simple_crypto_tests.c
EMUL_SRC+=$(wildcard src/crypto/McEliece/arena.c)
EMUL_SRC+=$(wildcard src/crypto/McEliece/common/sha2.c)
else
EMUL_SRC+=src/versat_crypto_tests.c
EMUL_SRC+=src/versat_mceliece.c
EMUL_SRC+=$(wildcard src/crypto/McEliece/*.c)
EMUL_SRC+=$(wildcard src/crypto/McEliece/common/*.c)
endif

# PERIPHERAL SOURCES
EMUL_SRC+=$(wildcard src/iob-*.c)

