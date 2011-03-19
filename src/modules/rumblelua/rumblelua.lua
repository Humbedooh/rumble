local portNum = 2580; -- Port number for the web interface

local fileCache = {};

-- function for retrieving files
function getFile(filename)
	local found = false;
	local ret = "";
	for k, entry in pairs(fileCache) do
		if ( entry.filename == filename ) then
			found = true;
			local fstat = file.stat(filename);
			if ( entry.modified ~= fstat.modified ) then
				local f = io.open(filename, "rb");
				if ( f ) then 
					entry.contents = f:read("*a");
					f:close();
				end
			end
			ret = entry.contents;
			break;
		end
	end
	if (not found) then
		local entry = { filename = filename };
		local fstat = file.stat(filename);
		entry.modified = fstat.modified;
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

function comma(number)
 local left,num,right = string.match(number,'^([^%d]*%d)(%d+)(.-)$');
local prettyNum = (left and left..(num:reverse():gsub('(%d%d%d)','%1,'):reverse()) or number);
return prettyNum;
end

-- character table string
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
function parseHTTP(session) 
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
    for k, v in string.gmatch(formdata, "([^&=]+)=([^&]+)") do
        v = v:gsub("+", " "):gsub("%%(%w%w)", function(s) return string.char(tonumber(s,16)) end);
       ret.form[k] = v;
    end
    
    return ret;
end

function append(...)
    sess.contents = sess.contents .. string.format(...);
end



function acceptHTTP(session)
    local servername = Rumble.readConfig("servername");
    session.info = Rumble.serverInfo();
    local d = debug.getinfo(1);
    session.path = session.info.path .. "/" .. d.short_src:match("^(.-)%w+%.lua$");
    local auth = {};
    
	--session:lock();
    session.output = getFile(session.path .. "/template.html");
    local config = getFile(session.path .. "/auth.cfg");
	--session:unlock();
	
    for user,pass,rights in config:gmatch("([^:\r\n]+):([^:\r\n]+):([^:\r\n]+)") do
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
    
    --[[ First, check authorization ]]--
    if (http.headers.Authorization) then
        local t,v = http.headers.Authorization:match("^(%w+) (.+)$");
        if (v) then
            local cred = dec(v);
            local user, pass = cred:match("^([^:]+):([^:]+)$");
            local pass_hash = (pass or ""):SHA256();
            for _user, cred in pairs(auth) do
                if (_user == user and pass_hash == cred.password) then session.credentials = cred; break; end
            end
        end
    end
    if (not session.credentials) then
        session:send("HTTP/1.1 401 Authorization Required\r\nWWW-Authenticate: Basic realm=\"RumbleLua\"\r\nContent-Type: text/html\r\n\r\nAuthorization required!\n");
        return;
    end
    session:send("HTTP/1.1 200 OK\r\n");
	session:send("Connection: close\r\n");
    --[[ Then, check if a specific file was requested rather than an action ]]--
    if ( http.URL:len() > 0 and not http.URL:match("auth%.cfg")) then
		--session:lock();
		local exists = file.exists(session.path .. http.URL);
		--session:unlock();
        if (exists) then
            output = getFile(session.path .. http.URL);
			
			if ( http.URL:match("%.png")) then session:send("Content-Type: image/png\r\n");
			elseif ( http.URL:match("%.jpg")) then session:send("Content-Type: image/jpeg\r\n");
			elseif ( http.URL:match("%.css")) then session:send("Content-Type: text/css\r\n");
			else session:send("Content-Type: binary/octet-stream\r\n");
			end
			session:send("\r\n");
			if (output) then session:send(output:len(), output); end
			output = nil;
			return;
		end
     end
    
    --[[ Otherwise, start the action! ]]--
    session:send("Content-Type: text/html\r\n");
    session:send("\r\n");
    session.output = session.output:gsub("%[%%header%%%]", ("RumbleLua on %s"):format(servername));
    session.output = session.output:gsub("%[%%title%%%]", "Main page");
	session.output = session.output:gsub("%[%%version%%%]", session.info.version);
    session.output = session.output:gsub("%[%%footer%%%]", ("Powered by Rumble Mail Server v/%s - %s"):format(session.info.version, os.date()));
    local section,subSection = http.URL:lower():match("^([^:]+):?(.-)$");
    
    session.contents = "";
    if (section == "domains") then
        if (subSection ~= "") then accountsPage(session,subSection);
        else domainPage(session);
        end
       
    else
        mainPage(session);
    end
    session.output = session.output:gsub("%[%%contents%%%]", session.contents);
    session:send(session.output);
	
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


function parseLua(data)
    local prev = _LUA_TMP;
    local _print, _printf = print, printf;
    print = lprint;
    printf = lprintf;
    _LUA_TMP = "";
    for snippet in data:gmatch("%b<>") do
        if (snippet:sub(1,5) == "<?lua") then
            local code = snippet:sub(6,snippet:len()-2);
            _print(code);
        end
    end
    print = _print;
    printf = _printf;
    _LUA_TMP = prev;
