local portNum = 2580; -- Port number for the web interface

local fileCache = {};

-- getFile(): Retrieves and caches files.
function getFile(filename)
    local found = false;
    local ret = "";
    for k, entry in pairs(fileCache) do
        if ( entry.filename == filename ) then
            local fstat = file.stat(filename);
            if (fstat) then
                found = true;
                if ( entry.modified ~= fstat.modified ) then
                    local f = io.open(filename, "rb");
                    if ( f ) then
                        entry.contents = f:read("*a");
                        f:close();
                    end
                end
                ret = entry.contents;
            else
                ret = "";
            end
            break;
        end
    end
    if (not found) then
        local entry = { filename = filename };
        local fstat = file.stat(filename);
        if (fstat) then
            entry.modified = fstat.modified;
        end
        entry.contents = "";
        local f = io.open(filename, "rb");
        if (f) then
            entry.contents = f:read("*a");
            f:close();
        end
        ret = entry.contents;
        table.insert(fileCache, entry);
    end

    return ret;
end

-- math.comma(): Turns 12345678 into 12,345,678 and so on.
function math.comma(number)
    local left,num,right = string.match(number,'^([^%d]*%d)(%d+)(.-)$');
    local prettyNum = (left and left..(num:reverse():gsub('(%d%d%d)','%1,'):reverse()) or number);
    return prettyNum;
end


-- character table string for base 64 stuff
local b='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'

