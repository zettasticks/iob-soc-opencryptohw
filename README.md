# What is IOb-SoC-OpenCryptoHW

IOb-SoC-OpenCryptoHW is a system on a chip that runs the SHA, AES, and McEliece algorithms on a RISC-V processor coupled with a custom Versat Coarse-Grained Reconfigurable Array (CGRA) as a hardware accelerator.


## Table of Contents

- [Cloning the Repository](#cloning-the-repository)
- [Requirements](#requirements)
- [Running the Project](#running-the-project)
- [IOb-SoC/Versat Tutorial](#iob-soc-versat-tutorial)
    * [SHA-256](#sha-256)
    * [AES](#aes)
    * [McEliece](#mceliece)
    * [Full implementation](#full-implementation)
    * [Tests](#tests)
- [License](#license)
- [Acknowledgement](#acknowledgement)

# Cloning the Repository

This repository uses git sub-module trees and GitHub will ask for your password for each downloaded module if you clone it by *https*. To avoid this, set GitHub access with *ssh* and type:

```
git clone --recursive git@github.com:IObundle/iob-soc-opencryptohw.git
```

Alternatively, you can still clone this repository using *https* if you cache
your credentials before cloning the repository using: or using the URL:
```
git config --global credential.helper 'cache --timeout <time_in_seconds>'
```

# Requirements

IOb-SoC-OpenCryptoHW is based on IOb-SoC+IOb-Versat system, as exemplified in
the repository [IOb-SoC-Versat](https://github.com/IObundle/iob-soc-versat). This repository should be checked to install all required
dependencies.

# Running the Project

To run PC emulation, type:

```bash
make pc-emul
```

To run simulation, type:

```bash
make sim-run
```

To run FPGA emulation, type:

```bash
make fpga-run
```

More targets can be found in the Makefile.


# IOb-SoC/Versat Tutorial

Versat is a tool that generates custom-made accelerators following the dataflow paradigm. The accelerator used in IOb-SoC-OpenCryptoHW is specified in the Versat native specification language and can be found in the file ./versatSpec.txt. The majority of units used are either primary or complex default Versat units. Other units have been customized for this project and can be found in hardware\src\units.

Given that the IOb-Versat already explains how Versat works and there are illustrative examples in the IOb-SoC-Versat repository, this tutorial will only explain how the three crypto algorithms are implemented. All accelerators are described in the file ./versatSpec.txt, and the firmware is in the directory software/src. A software manual is delivered in the document directory.

## SHA-256

SHA-256 is a hash algorithm that transforms a sequence of bytes into a 256-bit hash value. SHA first starts by initializing a state with a predefined value and dividing the input into blocks of equal size. Then, each block of the input combines with the current state to generate the new state, which is combined with the next block until no more blocks are left. 

To speed up SHA-256, we designed an accelerator to process one block per run. In software, this portion is fully controlled by the function versat_crypto_hashblocks_sha256, defined in versat_sha.c. 

The accelerator stores the state inside it and contains some memories to store all the constants required by the SHA algorithm. The logic is implemented by instantiating the xunitF and xunitM custom units, written in Verilog and found in ./hardware/src/units.

We use a VRead unit to load the input. This unit allows us to process data while reading the next block, effectively hiding the read latency. Since we only need to read a block (64 bytes = 16 reads of 4 bytes at a time) and it takes around 67 cycles to process a block, the accelerator is practically never waiting for memory and is always doing useful work.

The full implementation of SHA-256 using Versat is called VersatSHA. This function expects the entire input to be passed as an argument. 

The last input block needs to be handled differently. Since SHA processes 64 bytes at a time, it employs a padding scheme to ensure that any number of blocks can be easily processed. This scheme basically always inserts a final block composed mostly of zeros except the last bytes, which contain information about the number of bytes processed.

## AES

AES is a symmetric key cryptographic algorithm that performs encryption and decryption of blocks of data given a key of size 128, 196, or 256 bits, depending on the version being used. Our implementation is capable of handling 128 and 256-bit keys.

AES defines the concept of rounds, which consist of a collection of steps that are performed repeatedly. A full AES implementation is usually divided into two parts: KeyExpansion and Encrypt/Decryption.

The first part is called KeyExpansion and is responsible for expanding the initial key. Since the result of a key expansion is always the same for the same key, the key expansion only needs to be performed once per key used. 

While the expansion of the key could be performed entirely on software, we still implement it on the accelerator since it is a fraction of the runtime for significant inputs. Fully described using Versat, the logic to generate the key is contained inside the FullAES module, the GenericKeySchedule256 module, and its subunits. Function ExpandKey includes the code to expand a key. 

The second part is to encrypt/decrypt the input. AES is a block cipher, which divides the input into blocks of 16 bytes, performs multiple rounds, and the resulting 16 bytes is the output of that run. While the algorithm is based on applying the same rounds various times, the last round differs slightly from the usual round. Also, a bit of simple pre-round logic needs to be used. Decrypt is the inverse of encryption: we need to perform the opposite steps, meaning that we have six different forms of a "round": 3 forms for the pre-round, average round, and final round of encryption and an equal amount for the inverse.

We accomplish this fully in Versat, by defining each round individually and then performing a merge of all the round types. The FullAES implementation instantiates this merged unit, called FullAESRounds, and in software, we change between round kinds according to our needs; we implement our algorithm at the level of a round: each accelerator run does one round and to process a block, we need to perform 11 (15) accelerator runs for AES-128 (AES-256).

Because AES is a block cipher, we need to implement a block cipher mode to process inputs of variable size. Our implementation supports ECB, CBC and CTR modes. These modes can be seen on versat_aes.c. Some extra units must be inserted in the accelerator to support these modes.

The Encrypt and Decrypt functions can be found inside versat_aes.c. Other than some logic related to the block cipher mode of operation, the software implementation basically only needs to select the correct key values to be used by the block cipher and change the merged instances when needed.

A more thorough explanation of AES can be found at https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197.pdf.

## McEliece

The McEliece algorithm is an asymmetric form of encryption designed for a Key encapsulation mechanism (KEM). After profiling the full run of McEliece, running Generation, Encapsulation, and Decapsulation, it was found that one portion of the Generation accounted for the majority of the time spent, and it was that portion that we decided to accelerate.

McEliece is defined for various parameters that define the algorithm's strength. We only provided an implementation for the McEliece348864 parameter set.

The part that took the majority of the time was a simple loop in the code that performed Gaussian elimination of a big bit matrix. We accelerate it by saving the current row being processed internally inside the accelerator and using VRead and VWrite units to load the other rows, process them with the current row, and store the result back into memory. The entire McEliece accelerator is described by the single unit called McEliece.

More information about the algorithm, as well as the site where we obtained the KAT files, can be found [here](https://classic.mceliece.org/nist.html)

## Full implementation

A unit called CryptoAlgos, which instantiates the SHA, AES, and McEliece units, describes the full implementation.

## Tests

Each cryptographic algorithm is tested by comparing it to a KAT. AES was tested for the 256-bit variant in ECB mode.

# License

The IOb-SoC-OpenCryptoHW is licensed under the MIT License. See the LICENSE file for more information.


# Acknowledgement
This project is funded through the NGI Assure Fund, a fund established by NLnet
with financial support from the European Commission's Next Generation Internet
programme, under the aegis of DG Communications Networks, Content and Technology
under grant agreement No 957073.

<table>
    <tr>
        <td align="center" width="50%"><img src="https://nlnet.nl/logo/banner.svg" alt="NLnet foundation logo" style="width:90%"></td>
        <td align="center"><img src="https://nlnet.nl/image/logos/NGIAssure_tag.svg" alt="NGI Assure logo" style="width:90%"></td>
    </tr>
</table>
