ROOT = $(shell pwd)
STAGE = stage
RUNTIME = /usr/local/etc/sliderule
SLIDERULE = $(ROOT)/../sliderule

DOCKERTAG ?= icesat2sliderule/sliderule:latest

ICESAT2CFG := -DMAX_FREE_STACK_SIZE=1
ICESAT2CFG += -DUSE_AWS_PACKAGE=ON
ICESAT2CFG += -DUSE_H5_PACKAGE=ON
ICESAT2CFG += -DUSE_ICESAT2_PLUGIN=ON
ICESAT2CFG += -DUSE_LEGACY_PACKAGE=OFF
ICESAT2CFG += -DUSE_CCSDS_PACKAGE=OFF
ICESAT2CFG += -DUSE_ATLAS_PLUGIN=OFF

all: build


# Configuration Targets #

config: server-config plugin-config

server-config:
	mkdir -p server-build
	cd server-build; cmake -DCMAKE_BUILD_TYPE=Debug $(ICESAT2CFG) $(SLIDERULE)

plugin-config:
	mkdir -p plugin-build
	cd plugin-build; cmake -DCMAKE_BUILD_TYPE=Debug ..


# Build Targets #

build: server-build plugin-build

server-build:
	make -j4 -C server-build

plugin-build:
	make -j4 -C plugin-build


# Install Targets #

install: server-install plugin-install

server-install:
	make -C server-build install

plugin-install:
	make -C build install
	cp asset_directory.csv $(RUNTIME)
	cp empty.index $(RUNTIME)

# Run Targets #

run:
	sliderule $(SLIDERULE)/plugins/icesat2/apps/server.lua config.json


# Production Targets #

production-docker: distclean production-server production-plugin
	# install sliderule #
	make -C build install
	# copy over dockerfile #
	cp config/dockerfile.app $(STAGE)/Dockerfile
	# copy over installation configuration #
	cp build/plugins.conf $(STAGE)/etc/sliderule
	cp config.json $(STAGE)/etc/sliderule
	cp asset_directory.csv $(STAGE)/etc/sliderule
	cp empty.index $(STAGE)/etc/sliderule
	# copy over scripts #
	mkdir $(STAGE)/scripts
	cp -R $(SLIDERULE)/scripts/apps $(STAGE)/scripts/apps
	cp -R $(SLIDERULE)/scripts/tests $(STAGE)/scripts/tests
	cp docker-entrypoint.sh $(STAGE)/scripts/apps
	chmod +x $(STAGE)/scripts/apps/docker-entrypoint.sh
	# copy over plugin tests #
	mkdir -p $(STAGE)/plugins/icesat2/tests
	cp $(SLIDERULE)/plugins/icesat2/tests/* $(STAGE)/plugins/icesat2/tests
	# copy over plugin apps #
	mkdir -p $(STAGE)/plugins/icesat2/apps
	cp $(SLIDERULE)/plugins/icesat2/apps/* $(STAGE)/plugins/icesat2/apps
	# build image #
	cd $(STAGE); docker build -t $(DOCKERTAG) .

production-server:
	mkdir -p server-build
	cd server-build; cmake -DCMAKE_BUILD_TYPE=Release $(ICESAT2CFG) -DINSTALLDIR=$(ROOT)/$(STAGE) -DRUNTIMEDIR=$(RUNTIME) $(SLIDERULE)
	make -j4 server-build

production-plugin:
	mkdir -p plugin-build
	cd plugin-build; cmake -DCMAKE_BUILD_TYPE=Release $(ICESAT2CFG) -DINSTALLDIR=$(ROOT)/$(STAGE) -DRUNTIMEDIR=$(RUNTIME) ..
	make -j4 plugin-build

production-run:
	docker run -it --rm --name=sliderule-app -v /etc/ssl/certs:/etc/ssl/certs -v /data:/data -p 9081:9081 --entrypoint /usr/local/scripts/apps/docker-entrypoint.sh $(DOCKERTAG)


# Cleanup Targets #

clean:
	make -C build clean
	- rm aws_sdk_*.log

distclean:
	- rm -Rf server-build
	- rm -Rf plugin-build
	- rm -Rf $(STAGE)