end


function mainPage(session)
    append("<h2>Server information</h2>");
    append("<table class=\"elements\" border='0' cellspacing='1' cellpadding='5'>");
    append("<tr><td>Version:</td><td class='plain_text'>%s</td></tr>", session.info.version);
	append("<tr><td>System:</td><td class='plain_text'>%s (%s)</td></tr>", session.info.os, (session.info.arch == 32 and "x86") or "x64");
    append("<tr><td>Location:</td><td class='plain_text'>%s</td></tr>", session.info.path);
    local hours = math.floor(session.info.uptime/3600);
    local minutes = math.floor(math.fmod(session.info.uptime, 3600)/60);
    local seconds = math.fmod(session.info.uptime, 60);
    append("<tr><td>Uptime:</td><td class='plain_text'>%02u:%02u:%02u</td></tr>", hours,minutes,seconds);
    append("</table>");
    
    append("<br/><br/><h2>Services</h2>");
    for k,v in pairs({"smtp", "imap", "pop3", "core"}) do
        local svc = Rumble.serviceInfo(v);
        if (svc) then
            append("<table class=\"elements\" border='0' cellspacing='1' cellpadding='5'>");
            append("<tr><th colspan='2'>%s</th></tr>", v:upper());
            if (svc.enabled) then 
                append("<tr><td>Status:</td><td class='plain_text'><font color='darkgreen'>Enabled</font></td></tr>");
                append("<tr><td>Threads:</td><td class='plain_text'>%u total, %u working, %u idling.</td></tr>", svc.workers, svc.busy, svc.idle);
                append("<tr><td>Traffic:</td><td class='plain_text'>%s sessions, %s bytes received, %s bytes sent</td></tr>", comma(svc.sessions), comma(svc.received), comma(svc.sent));
				if (svc.capabilities:len() > 0) then
					append("<tr><td>Capabilities:</td><td class='plain_text'>%s</td></tr>", svc.capabilities);
				end	
            else 
                append("<tr><td>Status:</td><td class='plain_text'><font color='darkred'>Disabled</font></td></tr>");
            end
            append("</table>");
        end
    end
    
    append("<br/><br/><h2>Modules</h2>");
    for k,mod in pairs(Rumble.listModules()) do
        local file = mod.file;
        if not (file:match("^/") or file:match(":")) then
            file = session.info.path .. "/" ..file;
        end
        append("<table class=\"elements\" border='0' cellspacing='1' cellpadding='5'>");
        append("<tr><th colspan='2'>%s</th></tr>", mod.title);
        append("<tr><td>Description:</td><td class='plain_text'>%s</td></tr>", mod.description);
        if (mod.author ~= "Unknown") then append("<tr><td>Author:</td><td class='plain_text'>%s</td></tr>", mod.author); end
        append("<tr><td>File:</td><td class='plain_text'>%s</td></tr>", file);
        append("</table>");
    end
end


