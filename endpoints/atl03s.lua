--
-- ENDPOINT:    /source/atl03s
--
-- PURPOSE:     subset and return atl03 segments
--
-- INPUT:       rqst
--              {
--                  "atl03-asset":  "<name of asset to use, defaults to atlas-local>"
--                  "resource":     "<url of hdf5 file or object>"
--                  "track":        <track number: 1, 2, 3>
--                  "parms":        {<table of parameters>}
--                  "timeout":      <milliseconds to wait for first response>
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      atl03rec
--
-- NOTES:       1. The rqst is provided by arg[1] which is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing serialized 'atl03rec' and 'atl03rec.photons' RecordObjects
--

local json = require("json")
local asset = require("asset")

-- Create User Status --
local userlog = msg.publish(rspq)

-- Request Parameters --
local rqst = json.decode(arg[1])
local atl03_asset = rqst["atl03-asset"] or "atlas-s3"
local resource = rqst["resource"]
local track = rqst["track"] or icesat2.ALL_TRACKS
local parms = rqst["parms"]
local timeout = rqst["timeout"] or core.PEND

-- Send Data Directly to Response --
local recq = rspq 

-- Post Initial Status Progress --
userlog:sendlog(core.INFO, string.format("atl03 subsetting for %s ...", resource))

-- ATL03 Reader --
local resource_url = asset.buildurl(atl03_asset, resource)
atl03_reader = icesat2.atl03(resource_url, recq, parms, track)
atl03_reader:name("atl03_reader")

-- Wait Until Completion --
local duration = 0
local interval = 10000 -- 10 seconds
while not atl03_reader:waiton(interval) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout > 0 and duration == timeout then
        userlog:sendlog(core.INFO, string.format("request for %s timed-out after %d seconds", resource, duration / 1000))
        return
    end
    local atl03_stats = atl03_reader:stats(false)
    userlog:sendlog(core.INFO, string.format("read %d segments in %s (after %d seconds)", atl03_stats.read, resource, duration / 1000))
end

-- Processing Complete
userlog:sendlog(core.INFO, string.format("processing of %s complete", resource))
return