-- base64 encoding
function enc(data)
    return ((data:gsub('.', function(x)
        local r,b='',x:byte()
        for i=8,1,-1 do r=r..(b%2^i-b%2^(i-1)>0 and '1' or '0') end
        return r;
    end)..'0000'):gsub('%d%d%d?%d?%d?%d?', function(x)
        if (#x < 6) then return '' end
        local c=0
        for i=1,6 do c=c+(x:sub(i,i)=='1' and 2^(6-i) or 0) end
        return b:sub(c+1,c+1)
    end)..({ '', '==', '=' })[#data%3+1])
end

-- base64 decoding
function dec(data)
    data = string.gsub(data, '[^'..b..'=]', '')
    return (data:gsub('.', function(x)
        if (x == '=') then return '' end
        local r,f='',(b:find(x)-1)
        for i=6,1,-1 do r=r..(f%2^i-f%2^(i-1)>0 and '1' or '0') end
        return r;
    end):gsub('%d%d%d?%d?%d?%d?%d?%d?', function(x)
        if (#x ~= 8) then return '' end
        local c=0
        for i=1,8 do c=c+(x:sub(i,i)=='1' and 2^(8-i) or 0) end
        return string.char(c)
    end))
end


--[[ parseHTTP: Parses a HTTP request into URL, headers and form data ]] --
function _G.parseHTTP(session)
    local ret = { URL = "", headers = {}, form = {}};
    local formdata = "";

    --[[  Read request headers  ]]--
    while (true) do
        line = session:receive();
        local key, value = line:match("([%w%-]+):? (.*)");
        if (line:len() == 0) then break; end
        ret.headers[key] = value;
    end

    --[[  Read GET or POST data  ]]--
    if (ret.headers["Content-Length"]) then
        value = tonumber(ret.headers["Content-Length"]);
        formdata = session:receivebytes(value);
    end
    if (ret.headers['GET']) then
        ret.URL, formdata = string.match(ret.headers['GET'], "/([^%? ]*)%??(%S-) ");
    elseif (ret.headers['POST']) then
        ret.URL = string.match(ret.headers['POST'], "/([^%? ]*)");
    end
    ret.URL = ret.URL:gsub("%.%.", "");

    --[[  Parse form data  ]]--
	local noDoS = 0;
    for k, v in string.gmatch(formdata, "([^&=]+)=([^&]+)") do
		noDoS = noDoS + 1;
		if (noDoS > 256) then break; end -- hashing DoS attack ;O
        v = v:gsub("+", " "):gsub("%%(%w%w)", function(s) return string.char(tonumber(s,16)) end);
        if (not ret.form[k]) then
            ret.form[k] = v;
        else
            if ( type(ret.form[k]) == "string") then
                local tmp = ret.form[k];
                ret.form[k] = {tmp};
            end
            table.insert(ret.form[k], v);
        end
    end

    return ret;
end

function _G.append(...) -- This is the printf() function for the <? code ?> snippets inside the PHTML files.
    sess.contents = sess.contents .. string.format(...);
end

function _G.echo(msg)
	sess.contents = sess.contents .. (msg or "");
end


-- acceptHTTP: The big service handling function.
function acceptHTTP(session)
    local rnd = math.random(os.time());

    local servername = Rumble.readConfig("servername");
    session.info = Rumble.serverInfo();
    local d = debug.getinfo(1);
    session.path = session.info.path .. "/" .. d.short_src:match("^(.-)%w+%.lua$");
    local auth = {};
    local firstVisit = true;

    --session:lock();
    session.output = getFile(session.path .. "/template.html");
    local config = getFile(session.path .. "/auth.cfg");
    --session:unlock();

    for user,pass,rights in config:gmatch("([^:\r\n]+):([^:\r\n]+):([^:\r\n]+)") do
        firstVisit = false;
        local domains = {};
        local admin = false;
        if ( rights == "*" ) then admin = true;
        else
            for domain in rights:gmatch("([^, ]+)") do domains[domain] = true; end
        end
        auth[user] = {password = pass, domains = domains, admin = admin};
    end

    http = parseHTTP(session); -- Parse HTTP request.
    sess = session;
    session.credentials = nil;
    session.http = http;
    session.auth = auth;

    --[[ First, check authorization ]]--
    if (http.headers.Authorization) then
        local t,v = http.headers.Authorization:match("^(%w+) (.+)$");
        if (v) then
            local cred = dec(v);
            local user, pass = cred:match("^([^:]+):([^:]+)$");
            local pass_hash = (pass or ""):SHA256();
            for _user, cred in pairs(auth) do
                if (_user == user and pass_hash == cred.password) then
                    session.credentials = cred;
                    session.credentials.user = _user;
                    break;
                end
            end
        end
    end

    if (firstVisit and http.URL == "") then
        session:send("HTTP/1.1 302 Moved\r\n");
        session:send("Location: /welcome\r\n\r\n");
        return;
    elseif (not firstVisit) then
        if (not session.credentials) then
            session:send("HTTP/1.1 401 Authorization Required\r\nWWW-Authenticate: Basic realm=\"RumbleLua\"\r\nContent-Type: text/html\r\n\r\nAuthorization required!\n");
            return;
        end
    end

    --[[ Then, check if a specific file was requested rather than an action ]]--
    if ( not http.URL:match("auth%.cfg")) then
        --session:lock();
        if (http.URL:len() == 0) then http.URL = "index"; end
        local exists = file.exists(session.path .. http.URL);
        --session:unlock();
        if (exists) then
            session:send("HTTP/1.1 200 OK\r\n");
            session:send("Connection: close\r\n");
            output = getFile(session.path .. http.URL);

            if ( http.URL:match("%.png")) then session:send("Content-Type: image/png\r\n");
            elseif ( http.URL:match("%.jpg")) then session:send("Content-Type: image/jpeg\r\n");
			elseif ( http.URL:match("%.svg")) then session:send("Content-Type: image/svg+xml\r\n");
            elseif ( http.URL:match("%.css")) then session:send("Content-Type: text/css\r\n");
            else session:send("Content-Type: binary/octet-stream\r\n");
            end
            session:send("\r\n");
            if (output) then session:send(output:len(), output); end
            output = nil;
            return;
        else
            local section,subSection = http.URL:lower():match("^([^:]+):?(.-)$");
            session.section = subSection;
            local scriptFile = session.path .. "/scripts/" .. section .. ".phtml";
            if ( http.URL == "" ) then scriptFile = session.path .. "/scripts/index.phtml"; end
            if (file.exists(scriptFile) and not (firstVisit and http.URL ~= "welcome")) then
                session.script = getFile(scriptFile);
            else
                session:send("HTTP/1.1 302 Moved\r\n");
                session:send("Location: /\r\n\r\n");
                return;
            end


            session.pos = "<!-- -->"
            session.atend = nil;
            _G.my = {};
            _G.rnd = rnd;
            local Script = " <? local SC_START; ?> " .. session.script .. "<? local SC_END; ?>\n";
			_G.my = {};
			_G.rnd = rnd;
			_G.printf = append;
			session.script = Script:gsub("(.-)<%?(.-)%?>",
				function(z,x)
					_G.session = session;
					if (string.sub(x, 1, 1) == "=") then
						x = x:sub(2);
						x = "echo("..x..");"
					end
					return string.format("echo([=[%s]=]);\n %s", z, x);
				end);

			session.contents = "";
			loadstring(session.script)();
        end

     end

    --[[ Add the remaining stuff and send the page ]]--
    if (not session.killed) then
        session:send("HTTP/1.1 200 OK\r\n");
        session:send("Connection: close\r\n");
        session:send("Content-Type: text/html\r\n");
        session:send("\r\n");
        session.output = session.output:gsub("%[%%header%%%]", ("RumbleLua on %s"):format(servername));
        session.output = session.output:gsub("%[%%title%%%]", "Main page");
        session.output = session.output:gsub("%[%%version%%%]", session.info.version);
        session.output = session.output:gsub("%[%%footer%%%]", ("Powered by Rumble Mail Server v/%s - %s"):format(session.info.version, os.date()));
        session.output = session.output:gsub("%[%%contents%%%]", session.contents);
        _G.my = {};
        session.output = session.output:gsub("<%?(.-)%?>",
            function(x)
                _G.session = session;
                local output = "";
                local _printf = printf;
                local xit = _G.exit;
				if (string.sub(x, 1, 1) == "=") then
					x = x:sub(2);
					local ret, val = pcall(loadstring("return ("..x..")")); return val or x;
				end
                _G.printf = function(...) output = output .. string.format(...); end;
                if (not session.stop) then
                    loadstring(x)();
                end
                _G.printf = _printf;
                _G.exit = xit;
                return output;
            end);
        if (session.atend) then session.script = session.output:sub(1,session.atend); end
        session:send(session.output);
    end
    session.credentials = nil;
    auth = nil;
    session = {};
    output = "";
    http.headers = nil;
    http = nil;
    _LUA_TMP = "";
    collectgarbage();
end

_LUA_TMP = "";

function lprintf(...)
    _LUA_TMP = _LUA_TMP .. string.format(...);
end
function lprint(...)
    _LUA_TMP = _LUA_TMP .. string.format(...);
end

--[[ Initialize the service ]]--

do
    if (Rumble.createService(acceptHTTP, portNum, 10) == true) then
        print(string.format("%-48s[%s]", "Launching RumbleLua service on port " .. portNum .. "...", "OK"));
    end

end

