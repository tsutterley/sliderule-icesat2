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
local logfmt = global.eval(cfgtbl["logfmt"]) or core.FMT_TEXT
local loglvl = global.eval(cfgtbl["loglvl"]) or core.INFO
local port = cfgtbl["server_port"] or 9081
local asset_directory = cfgtbl["asset_directory"] or nil

-- Configure Logging --
sys.setlvl(core.LOG, loglvl)
local monitor = core.monitor(core.LOG, loglvl, logfmt)
monitor:name("LogMonitor")
local dispatcher = core.dispatcher(core.MONITORQ)
dispatcher:name("LogDispatcher")
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
