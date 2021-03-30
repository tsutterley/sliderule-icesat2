SlideRule ICESat-2 Plugin
===========================

This SlideRule plugin includes algorithms for processing ICESat-2 data products.

If you are a science user interested in processing ICESat-2 data with SlideRule, please see the [sliderule-python client](https://github.com/ICESat2-SlideRule/sliderule-python).

## I. Prerequisites

1. The base build environment must be setup as described in the SlideRule readme.  Follow the first three steps in the Quick Start section to install the necessary system packages, a recent version of cmake, and Docker.  See [SlideRule Readme: III. Quick Start](https://github.com/ICESat2-SlideRule/sliderule/blob/master/README.md#iii-quick-start).  

2. Optionally install __Lttng__. See [Core Package Overview](https://github.com/ICESat2-SlideRule/sliderule/blob/master/packages/core/core.md) for installation instructions.

3. Install __AWS SDK__. See [Install AWS SDK Library](https://github.com/ICESat2-SlideRule/sliderule/blob/master/packages/aws/aws.md) for installation instructions.


## II. Building

Use the [Makefile](Makefile) to build and install the plugin.

For a development version of SlideRule that is run locally:
```bash
$ make config
$ make
$ sudo make install
$ make run
```

For a production version of SlideRule that is run in a docker container:
```bash
$ make production-docker
$ make production-run
```

#### Installing Docker

The official Docker installation instructions found at https://docs.docker.com/engine/install/ubuntu/.

For Ubuntu 20.04, Docker can be installed with the following commands:
```bash
$ sudo apt install docker.io
```

In order to run docker without having to be root, use the following commands:
```bash
$ sudo usermod -aG docker {username}
$ newgrp docker # apply group to user
```

#### Changing how SlideRule is run in a Docker container

```bash
$ docker run -it --rm --name=sliderule-app -v /data:/data -p 9081:9081 sliderule-application /usr/local/scripts/apps/server.lua {config.json}
```

The command above runs the server application inside the Docker container and can be configured in the following ways:
* A script other than `/usr/local/scripts/apps/server.lua` can be passed to the SlideRule executable running inside the Docker container
* The {config.json} file provided to the server.lua script can be used to change server settings
* Environment variables can be passed via `-e {parameter=value}` on the command line to docker
* Different local files and directories can be mapped in via `-v {source abs. path}:{destination abs. path}` on the command line to docker


## III. Setting Up A Python Environment

In order to use the ICESat-2 plugin it is highly recommended that the SlideRule Python Client package be installed in your python environment.  

See https://github.com/ICESat2-SlideRule/sliderule-python for more details.


## IV. What Is Provided

The plugin supplies the following endpoints:
* [atl06](endpoints/atl06.lua): process ATL03 photon data to produce gridded elevations
* [indexer](endpoints/idnexer.lua): process ATL03 resource and produce an index record (used with [build_indexes.py](utils/build_indexes.py))

This plugin supplies the following record types:
* `atl03rec`: a variable along-track extent of ATL03 photon data
* `atl03rec.photons`: individual ATL03 photons
* `atl06rec`: ATL06 algorithm results
* `atl06rec.elevation`: individual ATL06 elevations
* `atl03rec.index`: ATL03 meta data

The plugin supplies the following lua user data types:
* `icesat2.atl03(<url>, <outq_name>, [<parms>], [<track>])`: ATL03 reader base object
* `icesat2.atl03indexer(<asset>, <resource table>, <outq_name>, [<num threads>])`: ATL03 indexer base object
* `icesat2.atl06(<outq name>)`: ATL06 dispatch object
* `icesat2.ut_atl06()`: ATL06 dispatch unit test base object 

## V. Licensing

SlideRule is licensed under the 3-clause BSD license found in the LICENSE file at the root of this source tree.
