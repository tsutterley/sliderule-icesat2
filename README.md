# SlideRule ICESat-2 Plugin
[![DOI](https://zenodo.org/badge/344240956.svg)](https://zenodo.org/badge/latestdoi/344240956)

This SlideRule plugin includes algorithms for processing ICESat-2 data products.

If you are a science user interested in processing ICESat-2 data with SlideRule, please see the [sliderule-python client](https://github.com/ICESat2-SlideRule/sliderule-python).

## I. Prerequisites

1. The base build environment must be setup as described in the [SlideRule](https://github.com/ICESat2-SlideRule/sliderule) readme.  Follow the first three steps in the Quick Start section to install the necessary system packages.  See [SlideRule Readme: III. Quick Start](https://github.com/ICESat2-SlideRule/sliderule/blob/master/README.md#iii-quick-start).  

2. Install __AWS SDK__. See [Install AWS SDK Library](https://github.com/ICESat2-SlideRule/sliderule/blob/master/packages/aws/aws.md) for installation instructions.


## II. Building

Use the [Makefile](Makefile) to build and install the plugin.

For a development version of SlideRule that includes the plugin and is run locally:
```bash
$ make config
$ make
$ make install
```

## III. What Is Provided

The plugin supplies the following endpoints:
* [atl06](endpoints/atl06.lua): process ATL03 photon data segments to produce gridded elevations
* [atl03s](endpoints/atl03s.lua): create and return ATL03 photon data segments
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

## IV. Licensing

SlideRule is licensed under the 3-clause BSD license found in the LICENSE file at the root of this source tree.
