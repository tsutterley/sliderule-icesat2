#
# Imports
#
import sys
import json
import geopandas
from sliderule import icesat2

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Override region of interest
    regions = icesat2.toregion(sys.argv[1])

    # Override dataset
    dataset='ATL03'
    if len(sys.argv) > 2:
        dataset = sys.argv[2]

    # Query CMR for list of resources
    for i in range(len(regions)):
        region = regions[i]
        resources = icesat2.cmr(region, short_name=dataset)
        print("\nRegion:", i)
        for resource in resources:
            print(resource)
