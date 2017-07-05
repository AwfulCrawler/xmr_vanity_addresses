# Monero Vanity Address Generator


## Introduction

Works with a list of words to find vanity addresses.

Word lists not included.  Example word list can be found [here](https://github.com/dwyl/english-words).

## License

See [LICENSE](LICENSE).


## Build instructions

### On Linux

* Install the dependencies (Same dependencies as monero)
* Run `get_libwallet_api.sh` to download and compile the merged wallet library.
* Run `make`.
* Afterwards: `rm -rf monero` to get rid of the monero directory.



## Usage

* `vanity_address_generator` opens a command-line interface.
