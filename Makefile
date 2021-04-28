# example use:
#	$ make
#	$ sudo make install

ROOT = $(shell pwd)
RUNTIME = /usr/local/etc/sliderule
SLIDERULE = $(ROOT)/../sliderule
BUILD = $(ROOT)/build

CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(SLIDERULE)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-

all: icesat2

icesat2:
	make -j4 -C $(BUILD)

config:
	mkdir -p $(BUILD)
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)

install:
	make -C $(BUILD) install

scan:
	mkdir -p $(BUILD)
	cd $(BUILD); export CC=clang; export CXX=clang++; scan-build cmake $(CLANG_OPT) $(ROOT)
	cd $(BUILD); scan-build -o scan-results make

clean:
	make -C build clean

distclean:
	- rm -Rf $(BUILD)
