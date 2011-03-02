--_G.dprint = function(a,b,c) print("mooo") end
local servername = readConfig("servername");
local info = serverInfo();

local d = debug.getinfo(1);
local path = info.path .. "/" .. d.short_src:match("^(.-)%w+%.lua$");
local template = "";
local auth = {};

function getFile(filename)
    local f = io.open(path .. filename, "rb");
    local ret = f:read("*a");
    f:close();
    return ret;
end

-- character table string
local b='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'

-- encoding
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

-- decoding
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
    local ret = { URL = "/", headers = {}, form = {}};
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
    http = parseHTTP(session); -- Parse HTTP request.
    sess = session;
    session.credentials = nil;
    --[[ First, check authorization ]]--
    print(session.address);
    if (http.headers.Authorization) then
        local t,v = http.headers.Authorization:match("^(%w+) (.+)$");
        if (v) then
            local cred = dec(v);
            local user, pass = cred:match("^([^:]+):([^:]+)$");
            local pass_hash = SHA256(pass or "");
            for _user, cred in pairs(auth) do
                if (_user == user and pass_hash == cred.password) then session.credentials = cred; break; end
            end
        end
    end
    if (not session.credentials) then
        session:send("HTTP/1.1 401 Authorization Required\r\nWWW-Authenticate: Basic realm=\"RumbleLua\"\r\nContent-Type: text/html\r\n\r\nAuthorization required!\n");
        return;
    end
    
    --[[ Then, check if a specific file was requested rather than an action ]]--
    if ( http.URL:len() and not http.URL:match("auth%.cfg")) then 
        local info = fstat(path .. http.URL);
        if (info) then
            local f = io.open(path .. http.URL, "rb");
            output = f:read("*a");
            f:close();
            session:send("HTTP/1.1 200 OK\r\n");
            if ( http.URL:match("%.png")) then session:send("Content-Type: image/png\r\n");
            elseif ( http.URL:match("%.jpg")) then session:send("Content-Type: image/jpeg\r\n");
            elseif ( http.URL:match("%.css")) then session:send("Content-Type: text/css\r\n");
            else session:send("Content-Type: binary/octet-stream\r\n");
            end
            session:send("\r\n");
            session:send(output:len(), output);
            output = nil;
            return;
        end
    end
    
    --[[ Otherwise, start the action! ]]--
    session:send("HTTP/1.1 200 OK\r\n");
    session:send("Content-Type: text/html\r\n");
    session:send("\r\n");
    local output = template:gsub("%[%%header%%%]", ("RumbleLua on %s"):format(servername));
    output = output:gsub("%[%%title%%%]", "Main page");
    output = output:gsub("%[%%footer%%%]", ("Powered by Rumble Mail Server v/%s - %s"):format(info.version, os.date()));
    local section,subSection = http.URL:lower():match("^([^:]+):?(.-)$");
    
    session.contents = "";
    if (section == "domains") then
        if (subSection ~= "") then accountsPage(session,subSection);
        else domainPage(session);
        end
       
    else
        mainPage(session);
    end
    output = output:gsub("%[%%contents%%%]", session.contents);
    session:send(output);
    --[[
    session:send( string.format("Hello %s!<hr/><h2>Domains I host:</h2>", (http.form['smarmy'] and http.form['smarmy']) or session.address));
    for k,v in pairs(listDomains()) do
        local t = listAccounts(v);
        session:send(string.format("<h3>%s (%d accounts)</h3>", v, #t));
        
        for k, account in pairs(t) do
         --   session:send("- " .. account.name .. "@" .. account.domain .. "<br/>");
        end
    end
    session:send( string.format("<br/><small>Powered by RumbleLua on Rumble v/%s</small>", info.version));
    
    ]]--
end

function mainPage(session)
    info = serverInfo();
    append("<h2>Server information</h2>");
    append("<table class=\"elements\" border='0' cellspacing='1' cellpadding='5'>");
--    append("<tr><th>Domain</th></tr>");
    append("<tr><td>Version:</td><td>%s</td></tr>", info.version);
    append("<tr><td>Location:</td><td>%s</td></tr>", info.path);
    local hours = math.floor(info.uptime/3600);
    local minutes = math.floor(math.fmod(info.uptime, 3600)/60);
    local seconds = math.fmod(info.uptime, 60);
    append("<tr><td>Uptime:</td><td>%02u:%02u:%02u</td></tr>", hours,minutes,seconds);
    append("</table>");
end


function domainPage(session, domain)
append("<h2>Domains</h2>", domain);
    
    
    
    
    if (session.credentials.admin) then
    append("<table class=\"elements\" border='0' cellpadding='5' cellspacing='1'>");
    append("<tr><th>Create a new domain</th></tr>");
    
        if (http.form.domain) then
            if (http.form.delete) then
                if (deleteDomain(http.form.domain)) then
                    append("<tr><td><b><font color='red'>Deleted domain %s.</font></b></td></tr>", http.form.domain);
                else
                    append("<tr><td><b><font color='red'>Could not delete domain %s!</font></b></td></tr>", http.form.domain);
                end
            elseif (http.form.create) then
                createDomain(http.form.domain, http.form.path or "");
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
    local t = listDomains();
    table.sort(t);
    for k,v in pairs(t) do
        if (session.credentials.admin or session.credentials.domains[v]) then
            append("<tr><td><a href='/domains:%s'>%s</a></td><td>%s</td><td>[Edit] [<a href=\"/domains?domain=%s&delete=true\">Delete</a>]</td></tr>", v,v,v,v);
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
            local acc = readAccount(tonumber(http.form.user));
            if ( acc ) then
                deleteAccount(tonumber(http.form.user));
                append("<tr><td><b><font color='red'>Deleted account %s@%s.</font></b></td></tr>", acc.name, domain);
            end
            
        elseif (http.form.create) then
            if (accountExists(domain, http.form.user)) then
                append("<tr><td><b><font color='red'>Error: Account %s@%s already exists!</font></b></td></tr>", http.form.user, domain);
            else
                local acc = {};
                acc.name = http.form.user;
                acc.domain = domain;
                acc.type = http.form.type or "mbox";
                acc.password = SHA256(http.form.password or "");
                acc.arguments = http.form.arguments or "";
                saveAccount(acc);
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
    local t = listAccounts(domain);
    --table.sort(t);
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
    template = getFile("template.html");
    local config = getFile("auth.cfg");
    for user,pass,rights in config:gmatch("([^:\r\n]+):([^:\r\n]+):([^:\r\n]+)") do
        local domains = {};
        local admin = false;
        if ( rights == "*" ) then admin = true;
        else
            for domain in rights:gmatch("([^, ]+)") do domains[domain] = true; end
        end
        auth[user] = {password = pass, domains = domains, admin = admin};
    end
    print(string.format("%-48s[%s]", "Launching RumbleLua service on port 80...", (createService(acceptHTTP, 80, 5) and "OK") or "BAD"));
    
end


--[[
if (http.form.user) then
            if (http.form.delete) then
                print("Deleting account", http.form.user);
                deleteAccount(tonumber(http.form.user));
            else
                if (accountExists("gruno.dk", http.form.user)) then
                    append(session, "<li><font color='red'>Error: Account already exists!</font></li>");
                else
                    local acc = {};
                    acc.name = http.form.user;
                    acc.domain = "gruno.dk";
                    acc.type = "mbox";
                    acc.password = SHA256("smurf");
                    acc.arguments = "";
                    saveAccount(acc);
                    append(session, "<li><font color='blue'>Account saved!</font></li>");
                end
            end
        end
        
        t = listAccounts("gruno.dk");
        for k, account in pairs(t) do
             append(session, ("<li>%s@%s [<a href='/account?user=%u&delete=true'>delete</a>]</li>\n"):format(account.name, account.domain, account.id));
        end
        append(session,"</ul>");
        
        ]]--
        
