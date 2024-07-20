#!/usr/bin/env python3
import os
import sys
import shutil

from iob_soc import iob_soc
from iob_module import iob_module

from iob_versat import CreateVersatClass
from iob_eth import iob_eth
from iob_vexriscv import iob_vexriscv
from iob_reset_sync import iob_reset_sync

VERSAT_SPEC = "versatSpec.txt"
VERSAT_TOP = "Test"
VERSAT_EXTRA_UNITS = os.path.realpath(
    os.path.join(os.path.dirname(__file__), "hardware/src/units")
)

print("IOB_SOC_VERSAT", file=sys.stderr)

pc_emul = False
for arg in sys.argv[1:]:
    if arg == "PC_EMUL":
        pc_emul = True


class iob_soc_opencryptohw(iob_soc):
    name = "iob_soc_opencryptohw"
    version = "V0.70"
    flows = "pc-emul emb sim doc fpga"
    setup_dir = os.path.dirname(__file__)

    @classmethod
    def _create_instances(cls):
        super()._create_instances()

        # if iob_vexriscv in cls.submodule_list:
        #    cls.cpu = iob_vexriscv("cpu_0")
        if cls.versat_type in cls.submodule_list:
            cls.versat = cls.versat_type("VERSAT0", "Versat accelerator")
            cls.peripherals.append(cls.versat)

        if iob_eth in cls.submodule_list:
            cls.peripherals.append(
                iob_eth(
                    "ETH0",
                    "Ethernet interface",
                    parameters={
                        "AXI_ID_W": "AXI_ID_W",
                        "AXI_LEN_W": "AXI_LEN_W",
                        "AXI_ADDR_W": "AXI_ADDR_W",
                        "AXI_DATA_W": "AXI_DATA_W",
                    },
                )
            )

    @classmethod
    def _create_submodules_list(cls, extra_submodules=[]):
        """Create submodules list with dependencies of this module"""

        cls.versat_type = CreateVersatClass(
            pc_emul,
            VERSAT_SPEC,
            "CryptoAlgos",
            VERSAT_EXTRA_UNITS,
            cls.build_dir,
            os.path.realpath(cls.setup_dir + "../debug/"),
        )

        super()._create_submodules_list(
            [
                {"interface": "peripheral_axi_wire"},
                {"interface": "intmem_axi_wire"},
                {"interface": "dBus_axi_wire"},
                {"interface": "iBus_axi_wire"},
                {"interface": "dBus_axi_m_port"},
                {"interface": "iBus_axi_m_port"},
                {"interface": "dBus_axi_m_portmap"},
                {"interface": "iBus_axi_m_portmap"},
                # iob_vexriscv,
                cls.versat_type,
                iob_reset_sync,
            ]
            + extra_submodules
        )

        # Remove iob_picorv32 because we want vexriscv
        # i = 0
        # while i < len(cls.submodule_list):
        #    if type(cls.submodule_list[i]) == type and cls.submodule_list[i].name in [
        #        "iob_picorv32",
        #        # "iob_cache",
        #    ]:
        #        cls.submodule_list.pop(i)
        #        continue
        #    i += 1

    @classmethod
    def _post_setup(cls):
        super()._post_setup()

        shutil.copytree(
            f"{cls.setup_dir}/hardware/src/units",
            f"{cls.build_dir}/hardware/src",
            dirs_exist_ok=True,
        )

        dst = f"{cls.build_dir}/software/src"

        # Copy scripts to scripts build directory
        iob_soc_scripts = [
            "terminalMode",
            "makehex",
            "hex_split",
            "hex_join",
            "board_client",
            "console",
            "console_ethernet",
        ]

        dst = f"{cls.build_dir}/scripts"
        for script in iob_soc_scripts:
            src_file = f"{__class__.setup_dir}/submodules/IOBSOC/scripts/{script}.py"
            shutil.copy2(src_file, dst)
        src_file = f"{__class__.setup_dir}/scripts/check_if_run_linux.py"
        shutil.copy2(src_file, dst)

        if cls.is_top_module:
            # Set ethernet MAC address
            append_str_config_build_mk(
                """
### Set Ethernet environment variables
#Mac address of pc interface connected to ethernet peripheral (based on board name)
$(if $(findstring sim,$(MAKECMDGOALS))$(SIMULATOR),$(eval BOARD=))
ifeq ($(BOARD),AES-KU040-DB-G)
RMAC_ADDR ?=989096c0632c
endif
ifeq ($(BOARD),CYCLONEV-GT-DK)
RMAC_ADDR ?=309c231e624b
endif
RMAC_ADDR ?=000000000000
export RMAC_ADDR
#Set correct environment if running on IObundle machines
ifneq ($(filter pudim-flan sericaia,$(shell hostname)),)
IOB_CONSOLE_PYTHON_ENV ?= /opt/pyeth3/bin/python
endif
                    """,
                cls.build_dir,
            )

    @classmethod
    def _setup_confs(cls, extra_confs=[]):
        # Append confs or override them if they exist

        confs = [
            {
                "name": "SRAM_ADDR_W",
                "type": "P",
                "val": "20",
                "min": "1",
                "max": "32",
                "descr": "SRAM address width",
            },
            {
                "name": "USE_ETHERNET",
                "type": "M",
                "val": True,
                "min": "0",
                "max": "1",
                "descr": "Enable ethernet support.",
            },
        ]

        if cls.versat_type.USE_EXTMEM:
            print("USING EXT MEM BECAUSE VERSAT", file=sys.stderr)
            confs.append(
                {
                    "name": "USE_EXTMEM",
                    "type": "M",
                    "val": True,
                    "min": "0",
                    "max": "1",
                    "descr": "Versat AXI implies External memory",
                }
            )

        super()._setup_confs(confs)

    @classmethod
    def _setup_portmap(cls):
        if iob_eth in cls.submodule_list:
            cls.peripheral_portmap += [
                # ETHERNET
                (
                    {
                        "corename": "ETH0",
                        "if_name": "general",
                        "port": "inta_o",
                        "bits": [],
                    },
                    {
                        "corename": "internal",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
                # phy - connect to external interface
                (
                    {
                        "corename": "ETH0",
                        "if_name": "phy",
                        "port": "MTxClk",
                        "bits": [],
                    },
                    {
                        "corename": "external",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
                (
                    {
                        "corename": "ETH0",
                        "if_name": "phy",
                        "port": "MTxD",
                        "bits": [],
                    },
                    {
                        "corename": "external",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
                (
                    {
                        "corename": "ETH0",
                        "if_name": "phy",
                        "port": "MTxEn",
                        "bits": [],
                    },
                    {
                        "corename": "external",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
                (
                    {
                        "corename": "ETH0",
                        "if_name": "phy",
                        "port": "MTxErr",
                        "bits": [],
                    },
                    {
                        "corename": "external",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
                (
                    {
                        "corename": "ETH0",
                        "if_name": "phy",
                        "port": "MRxClk",
                        "bits": [],
                    },
                    {
                        "corename": "external",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
                (
                    {
                        "corename": "ETH0",
                        "if_name": "phy",
                        "port": "MRxDv",
                        "bits": [],
                    },
                    {
                        "corename": "external",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
                (
                    {
                        "corename": "ETH0",
                        "if_name": "phy",
                        "port": "MRxD",
                        "bits": [],
                    },
                    {
                        "corename": "external",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
                (
                    {
                        "corename": "ETH0",
                        "if_name": "phy",
                        "port": "MRxErr",
                        "bits": [],
                    },
                    {
                        "corename": "external",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
                (
                    {
                        "corename": "ETH0",
                        "if_name": "phy",
                        "port": "MColl",
                        "bits": [],
                    },
                    {
                        "corename": "external",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
                (
                    {
                        "corename": "ETH0",
                        "if_name": "phy",
                        "port": "MCrS",
                        "bits": [],
                    },
                    {
                        "corename": "external",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
                (
                    {
                        "corename": "ETH0",
                        "if_name": "phy",
                        "port": "MDC",
                        "bits": [],
                    },
                    {
                        "corename": "external",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
                (
                    {
                        "corename": "ETH0",
                        "if_name": "phy",
                        "port": "MDIO",
                        "bits": [],
                    },
                    {
                        "corename": "external",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
                (
                    {
                        "corename": "ETH0",
                        "if_name": "phy",
                        "port": "phy_rstn_o",
                        "bits": [],
                    },
                    {
                        "corename": "external",
                        "if_name": "ETH0",
                        "port": "",
                        "bits": [],
                    },
                ),
            ]