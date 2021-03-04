ROOT = $(shell pwd)
STAGE = $(ROOT)/stage
RUNTIME = /usr/local/etc/sliderule
SLIDERULE = $(ROOT)/../sliderule
SERVER = $(ROOT)/build/server
PLUGIN = $(ROOT)/build/plugin

DOCKERTAG ?= icesat2sliderule/sliderule:latest

SERVERCFG := -DMAX_FREE_STACK_SIZE=1
SERVERCFG += -DUSE_AWS_PACKAGE=ON
SERVERCFG += -DUSE_H5_PACKAGE=ON
SERVERCFG += -DUSE_LEGACY_PACKAGE=OFF
SERVERCFG += -DUSE_CCSDS_PACKAGE=OFF

all: build


# Configuration Targets #

config: config-server config-plugin

config-server:
	mkdir -p $(SERVER)
	cd $(SERVER); cmake -DCMAKE_BUILD_TYPE=Debug $(SERVERCFG) $(SLIDERULE)

config-plugin:
	mkdir -p $(PLUGIN)
	cd $(PLUGIN); cmake -DCMAKE_BUILD_TYPE=Debug $(ROOT)


# Build Targets #

build: build-server build-plugin

build-server:
	make -j4 -C $(SERVER)

build-plugin:
	make -j4 -C $(PLUGIN)


# Install Targets #

install: install-server install-plugin

install-server:
	make -C $(SERVER) install

install-plugin:
	make -C $(PLUGIN) install
	cp config/asset_directory.csv $(RUNTIME)
	cp config/empty.index $(RUNTIME)
	cp config/plugins.conf $(RUNTIME)


# Run Targets #

run:
	sliderule apps/server.lua config/config.json


# Production Targets #

production-docker: distclean
	# build and install server into staging #
	mkdir -p $(SERVER)
	cd $(SERVER); cmake -DCMAKE_BUILD_TYPE=Release $(SERVERCFG) -DINSTALLDIR=$(STAGE) -DRUNTIMEDIR=$(RUNTIME) $(SLIDERULE)
	make -j4 $(SERVER)
	make -C $(SERVER) install
	# build and install plugin into staging #
	mkdir -p $(PLUGIN)
	cd $(PLUGIN); cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(STAGE) -DRUNTIMEDIR=$(RUNTIME) $(ROOT)
	make -j4 $(PLUGIN)
	make -C $(PLUGIN) install
	# copy over dockerfile #
	cp config/dockerfile.app $(STAGE)/Dockerfile
	# copy over installation configuration #
	cp config/plugins.conf $(STAGE)/etc/sliderule
	cp config/config.json $(STAGE)/etc/sliderule
	cp config/asset_directory.csv $(STAGE)/etc/sliderule
	cp config/empty.index $(STAGE)/etc/sliderule
	# copy over scripts #
	mkdir $(STAGE)/scripts
	cp -R $(SLIDERULE)/scripts/apps $(STAGE)/scripts/apps
	cp -R $(SLIDERULE)/scripts/tests $(STAGE)/scripts/tests
	cp config/docker-entrypoint.sh $(STAGE)/scripts/apps
	chmod +x $(STAGE)/scripts/apps/docker-entrypoint.sh
	# copy over plugin tests #
	mkdir -p $(STAGE)/plugins/icesat2/tests
	cp tests/* $(STAGE)/plugins/icesat2/tests
	# copy over plugin apps #
	mkdir -p $(STAGE)/plugins/icesat2/apps
	cp apps/* $(STAGE)/plugins/icesat2/apps
	# build image #
	cd $(STAGE); docker build -t $(DOCKERTAG) .

production-run:
	docker run -it --rm --name=sliderule-app -v /etc/ssl/certs:/etc/ssl/certs -v /data:/data -p 9081:9081 --entrypoint /usr/local/scripts/apps/docker-entrypoint.sh $(DOCKERTAG)


# Cleanup Targets #

clean:
	make -C build clean
	- rm aws_sdk_*.log

distclean:
	- rm -Rf $(SERVER)
	- rm -Rf $(PLUGIN)
	- rm -Rf $(STAGE)
