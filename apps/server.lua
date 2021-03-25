local global = require("global")
local asset = require("asset")
local json = require("json")

-- Process Arguments: JSON Configuration File
local cfgtbl = {}
local json_input = arg[1]
if json_input and string.match(json_input, ".json") then
    sys.log(core.CRITICAL, string.format('Reading json file: %s', json_input))
    local f = io.open(json_input, "r")
    local content = f:read("*all")
    f:close()
    cfgtbl = json.decode(content)
end

-- Pull Out Parameters --
local event_format = global.eval(cfgtbl["event_format"]) or core.FMT_TEXT
local event_level = global.eval(cfgtbl["event_level"]) or core.INFO
local port = cfgtbl["server_port"] or 9081
local asset_directory = cfgtbl["asset_directory"] or nil

-- Configure Monitoring --
sys.setlvl(core.LOG | core.TRACE | core.METRIC, event_level) -- set level globally
local monitor = core.monitor(core.LOG, core.INFO, event_format) -- monitor only logs
monitor:name("EventMonitor")
local dispatcher = core.dispatcher(core.MONITORQ)
dispatcher:name("EventDispatcher")
dispatcher:attach(monitor, "eventrec")
dispatcher:run()

-- Configure Assets --
assets = asset.loaddir(asset_directory, true)

-- Configure and Run Server --
server = core.httpd(port)
server:name("HttpServer")
endpoint = core.endpoint()
endpoint:name("LuaEndpoint")
server:attach(endpoint, "/source")
