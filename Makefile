CC = g++
CXXFLAGS = -std=c++11
EPEE_DIR = monero/contrib/epee/include
MONERO_DIR = monero/build/release
MONERO_SRC = monero/src
MONERO_LIB = $(MONERO_DIR)/lib/libwallet_merged.a

BOOST_LIBS = -lboost_system -lboost_thread -lboost_filesystem -lboost_date_time -lboost_chrono

SOURCE_FILES = vanity_address_generator.cpp trim_account.cpp aeon-words.cpp

all:
	$(CC) $(CXXFLAGS) -I $(EPEE_DIR) -I $(MONERO_SRC) $(SOURCE_FILES) -pthread  -o vanity_address_generator $(MONERO_LIB) $(BOOST_LIBS)


.PHONY: all