function domainPage(session, domain)
append("<h2>Domains</h2>", domain);
    
    
    
    
    if (session.credentials.admin) then
    append("<table class=\"elements\" border='0' cellpadding='5' cellspacing='1'>");
    append("<tr><th>Create a new domain</th></tr>");
    
        if (http.form.domain) then
            if (http.form.delete) then
                if (Mailman.deleteDomain(http.form.domain)) then
                    append("<tr><td><b><font color='red'>Deleted domain %s.</font></b></td></tr>", http.form.domain);
                else
                    append("<tr><td><b><font color='red'>Could not delete domain %s!</font></b></td></tr>", http.form.domain);
                end
            elseif (http.form.create) then
                Mailman.createDomain(http.form.domain, http.form.path or "");
                append("<tr><td><b><font color='darkgreen'>Domain %s has been created.</font></b></td></tr>", http.form.domain);
            end
        end
        
        append("<tr><td>");
        append([[
        <form action="/domains" method="post" id='create'>
        
        <div class='form_el' style='display: block;' id='domainname'>
            <div class='form_key'>
                Domain name:
            </div>
            <div class='form_value'>
                <input type="text" name="domain"/>
            </div>
        </div>
        
        <div class='form_el' style='display: block;' id='domainname'>
            <div class='form_key'>
                Optional alt. storage path:
            </div>
            <div class='form_value'>
                <input type="text" name="path"/>
            </div>
        </div>
        
        <input type="hidden" name="create" value="true"/>
        <div class='form_el' id='domainsave' >
            <input type="submit" value="Save domain"/>
        </div>
        </form>
        ]]);
        append("</td></tr>");
        
        
        append("</table>");
    end
    
    append("<table class=\"elements\" border='0' cellpadding='5' cellspacing='1'>");
    append("<tr><th>Domain</th><th>Type</th><th>Actions</th></tr>");
    local t = Mailman.listDomains(); 
    table.sort(t);
    for k,v in pairs(t) do
        if (session.credentials.admin or session.credentials.domains[v]) then
            append("<tr><td><img src='/bullet.png' align='absmiddle'/><a href='/domains:%s'>%s</a></td><td>domain</td><td>[Edit] [<a href=\"/domains?domain=%s&delete=true\">Delete</a>]</td></tr>", v,v,v);
        end
    end
    if (#t == 0) then
        append("<tr><td colspan='3'><i>No domains are configured for this server yet.</i></td></tr>");
    end
    append("</table>");
end


function accountsPage(session, domain)
    append("<h2>Accounts @ %s</h2>", domain);
    
    append("<table class=\"elements\" border='0' cellpadding='5' cellspacing='1'>");
    append("<tr><th>Create a new account</th></tr>");
    
    
    
    if (http.form.user) then
        if (http.form.delete) then
            local acc = Mailman.readAccount(tonumber(http.form.user));
            if ( acc ) then
                Mailman.deleteAccount(tonumber(http.form.user));
                append("<tr><td><b><font color='red'>Deleted account %s@%s.</font></b></td></tr>", acc.name, domain);
            end
            
        elseif (http.form.create) then
            if (Mailman.accountExists(domain, http.form.user)) then
                append("<tr><td><b><font color='red'>Error: Account %s@%s already exists!</font></b></td></tr>", http.form.user, domain);
            else
                local acc = {};
                acc.name = http.form.user;
                acc.domain = domain;
                acc.type = http.form.type or "mbox";
                acc.password = SHA256(http.form.password or "");
                acc.arguments = http.form.arguments or "";
                Mailman.saveAccount(acc);
                append("<tr><td><b><font color='darkgreen'>Account %s@%s has been created.</font></b></td></tr>", http.form.user, domain);
            end
        end
    end
    
    append("<tr><td>");
    append([[
    
    <form action="/domains:%s" method="post" id='create'>
    <div>
    <div class='form_el'>
        <div class='form_key'>
            Account type:
        </div>
        <div class='form_value'>
            <select name="type" 
            onChange="
            document.getElementById('accname').style.display = 'inherit';
            var type = this.options[this.selectedIndex].value;
            document.getElementById('accsave').style.display = 'inherit';
            if (type == '') {
                document.getElementById('accarg').style.display = 'none';
                document.getElementById('accname').style.display = 'none';
                document.getElementById('accpass').style.display = 'none';
                document.getElementById('accsave').style.display = 'none';
                }
            else if (type == 'mbox') { 
                document.getElementById('accpass').style.display = 'inherit';
                document.getElementById('accarg').style.display = 'none';
            }
            else {
                document.getElementById('accpass').style.display = 'none';
                document.getElementById('accarg').style.display = 'inherit';
                }
            "
            >
            <option value="">Select...</option>
            <option value="mbox">Mailbox</option>
            <option value="alias">Alias</option>
            <option value="mod">Feed to module...</option>
            <option value="feed">Feed to program...</option>
            <option value="relay">Relay to other server</option>
            </select>
        </div>
    </div>
    <div class='form_el' style='display: none;' id='accname'>
        <div class='form_key'>
            Account name:
        </div>
        <div class='form_value'>
            <input type="text" name="user"/> <i>@%s</i>
        </div>
    </div>
    
    <div class='form_el' style="display: none" id='accpass';>
        <div class='form_key'>
            Password:
        </div>
        <div class='form_value'>
            <input type="password" name="password"/>
        </div>
    </div>
    <div class='form_el' id='accarg' style='display: none;' >
        <div class='form_key'>
            Arguments: 
        </div>
        <div class='form_value'>
            <input type="text" name="arguments"/>
        </div>
    </div>
    <input type="hidden" name="create" value="true"/>
    <div class='form_el' id='accsave' style='display: none;' >
            <input type="submit" value="Save account"/>
    </div>
    </div>
</form>
    ]], domain,domain);
    append("</td></tr>");
    
    
    append("</table>");
    
    append("<table class=\"elements\" border='0' cellpadding='5' cellspacing='1'>");
    append("<tr><th>Account</th><th>Type</th><th>Actions</th></tr>");
    local t = Mailman.listAccounts(domain);
    for k,v in pairs(t) do
        append("<tr><td><a href='/accounts:%u'>%s@%s</a></td><td>%s</td><td>[Edit] [<a href=\"/domains:%s?user=%u&delete=true\">Delete</a>]</td></tr>", v.id,v.name,domain,v.type,domain,v.id);
    end
    if (#t == 0) then
        append("<tr><td colspan='3'><i>%s does not have any mail accounts.</i></td></tr>", domain);
    end
    append("</table>");
end

--[[ Initialize the service ]]--

do
    if (Rumble.createService(acceptHTTP, portNum, 10) == true) then
		print(string.format("%-48s[%s]", "Launching RumbleLua service on port " .. portNum .. "...", "OK"));
	end
    
end

