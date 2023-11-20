--Cracker64's Autorun Script Manager
--The autorun to end all autoruns
--Version 3.12

--TODO:
--manual file addition (that can be anywhere and any extension)
--Moving window (because why not)
--some more API functions
--prettier, organize code

--CHANGES:
--Version 3.12: Use https for all requests, online view loads async, add FILTER button to online, fix rare failure on startup if downloaded scripts list is corrupted
--Version 3.11: Fix icons in 94.0, fix "view script in browser"
--Version 3.10: Fix HTTP requests, without this update the online section may break
--Version 3.9: Minor icon fix for latest version of jacob1's mod
--Version 3.8: Fix being unable to download scripts with / in the name, make sure tooltips don't go offscreen
--Version 3.7: Account for extra menu in TPT 91.4
--Version 3.6: Fix bug where it might delete your scripts after updating on windows
--Version 3.5: Lua5.2 support, TPT 91.0 platform API support, [] can be used to scroll, misc fixes
--Version 3.4: some new buttons, better tooltips, fix 'Change dir' button, fix broken buttons on OS X
--Version 3.3: fix apostophes in filenames, allow authors to rename their scripts on the server
--Version 3.2: put MANAGER stuff in table, fix displaying changelogs
--Version 3.1: Organize scripts less randomly, fix scripts being run twice, fix other bugs
--central script / update server at starcatcher.us / delete local scripts / lots of other things by jacob1 v3.0
--Scan all subdirectories in scripts folder! v2.25
--Remove step hooks, v87 fixes them
--Socket is now default in v87+ , horray, everyone can now use update features without extra downloads.
--Handles up to 50 extra step functions, up from the default 5 (not including the manager's step) v2.1
--Other various nice API functions
--Scripts can store/retrieve settings through the manager, see comments below v2.0
--Small fillrect change for v85, boxes can have backgrounds v1.92
--Support spaces in exe names v1.91
--Auto-update for OTHER scripts now works, is a bit messy, will fix later, but nothing should change for users to use this
--  Place a line '--VER num UPDATE link' in one of the first four lines of the file, see my above example
--  The link at top downloads a file that contains ONLY version,full link,and prints the rest(changelog). See my link for example

local jacobsmod = tpt.version.jacob1s_mod

local icons = {
	["delete1"] = "\xEE\x80\x85",
	["delete2"] = "\xEE\x80\x86",
	["folder"] = "\xEE\x80\x93"
}
if jacobsmod then
	icons = {
		["delete1"] = "\133",
		["delete2"] = "\134",
		["folder"] = "\147"
	}
end


if not socket then error("不支持此TPT版本") end
if MANAGER then error("管理器已经在运行") end

local scriptversion = 14
MANAGER = {["version"] = "3.12", ["scriptversion"] = scriptversion, ["hidden"] = true}

local type = type -- people like to overwrite this function with a global a lot
local TPT_LUA_PATH = 'scripts'
local PATH_SEP = '\\'
local OS = "WIN32"
local CHECKUPDATE = false
local EXE_NAME
if platform then
	OS = platform.platform()
	if OS ~= "WIN32" and OS ~= "WIN64" then
		PATH_SEP = '/'
	end
	EXE_NAME = platform.exeName()
	local temp = EXE_NAME:reverse():find(PATH_SEP)
	EXE_NAME = EXE_NAME:sub(#EXE_NAME-temp+2)
else
	if os.getenv('HOME') then
		PATH_SEP = '/'
		if fs.exists("/Applications") then
			OS = "MACOSX"
		else
			OS = "LIN64"
		end
	end
	if OS == "WIN32" or OS == "WIN64" then
		EXE_NAME = jacobsmod and "Jacob1\'s Mod.exe" or "Powder.exe"
	elseif OS == "MACOSX" then
		EXE_NAME = "powder-x" --can't restart on OS X (if using < 91.0)
	else
		EXE_NAME = jacobsmod and "Jacob1\'s Mod" or "powder"
	end
end
local filenames = {}
local num_files = 0 --downloaded scripts aren't stored in filenames
local localscripts = {}
local onlinescripts = {}
local running = {}
local requiresrestart=false
local online = false
local first_online = true
local online_req = nil
local script_manager_update_req = nil
local updatetable --temporarily holds info on script manager updates
local gen_buttons
local check_req_status
local sidebutton
local download_file
local settings = {}
local search_terms = {}
math.randomseed(os.time()) math.random() math.random() math.random() --some filler randoms

--get line that can be saved into scriptinfo file
local function scriptInfoString(info)
	--Write table into data format
	if type(info)~="table" then return end
	local t = {}
	for k,v in pairs(info) do
		table.insert(t,k..":\""..v.."\"")
	end
	local rstr = table.concat(t,","):gsub("\r",""):gsub("\n","\\n")
	return rstr
end

--read a scriptinfo line
local function readScriptInfo(list)
	if not list then return {} end
	local scriptlist = {}
	for i in list:gmatch("[^\n]+") do
		local t = {}
		local ID = 0
		for k,v in i:gmatch("(%w+):\"([^\"]*)\"") do
			t[k]= tonumber(v) or v:gsub("\r",""):gsub("\\n","\n")
		end
		if not t.ID then
			print("跳过脚本列表中的无效脚本")
		else
			scriptlist[t.ID] = t
		end
	end
	return scriptlist
end

--save settings
local function save_last()
	local savestring=""
	for script,v in pairs(running) do
		savestring = savestring.." \""..script.."\""
	end
	savestring = "SAV "..savestring.."\nDIR "..TPT_LUA_PATH
	for k,t in pairs(settings) do
	for n,v in pairs(t) do
		savestring = savestring.."\nSET "..k.." "..n..":\""..v.."\""
	end
	end
	local f
	if TPT_LUA_PATH == "scripts" then
		f = io.open(TPT_LUA_PATH..PATH_SEP.."autorunsettings.txt", "w")
	else
		f = io.open("autorunsettings.txt", "w")
	end
	if f then
		f:write(savestring)
		f:close()
	end

	f = io.open(TPT_LUA_PATH..PATH_SEP.."downloaded"..PATH_SEP.."scriptinfo", "w")
	if f then
		for k,v in pairs(localscripts) do
			f:write(scriptInfoString(v).."\n")
		end
		f:close()
	end
end

local function load_downloaded()
	local f = io.open(TPT_LUA_PATH..PATH_SEP.."downloaded"..PATH_SEP.."scriptinfo","r")
	if f then
		local lines = f:read("*a")
		f:close()
		localscripts = readScriptInfo(lines)
		for k,v in pairs(localscripts) do
			if k ~= 1 then
				if not v["ID"] or not v["name"] or not v["description"] or not v["path"] or not v["version"] then
					localscripts[k] = nil
				elseif not fs.exists(TPT_LUA_PATH.."/"..v["path"]:gsub("\\","/")) then
					 localscripts[k] = nil
				end
			end
		end
	end
end

--load settings before anything else
local function load_last()
	local f = io.open(TPT_LUA_PATH..PATH_SEP.."autorunsettings.txt","r")
	if not f then
		f = io.open("autorunsettings.txt","r")
	end
	if f then
		local lines = {}
		local line = f:read("*l")
		while line do
			table.insert(lines,(line:gsub("\r","")))
			line = f:read("*l")
		end
		f:close()
		for i=1, #lines do
			local tok=lines[i]:sub(1,3)
			local str=lines[i]:sub(5)
			if tok=="SAV" then
				for word in string.gmatch(str, "\"(.-)\"") do running[word] = true end
			elseif tok=="EXE" then
				EXE_NAME=str
			elseif tok=="DIR" then
				TPT_LUA_PATH=str
			elseif tok=="SET" then
				local ident,name,val = string.match(str,"(.-) (.-):\"(.-)\"")
				if settings[ident] then settings[ident][name]=val
				else settings[ident]={[name]=val} end
			end
		end
	end

	load_downloaded()
end
load_last()
--get list of files in scripts folder
local function load_filenames()
	filenames = {}
	local function searchRecursive(directory)
		local dirlist = fs.list(directory)
		if not dirlist then return end
		for i,v in ipairs(dirlist) do
			local file = directory.."/"..v
			if fs.isDirectory(file) and v ~= "downloaded" then
				searchRecursive(file)
			elseif fs.isFile(file) then
				if file:find("%.lua$") then
					local toinsert = file:sub(#TPT_LUA_PATH+2)
					if OS == "WIN32" or OS == "WIN64" then
						toinsert = toinsert:gsub("/", "\\") --not actually required
					end
					table.insert(filenames, toinsert)
				end
			end
		end
	end
	searchRecursive(TPT_LUA_PATH)
	table.sort(filenames, function(first,second) return first:lower() < second:lower() end)
end
--ui object stuff
local ui_base local ui_box local ui_line local ui_text local ui_button local ui_scrollbar local ui_tooltip local ui_checkbox local ui_console local ui_window
local tooltip
ui_base = {
new = function()
	local b={}
	b.drawlist = {}
	function b:drawadd(f)
		table.insert(self.drawlist,f)
	end
	function b:draw(...)
		for _,f in ipairs(self.drawlist) do
			if type(f)=="function" then
				f(self,...)
			end
		end
	end
	b.movelist = {}
	function b:moveadd(f)
		table.insert(self.movelist,f)
	end
	function b:onmove(x,y)
		for _,f in ipairs(self.movelist) do
			if type(f)=="function" then
				f(self,x,y)
			end
		end
	end
	return b
end
}
ui_box = {
new = function(x,y,w,h,r,g,b)
	local box=ui_base.new()
	box.x=x box.y=y box.w=w box.h=h box.x2=x+w box.y2=y+h
	box.r=r or 255 box.g=g or 255 box.b=b or 255
	function box:setcolor(r,g,b) self.r=r self.g=g self.b=b end
	function box:setbackground(r,g,b,a) self.br=r self.bg=g self.bb=b self.ba=a end
	box.drawbox=true
	box.drawbackground=false
	box:drawadd(function(self) if self.drawbackground then tpt.fillrect(self.x,self.y,self.w+1,self.h+1,self.br,self.bg,self.bb,self.ba) end
								if self.drawbox then tpt.drawrect(self.x,self.y,self.w,self.h,self.r,self.g,self.b) end end)
	box:moveadd(function(self,x,y)
		if x then self.x=self.x+x self.x2=self.x2+x end
		if y then self.y=self.y+y self.y2=self.y2+y end
	end)
	return box
end
}
ui_line = {
new=function(x,y,x2,y2,r,g,b)
	local line=ui_box.new(x,y,x2-x,y2-y,r,g,b)
	--Line is essentially a box, but with a different draw
	line.drawlist={}
	line:drawadd(function(self) tpt.drawline(self.x,self.y,self.x2,self.y2,self.r,self.g,self.b) end)
	return line
	end
}
ui_text = {
new = function(text,x,y,r,g,b)
	local txt = ui_base.new()
	txt.text = text
	txt.x=x or 0 txt.y=y or 0 txt.r=r or 255 txt.g=g or 255 txt.b=b or 255
	function txt:setcolor(r,g,b) self.r=r self.g=g self.b=b end
	txt:drawadd(function(self,x,y) tpt.drawtext(x or self.x,y or self.y,self.text,self.r,self.g,self.b) end)
	txt:moveadd(function(self,x,y)
		if x then self.x=self.x+x end
		if y then self.y=self.y+y end
	end)
	function txt:process() return false end
	return txt
end,
--Scrolls while holding mouse over
newscroll = function(text,x,y,vis,r,g,b)
	local txt = ui_text.new(text,x,y,r,g,b)
	if tpt.textwidth(text)<vis then return txt end
	txt.visible=vis
	txt.length=string.len(text)
	txt.start=1
	txt.drawlist={} --reset draw
	txt.timer=socket.gettime()+3
	function txt:cuttext(self)
		local last = self.start+1
		while tpt.textwidth(self.text:sub(self.start,last))<txt.visible and last<=self.length do
			last = last+1
		end
		self.last=last-1
	end
	txt:cuttext(txt)
	txt.minlast=txt.last-1
	txt.ppl=((txt.visible-6)/(txt.length-txt.minlast+1))
	txt:drawadd(function(self,x,y)
		if socket.gettime() > self.timer then
			if self.last >= self.length then
				self.start = 1
				self:cuttext(self)
				self.timer = socket.gettime()+3
			else
				self.start = self.start + 1
				self:cuttext(self)
				if self.last >= self.length then
					self.timer = socket.gettime()+3
				else
					self.timer = socket.gettime()+.15
				end
			end
		end
		tpt.drawtext(x or self.x,y or self.y, self.text:sub(self.start,self.last) ,self.r,self.g,self.b)
	end)
	function txt:process(mx,my,button,event,wheel)
		if event==3 then
			local newlast = math.floor((mx-self.x)/self.ppl)+self.minlast
			if newlast<self.minlast then newlast=self.minlast end
			if newlast>0 and newlast~=self.last then
				local newstart=1
				while tpt.textwidth(self.text:sub(newstart,newlast))>= self.visible do
					newstart=newstart+1
				end
				self.start=newstart self.last=newlast
				self.timer = socket.gettime()+3
			end
		end
	end
	return txt
end
}
ui_scrollbar = {
new = function(x,y,h,t,m)
	local bar = ui_base.new() --use line object as base?
	bar.x=x bar.y=y bar.h=h
	bar.total=t
	bar.numshown=m
	bar.pos=0
	bar.length=math.floor((1/math.ceil(bar.total-bar.numshown+1))*bar.h)
	bar.soffset=math.floor(bar.pos*((bar.h-bar.length)/(bar.total-bar.numshown)))
	function bar:update(total,shown,pos)
		self.pos=pos or 0
		if self.pos<0 then self.pos=0 end
		self.total=total
		self.numshown=shown
		self.length= math.floor((1/math.ceil(self.total-self.numshown+1))*self.h)
		self.soffset= math.floor(self.pos*((self.h-self.length)/(self.total-self.numshown)))
	end
	function bar:move(wheel)
		self.pos = self.pos-wheel
		if self.pos < 0 then self.pos=0 end
		if self.pos > (self.total-self.numshown) then self.pos=(self.total-self.numshown) end
		self.soffset= math.floor(self.pos*((self.h-self.length)/(self.total-self.numshown)))
	end
	bar:drawadd(function(self)
		if self.total > self.numshown then
			tpt.drawline(self.x,self.y+self.soffset,self.x,self.y+self.soffset+self.length)
		end
	end)
	bar:moveadd(function(self,x,y)
		if x then self.x=self.x+x end
		if y then self.y=self.y+y end
	end)
	function bar:process(mx,my,button,event,wheel)
		if wheel~=0 and not MANAGER.hidden then
			if self.total > self.numshown then
				local previous = self.pos
				self:move(wheel)
				if self.pos~=previous then
					return previous-self.pos
				end
			end
		end
		--possibly click the bar and drag?
		return false
	end
	return bar
end
}
ui_button = {
new = function(x,y,w,h,f,text)
	local b = ui_box.new(x,y,w,h)
	b.f=f
	b.t=ui_text.new(text,x+2,y+2)
	b.drawbox=false
	b.clicked=false
	b.almostselected=false
	b.invert=true
	b:setbackground(127,127,127,125)
	b:drawadd(function(self)
		if self.invert and self.almostselected then
			self.almostselected=false
			tpt.fillrect(self.x,self.y,self.w,self.h)
			local tr=self.t.r local tg=self.t.g local tb=self.t.b
			b.t:setcolor(0,0,0)
			b.t:draw()
			b.t:setcolor(tr,tg,tb)
		else
			if tpt.mousex>=self.x and tpt.mousex<=self.x2 and tpt.mousey>=self.y and tpt.mousey<=self.y2 then
				self.drawbackground=true
			else
				self.drawbackground=false
			end
			b.t:draw()
		end
	end)
	b:moveadd(function(self,x,y)
		self.t:onmove(x,y)
	end)
	function b:process(mx,my,button,event,wheel)
		local clicked = self.clicked
		if event==2 then self.clicked = false end
		if mx<self.x or mx>self.x2 or my<self.y or my>self.y2 then return false end
		if event==1 then
			self.clicked=true
		elseif clicked then
			if event==3 then self.almostselected=true end
			if event==2 then self:f() end
			return true
		end
	end
	return b
end
}
ui_tooltip = {
new = function(x,y,w,text)
	local b = ui_box.new(x,y-1,w,0)
	function b:updatetooltip(tooltip)
		self.tooltip = tooltip
		self.length = #tooltip
		self.lines = 1

		local linebreak,lastspace = 0,nil
		for i=0,#self.tooltip do
			local width = tpt.textwidth(tooltip:sub(linebreak,i+1))
			if width > self.w/2 and tooltip:sub(i,i):match("[%s,_%.%-?!]") then
				lastspace = i
			end
			local isnewline = (self.tooltip:sub(i,i) == '\n')
			if width > self.w or isnewline then
				local pos = (i==#tooltip or not lastspace) and i or lastspace
				self.lines = self.lines + 1
				if self.tooltip:sub(pos,pos) == ' ' then
					self.tooltip = self.tooltip:sub(1,pos-1).."\n"..self.tooltip:sub(pos+1)
				elseif not isnewline then
					self.length = self.length + 1
					self.tooltip = self.tooltip:sub(1,pos-1).."\n"..self.tooltip:sub(pos)
					i = i + 1
					pos = pos + 1
				end
				linebreak = pos+1
				lastspace = nil
			end
		end
		self.h = self.lines*12+2
		if self.y + self.h > gfx.HEIGHT then
			local movement = (gfx.HEIGHT-self.h-1)-self.y
			if self.y+movement < 0 then
				movement = -self.y
			end
			self:onmove(0, movement)
		end
		--self.w = tpt.textwidth(self.tooltip)+3
		self.drawbox = tooltip ~= ""
		self.drawbackground = tooltip ~= ""
	end
	function b:settooltip(tooltip_)
		tooltip:onmove(tpt.mousex+5-tooltip.x, tpt.mousey+5-tooltip.y)
		tooltip:updatetooltip(tooltip_)
	end
	b:updatetooltip(text)
	b:setbackground(0,0,0,255)
	b.drawbackground = true
	b:drawadd(function(self)
		if self.tooltip ~= "" then
			tpt.drawtext(self.x+1,self.y+2,self.tooltip)
		end
		self:updatetooltip("")
	end)
	function b:process(mx,my,button,event,wheel) end
	return b
end
}
ui_checkbox = {
up_button = function(x,y,w,h,f,text)
	local b=ui_button.new(x,y,w,h,f,text)
	b.canupdate=false
	return b
end,
new_button = function(x,y,w,h,splitx,f,f2,text,localscript)
	local b = ui_box.new(x,y,splitx,h)
	b.f=f b.f2=f2
	b.localscript=localscript
	b.splitx = splitx
	b.t=ui_text.newscroll(text,x+24,y+2,splitx-24)
	b.clicked=false
	b.selected=false
	b.checkbut=ui_checkbox.up_button(x+splitx+9,y,33,9,ui_button.scriptcheck,"Update")
	b.drawbox=false
	b:setbackground(127,127,127,100)
	b:drawadd(function(self)
		if self.t.text == "" then return end
		self.drawbackground = false
		if tpt.mousey >= self.y and tpt.mousey < self.y2 then
			if tpt.mousex >= self.x and tpt.mousex < self.x+8 then
				if self.localscript then
					tooltip:settooltip("删除此脚本")
				else
					tooltip:settooltip("在浏览中查看脚本")
				end
			elseif tpt.mousex>=self.x and tpt.mousex<self.x2 then
				local script
				if online and onlinescripts[self.ID]["description"] then
					script = onlinescripts[self.ID]
				elseif not online and localscripts[self.ID] then
					script = localscripts[self.ID]
				end
				if script then
					tooltip:settooltip(script["name"].." by "..script["author"].."\n\n"..script["description"])
				end
				self.drawbackground = true
			elseif tpt.mousex >= self.x2 then
				if tpt.mousex < self.x2+9 and self.running then
					tooltip:settooltip(online and "已下载" or "运行中")
				elseif tpt.mousex >= self.x2+9 and tpt.mousex < self.x2+43 and self.checkbut.canupdate and onlinescripts[self.ID] and onlinescripts[self.ID]["changelog"] then
					tooltip:settooltip(onlinescripts[self.ID]["changelog"])
				end
			end
		end
		self.t:draw()
		if self.localscript then
			local swapicon = tpt.version.jacob1s_mod_build and tpt.version.jacob1s_mod_build > 76
			local offsetX = swapicon and 1 or 0
			local offsetY = swapicon and 2 or 0
			local innericon = swapicon and icons["delete1"] or icons["delete2"]
			local outericon = swapicon and icons["delete2"] or icons["delete1"]
			if self.deletealmostselected then
				self.deletealmostselected = false
				tpt.drawtext(self.x+1+offsetX, self.y+1+offsetY, innericon, 255, 48, 32, 255)
			else
				tpt.drawtext(self.x+1+offsetX, self.y+1+offsetY, innericon, 160, 48, 32, 255)
			end
			tpt.drawtext(self.x+1+offsetX, self.y+1+offsetY, outericon, 255, 255, 255, 255)
		else
			tpt.drawtext(self.x+1, self.y+1, icons["folder"], 255, 200, 80, 255)
		end
		tpt.drawrect(self.x+12,self.y+1,8,8)
		if self.almostselected then self.almostselected=false tpt.fillrect(self.x+12,self.y+1,8,8,150,150,150)
		elseif self.selected then tpt.fillrect(self.x+12,self.y+1,8,8) end
		local filepath = self.ID and localscripts[self.ID] and localscripts[self.ID]["path"] or self.t.text
		if self.running then tpt.drawtext(self.x+self.splitx+2,self.y+2,online and "D" or "R") end
		if self.checkbut.canupdate then self.checkbut:draw() end
	end)
	b:moveadd(function(self,x,y)
		self.t:onmove(x,y)
		self.checkbut:onmove(x,y)
	end)
	function b:process(mx,my,button,event,wheel)
		if self.f2 and mx <= self.x+8 then
			if event==1 then
				self.clicked = 1
			elseif self.clicked == 1 then
				if event==3 then self.deletealmostselected = true end
				if event==2 then self:f2() end
			end
		elseif self.f and mx<=self.x+self.splitx then
			if event==1 then
				self.clicked = 2
			elseif self.clicked == 2 then
				if event==3 then self.almostselected=true end
				if event==2 then self:f() end
				self.t:process(mx,my,button,event,wheel)
			end
		else
			if self.checkbut.canupdate then self.checkbut:process(mx,my,button,event,wheel) end
		end
		return true
	end
	return b
end,
new = function(x,y,w,h)
	local box = ui_box.new(x,y,w,h)
	box.list={}
	box.numlist = 0
	box.max_lines = math.floor(box.h/10)-1
	box.max_text_width = math.floor(box.w*0.8)
	box.splitx=x+box.max_text_width
	box.scrollbar = ui_scrollbar.new(box.x2-2,box.y+11,box.h-12,0,box.max_lines)
	box.lines={
		ui_line.new(box.x+1,box.y+10,box.x2-1,box.y+10,170,170,170),
		ui_line.new(box.x+22,box.y+10,box.x+22,box.y2-1,170,170,170),
		ui_line.new(box.splitx,box.y+10,box.splitx,box.y2-1,170,170,170),
		ui_line.new(box.splitx+9,box.y+10,box.splitx+9,box.y2-1,170,170,170),
	}
	function box:updatescroll()
		self.scrollbar:update(self.numlist,self.max_lines)
	end
	function box:clear()
		self.list={}
		self.numlist=0
	end
	function box:add(f,f2,text,localscript)
		local but = ui_checkbox.new_button(self.x,self.y+1+((self.numlist+1)*10),tpt.textwidth(text)+4,10,self.max_text_width,f,f2,text,localscript)
		table.insert(self.list,but)
		self.numlist = #self.list
		return but
	end
	box:drawadd(function (self)
		tpt.drawtext(self.x+24,self.y+2,"Files in "..TPT_LUA_PATH.." folder")
		tpt.drawtext(self.splitx+11,self.y+2,"Update")
		for i,line in ipairs(self.lines) do
			line:draw()
		end
		self.scrollbar:draw()
		local restart = false
		for i,check in ipairs(self.list) do
			local filepath = check.ID and localscripts[check.ID] and localscripts[check.ID]["path"] or check.t.text
			if not check.selected and running[filepath] then
				restart = true
			end
			if i>self.scrollbar.pos and i<=self.scrollbar.pos+self.max_lines then
				check:draw()
			end
		end
		requiresrestart = restart and not online
	end)
	box:moveadd(function(self,x,y)
		for i,line in ipairs(self.lines) do
			line:onmove(x,y)
		end
		for i,check in ipairs(self.list) do
			check:onmove(x,y)
		end
	end)
	function box:scroll(amount)
		local move = amount*10
		if move==0 then return end
		for i,check in ipairs(self.list) do
			check:onmove(0,move)
		end
	end
	function box:process(mx,my,button,event,wheel)
		if mx<self.x or mx>self.x2 or my<self.y or my>self.y2-7 then return false end
		local scrolled = self.scrollbar:process(mx,my,button,event,wheel)
		if scrolled then self:scroll(scrolled) end
		local which = math.floor((my-self.y-11)/10)+1
		if which>0 and which<=self.numlist then self.list[which+self.scrollbar.pos]:process(mx,my,button,event,wheel) end
		if event == 2 then
			for i,v in ipairs(self.list) do v.clicked = false end
		end
		return true
	end
	return box
end
}
ui_console = {
new = function(x,y,w,h)
	local con = ui_box.new(x,y,w,h)
	con.shown_lines = math.floor(con.h/10)
	con.max_lines = 300
	con.max_width = con.w-4
	con.lines = {}
	con.scrollbar = ui_scrollbar.new(con.x2-2,con.y+1,con.h-2,0,con.shown_lines)
	con:drawadd(function(self)
		self.scrollbar:draw()
		local count=0
		for i,line in ipairs(self.lines) do
			if i>self.scrollbar.pos and i<= self.scrollbar.pos+self.shown_lines then
				line:draw(self.x+3,self.y+3+(count*10))
				count = count+1
			end
		end
	end)
	con:moveadd(function(self,x,y)
		self.scrollbar:onmove(x,y)
	end)
	function con:clear()
		self.lines = {}
		self.scrollbar:update(0,con.shown_lines)
	end
	function con:addstr(str,r,g,b)
		str = tostring(str)
		local nextl = str:find('\n')
		while nextl do
			local line = str:sub(1,nextl-1)
			self:addline(line,r,g,b)
			str = str:sub(nextl+1)
			nextl = str:find('\n')
		end
		self:addline(str,r,g,b) --anything leftover
	end
	function con:addline(line,r,g,b)
		if not line or line=="" then return end --No blank lines
		table.insert(self.lines,ui_text.newscroll(line,self.x,0,self.max_width,r,g,b))
		if #self.lines>self.max_lines then table.remove(self.lines,1) end
		self.scrollbar:update(#self.lines,self.shown_lines,#self.lines-self.shown_lines)
	end
	function con:process(mx,my,button,event,wheel)
		if mx<self.x or mx>self.x2 or my<self.y or my>self.y2 then return false end
		self.scrollbar:process(mx,my,button,event,wheel)
		local which = math.floor((my-self.y-1)/10)+1
		if which>0 and which<=self.shown_lines and self.lines[which+self.scrollbar.pos] then self.lines[which+self.scrollbar.pos]:process(mx,my,button,event,wheel) end
		return true
	end
	return con
end
}
ui_window = {
new = function(x,y,w,h)
	local w=ui_box.new(x,y,w,h)
	w.sub={}
	function w:add(m,name)
		if name then w[name]=m end
		table.insert(self.sub,m)
		return m
	end
	w:drawadd(function(self)
		for i,sub in ipairs(self.sub) do
			sub:draw()
		end
	end)
	w:moveadd(function(self,x,y)
		for i,sub in ipairs(self.sub) do
			sub:onmove(x,y)
		end
	end)
	function w:process(mx,my,button,event,wheel)
		if mx<self.x or mx>self.x2 or my<self.y or my>self.y2 then if button == 0 then return end ui_button.sidepressed() return true end
		local ret
		for i,sub in ipairs(self.sub) do
			if sub:process(mx,my,button,event,wheel) then ret = true end
		end
		return ret
	end
	return w
end
}
--Main window with everything!
local mainwindow = ui_window.new(50,50,525,300)
mainwindow:setbackground(10,10,10,235) mainwindow.drawbackground=true
mainwindow:add(ui_console.new(275,148,300,189),"menuconsole")
mainwindow:add(ui_checkbox.new(50,80,225,257),"checkbox")
tooltip = ui_tooltip.new(0,1,250,"")

--Some API functions you can call from other scripts
--put 'using_manager=MANAGER ~= nil' or similar in your scripts, using_manager will be true if the manager is active
--Print a message to the manager console, can be colored
function MANAGER.print(msg,...)
	mainwindow.menuconsole:addstr(msg,...)
end
--downloads and returns a file, so you can do whatever...
local download_file
function MANAGER.download(url)
	return download_file(url)
end
function MANAGER.scriptinfo(id)
	local url = "https://starcatcher.us/scripts/main.lua"
	if id then
		url = url.."?info="..id
	end
	local info = download_file(url)
	infotable = readScriptInfo(info)
	return id and infotable[id] or infotable
end
--Get various info about the system (operating system, script directory, path seperator, if socket is loaded)
function MANAGER.sysinfo()
	return {["OS"]=OS, ["scriptDir"]=TPT_LUA_PATH, ["pathSep"]=PATH_SEP, ["exeName"] = EXE_NAME}
end
--Save a setting in the autorun settings file, ident should be your script name no one else would use.
--Name is variable name, val is the value which will be saved/returned as a string
function MANAGER.savesetting(ident,name,val)
	ident = tostring(ident)
	name = tostring(name)
	val = tostring(val)
	if settings[ident] then settings[ident][name]=val
	else settings[ident]={[name]=val} end
	save_last()
end
--Get a previously saved value, if it has one
function MANAGER.getsetting(ident,name)
	if settings[ident] then return settings[ident][name] end
	return nil
end
--delete a setting, leave name nil to delete all of ident
function MANAGER.delsetting(ident,name)
	if settings[ident] then
	if name then settings[ident][name]=nil
	else settings[ident]=nil end
	save_last()
	end
end

--mniip's download thing (mostly)
local pattern = "http://w*%.?(.-)(/.*)"
function download_file(url)
	if not http then
		MANAGER.print("TPT 95.0 or greater required to use http api", 255, 0, 0)
		return false
	end
	local req = http.get(url)
	local timeout_after = socket.gettime() + 3
	while true do
		local status = req:status()
		if status ~= "running" then
			local body, status_code = req:finish()
			if status_code and status_code ~= 200 then
				MANAGER.print("http download failed with status code " .. status_code, 255, 0, 0)
				return nil
			end
			return body
		end

		if socket.gettime() > timeout_after then
			MANAGER.print("http download timed out ", 255, 0, 0)
			req:cancel()
			return
		end
	end
end
--Downloads to a location
local function download_script(ID,location)
	local file = download_file("https://starcatcher.us/scripts/main.lua?get="..ID)
	if file then
		f=io.open(location,"w")
		f:write(file)
		f:close()
		return true
	end
	return false
end
--Restart exe (if named correctly)
local function do_restart()
	save_last()
	if platform then
		platform.restart()
	end
	if OS == "WIN32" or OS == "WIN64" then
		os.execute("TASKKILL /IM \""..EXE_NAME.."\" /F &&START .\\\""..EXE_NAME.."\"")
	elseif OS == "OSX" then
		MANAGER.print("Can't restart on OS X when using game versions less than 91.0, please manually close and reopen The Powder Toy")
		return
	else
		os.execute("killall -s KILL \""..EXE_NAME.."\" && ./\""..EXE_NAME.."\"")
	end
	MANAGER.print("Restart failed, do you have the exe name right?",255,0,0)
end
local function open_link(url)
	if platform then
		platform.openLink(url)
	else
		local command = (OS == "WIN32" or OS == "WIN64") and "start" or (OS == "MACOSX" and "open" or "xdg-open")
		os.execute(command.." "..url)
	end
end
--TPT interface
local function step()
	if jacobsmod then
		tpt.fillrect(0,0,gfx.WIDTH,gfx.HEIGHT,0,0,0,150)
	else
		tpt.fillrect(-1,-1,gfx.WIDTH,gfx.HEIGHT,0,0,0,150)
	end
	mainwindow:draw()
	tpt.drawtext(280,135,"控制台输出:                                 DragonRSTER")
	if requiresrestart then
		tpt.drawtext(280,88,"Disabling a script requires a restart for effect!",255,50,50)
	end
	tpt.drawtext(55,53,"点击选择脚本,点击完成后以激活脚本")
	tpt.drawtext(474,55,"脚本管理器 v"..MANAGER.version)--479 for simple versions
	tooltip:draw()

	if online_req and online then
		local textwidth = tpt.textwidth("加载中...")
		tpt.drawtext(mainwindow.checkbox.x + (mainwindow.checkbox.w - textwidth) / 2, mainwindow.checkbox.y + (mainwindow.checkbox.h - 6) / 2, "加载中...")
	end
end
local function mouseclick(mousex,mousey,button,event,wheel)
	sidebutton:process(mousex,mousey,button,event,wheel)
	if MANAGER.hidden then return true end

	if mousex>612 or mousey>384 then return false end
	mainwindow:process(mousex,mousey,button,event,wheel)
	return false
end
local jacobsmod_old_menu_check = false
local function keypress(key,nkey,modifier,event)
	if jacobsmod and (key == 'o' or nkey == 96) and event == 1 then jacobsmod_old_menu_check = true end
	if nkey==27 and not MANAGER.hidden then MANAGER.hidden=true return false end
	if MANAGER.hidden then return end

	if event == 1 then
		if key == "[" then
			mainwindow:process(mainwindow.x+30, mainwindow.y+30, 0, 2, 1)
		elseif key == "]" then
			mainwindow:process(mainwindow.x+30, mainwindow.y+30, 0, 2, -1)
		end
	end
	return false
end
--small button on right to bring up main menu
local WHITE = {255,255,255,255}
local BLACK = {0,0,0,255}
local ICON = math.random(2) --pick a random icon
local lua_letters= {{{2,2,2,7},{2,7,4,7},{6,7,6,11},{6,11,8,11},{8,7,8,11},{10,11,12,11},{10,11,10,15},{11,13,11,13},{12,11,12,15},},
	{{2,3,2,13},{2,14,7,14},{4,3,4,12},{4,12,7,12},{7,3,7,12},{9,3,12,3},{9,3,9,14},{10,8,11,8},{12,3,12,14},}}
local function smallstep()
	gfx.drawRect(sidebutton.x, sidebutton.y+1, sidebutton.w+1, sidebutton.h+1,200,200,200)
	local color=WHITE
	if not MANAGER.hidden then
		step()
		gfx.fillRect(sidebutton.x, sidebutton.y+1, sidebutton.w+1, sidebutton.h+1)
		color=BLACK
	end
	for i,dline in ipairs(lua_letters[ICON]) do
		tpt.drawline(dline[1]+sidebutton.x,dline[2]+sidebutton.y,dline[3]+sidebutton.x,dline[4]+sidebutton.y,color[1],color[2],color[3])
	end
	if jacobsmod_old_menu_check then
		local ypos = 134
		if jacobsmod and tpt.oldmenu and tpt.oldmenu()==1 then
			ypos = 390
		elseif tpt.num_menus then
			ypos = 390-16*tpt.num_menus()-(not jacobsmod and 16 or 0)
		end
		sidebutton:onmove(0, ypos-sidebutton.y)
		jacobsmod_old_menu_check = false
	end
	check_req_status()
end
--button functions on click


--安装元素说明
function ui_button.downloadtpt(self)
	--安装元素说明
local function uiinstalltptinstruction(location)
	local file = download_file("https://pan.dragonrster.top/Game/ThePowderToy/scripts/123.txt")
	if file then
		f=io.open("./scripts/".. location,"wb")
		f:write(file)
		f:close()
		running["123.lua"] = true
		do_restart()
		return true
	end
	return false
end
uiinstalltptinstruction("123.lua")
end
function ui_button.reloadpressed(self)
	if not online then
		load_filenames()
		load_downloaded()
		gen_buttons()
		mainwindow.checkbox:updatescroll()
		if num_files == 0 then
			MANAGER.print("在'"..TPT_LUA_PATH.."'中没有找到脚本",255,255,0)
			fs.makeDirectory(TPT_LUA_PATH)
		else
			MANAGER.print("重新加载文件列表,找到 "..num_files.." 个脚本")
		end
	else
		search_terms = {}
		local filter = tpt.input("脚本检索", "输入要检索的条件")
		for match in filter:gmatch("%w+") do
			table.insert(search_terms, match)
		end
		gen_buttons()
	end
end
function ui_button.selectnone(self)
	for i,but in ipairs(mainwindow.checkbox.list) do
		but.selected = false
	end
end
function ui_button.consoleclear(self)
	mainwindow.menuconsole:clear()
end
function ui_button.changedir(self)
	local last = TPT_LUA_PATH
	local new = tpt.input("更改搜索目录","输入脚本所在的文件夹",TPT_LUA_PATH,TPT_LUA_PATH)
	if new~=last and new~="" then
		fs.removeFile(last..PATH_SEP.."autorunsettings.txt")
		MANAGER.print("目录更改为 "..new,255,255,0)
		TPT_LUA_PATH = new
	end
	ui_button.reloadpressed()
	save_last()
end
function ui_button.uploadscript(self)
	if not online then
		local command = (OS == "WIN32" or OS == "WIN64") and "start" or (OS == "MACOSX" and "open" or "xdg-open")
		os.execute(command.." "..TPT_LUA_PATH)
	else
		open_link("https://starcatcher.us/scripts/paste.lua")
	end
end
local lastpaused
function ui_button.sidepressed(self)
	if TPTMP and TPTMP.chatHidden == false then print("在打开管理器之前尽量关闭TPTMP") return end
	MANAGER.hidden = not MANAGER.hidden
	ui_button.localview()
	if not MANAGER.hidden then
		lastpaused = tpt.set_pause()
		tpt.set_pause(1)
		ui_button.reloadpressed()
	else
		tpt.set_pause(lastpaused)
	end
end
local donebutton
function ui_button.donepressed(self)
	MANAGER.hidden = true
	for i,but in ipairs(mainwindow.checkbox.list) do
		local filepath = but.ID and localscripts[but.ID]["path"] or but.t.text
		if but.selected then
			if requiresrestart then
				running[filepath] = true
			else
				if not running[filepath] then
					local status,err = pcall(dofile,TPT_LUA_PATH..PATH_SEP..filepath)
					if not status then
						MANAGER.print(err,255,0,0)
						print(err)
						but.selected = false
					else
						MANAGER.print("开始 "..filepath)
						running[filepath] = true
					end
				end
			end
		elseif running[filepath] then
			running[filepath] = nil
		end
	end
	if requiresrestart then do_restart() return end
	save_last()
end
function ui_button.downloadpressed(self)
	local successful_download = false
	for i,but in ipairs(mainwindow.checkbox.list) do
		if but.selected then
			--maybe do better display names later
			local displayName
			local function get_script(butt)
				local script = download_file("https://starcatcher.us/scripts/main.lua?get="..butt.ID)
				if not script then
					MANAGER.print("下载失败 " .. but.t.text, 255, 0, 0)
					return false
				end
				displayName = "downloaded"..PATH_SEP..butt.ID.." "..onlinescripts[butt.ID].author:gsub("[^%w _-]", "_").."-"..onlinescripts[butt.ID].name:gsub("[^%w _-]", "_")..".lua"
				local name = TPT_LUA_PATH..PATH_SEP..displayName
				if not fs.exists(TPT_LUA_PATH..PATH_SEP.."downloaded") then
					fs.makeDirectory(TPT_LUA_PATH..PATH_SEP.."downloaded")
				end
				local file = io.open(name, "w")
				if not file then error("无法打开 "..name) end
				file:write(script)
				file:close()
				if localscripts[butt.ID] and localscripts[butt.ID]["path"] ~= displayName then
					local oldpath = localscripts[butt.ID]["path"]
					fs.removeFile(TPT_LUA_PATH.."/"..oldpath:gsub("\\","/"))
					running[oldpath] = nil
				end
				localscripts[butt.ID] = onlinescripts[butt.ID]
				localscripts[butt.ID]["path"] = displayName
				dofile(name)

				return true
			end
			local status,err = pcall(get_script, but)
			if not status then
				MANAGER.print(err,255,0,0)
				print(err)
				but.selected = false
			elseif err == true then
				MANAGER.print("下载并启动 "..but.t.text)
				running[displayName] = true
				successful_download = true
			end
		end
	end
	if successful_download then
		MANAGER.hidden = true
		ui_button.localview()
		save_last()
	end
end

function ui_button.pressed(self)
	self.selected = not self.selected
end
function ui_button.delete(self)
	--there is no tpt.confirm() yet
	if tpt.input("Delete File", "Delete "..self.t.text.."?", "yes", "no") == "yes" then
		local filepath = self.ID and localscripts[self.ID]["path"] or self.t.text
		fs.removeFile(TPT_LUA_PATH.."/"..filepath:gsub("\\","/"))
		if running[filepath] then running[filepath] = nil end
		if localscripts[self.ID] then localscripts[self.ID] = nil end
		save_last()
		ui_button.localview()
		load_filenames()
		gen_buttons()
	end
end
function ui_button.viewonline(self)
	open_link("https://starcatcher.us/scripts?view="..self.ID)
end
function ui_button.scriptcheck(self)
	local oldpath = localscripts[self.ID]["path"]
	local newpath = "downloaded"..PATH_SEP..self.ID.." "..onlinescripts[self.ID].author:gsub("[^%w _-]", "_").."-"..onlinescripts[self.ID].name:gsub("[^%w _-]", "_")..".lua"
	if download_script(self.ID,TPT_LUA_PATH..PATH_SEP..newpath) then
		self.canupdate = false
		localscripts[self.ID] = onlinescripts[self.ID]
		localscripts[self.ID]["path"] = newpath
		if oldpath:gsub("\\","/") ~= newpath:gsub("\\","/") then
			fs.removeFile(TPT_LUA_PATH.."/"..oldpath:gsub("\\","/"))
			if running[oldpath] then
				running[newpath],running[oldpath] = running[oldpath],nil
			end
		end
		if running[newpath] then
			do_restart()
		else
			save_last()
			MANAGER.print("Updated "..onlinescripts[self.ID]["name"])
		end
	end
end
function ui_button.doupdate(self)
	if jacobsmod and jacobsmod >= 30 then
		fileSystem.move("scriptmanager.lua", "scriptmanagerold.lua")
		download_script(1, 'scriptmanager.lua')
	else
		fileSystem.move("autorun.lua", "autorunold.lua")
		download_script(1, 'autorun.lua')
	end
	localscripts[1] = updatetable[1]
	do_restart()
end
local uploadscriptbutton, reloadbutton
function ui_button.localview(self)
	if online then
		online = false
		gen_buttons()
		donebutton.t.text = "确定"
		donebutton.w = 29 donebutton.x2 = donebutton.x + donebutton.w
		donebutton.f = ui_button.donepressed
		uploadscriptbutton.t.text = icons["folder"].." 脚本文件夹"
		reloadbutton.t.text = "刷新"
	end
end
function ui_button.onlineview(self)
	if not online then
		online = true
		gen_buttons()
		donebutton.t.text = "下载"
		donebutton.w = 35 donebutton.x2 = donebutton.x + donebutton.w
		donebutton.f = ui_button.downloadpressed
		uploadscriptbutton.t.text = "    上载脚本"
		reloadbutton.t.text = "检索"
		search_terms = {}
	end
end
--add buttons to window
donebutton = ui_button.new(55,337,29,10,ui_button.donepressed,"完成")
mainwindow:add(donebutton)
mainwindow:add(ui_button.new(134,337,29,10,ui_button.sidepressed,"返回"))
--mainwindow:add(ui_button.new(152,339,29,10,ui_button.selectnone,"NONE"))
local nonebutton = ui_button.new(62,81,8,8,ui_button.selectnone,"")
nonebutton.drawbox = true
mainwindow:add(nonebutton)
mainwindow:add(ui_button.new(538,337,30,10,ui_button.consoleclear,"清除"))
reloadbutton = mainwindow:add(ui_button.new(278,67,29,13,ui_button.reloadpressed,"刷新"), "reload")
downloadtptbutton = mainwindow:add(ui_button.new(150,65,80,11,ui_button.downloadtpt,"安装元素说明"), "reload")
mainwindow:add(ui_button.new(378,67,79,13,ui_button.changedir,"更改脚本路径"))
uploadscriptbutton = mainwindow:add(ui_button.new(478,67,84,13,ui_button.uploadscript, icons["folder"].." 脚本文件夹"))
local tempbutton = ui_button.new(60, 65, 30, 13, ui_button.localview, "本地")
tempbutton.drawbox = true
mainwindow:add(tempbutton)
tempbutton = ui_button.new(100, 65, 30, 13, ui_button.onlineview, "在线")
tempbutton.drawbox = true
mainwindow:add(tempbutton)
local ypos = 134
if jacobsmod and tpt.oldmenu and tpt.oldmenu()==1 then
	ypos = 390
elseif tpt.num_menus then
	ypos = 390-16*tpt.num_menus()-(not jacobsmod and 16 or 0)
end
sidebutton = ui_button.new(gfx.WIDTH-16,ypos,14,15,ui_button.sidepressed,'')

local function gen_buttons_local()
	local count = 0
	local sorted = {}
	for k,v in pairs(localscripts) do if v.ID ~= 1 then table.insert(sorted, v) end end
	table.sort(sorted, function(first,second) return first.name:lower() < second.name:lower() end)
	for i,v in ipairs(sorted) do
		local check = mainwindow.checkbox:add(ui_button.pressed,ui_button.delete,v.name,true)
		check.ID = v.ID
		if running[v.path] then
			check.running = true
			check.selected = true
		end
		count = count + 1
	end
	if #sorted >= 5 and #filenames >= 5 then
		mainwindow.checkbox:add(nil, nil, "", false) --empty space to separate things
	end
	for i=1,#filenames do
		local check = mainwindow.checkbox:add(ui_button.pressed,ui_button.delete,filenames[i],true)
		if running[filenames[i]] then
			check.running = true
			check.selected = true
		end
	end
	num_files = count + #filenames
end
local function gen_buttons_online()
	if not http then
		MANAGER.print("TPT 95.0 or greater required to use the online tab", 255, 0, 0)
		return
	end

	if online_req then
		online_req:cancel()
	end

	online_req = http.get("https://starcatcher.us/scripts/main.lua")
	
	if first_online then
		first_online = false
		script_manager_update_req = http.get("https://starcatcher.us/scripts/main.lua?info=1")
	end
end

local function check_search_term(script, search_term)
	search_term = search_term:lower()
	return script.name and script.name:lower():match(search_term)
		or script.description and script.description:lower():match(search_term)
		or script.author and script.author:lower():match(search_term)
end

-- Check status of "Online" tab request
local function check_online_req_status()
	if online_req and online_req:status() ~= "running" then
		local list, status_code = online_req:finish()
		online_req = nil
		if status_code ~= 200 then
			MANAGER.print("下载脚本列表失败,错误码 " .. status_code, 255, 0, 0)
			return
		end
		
		if not online then return end

		onlinescripts = readScriptInfo(list)
		local sorted = {}
		for k,v in pairs(onlinescripts) do table.insert(sorted, v) end
		table.sort(sorted, function(first,second) return first.ID < second.ID end)
		for k,v in pairs(sorted) do
			local matches = true
			for i,term in ipairs(search_terms) do
				matches = matches and check_search_term(v, term)
			end
			if matches then
				local check = mainwindow.checkbox:add(ui_button.pressed, ui_button.viewonline, v.name, false)
				check.ID = v.ID
				check.checkbut.ID = v.ID
				if localscripts[v.ID] then
					check.running = true
					if tonumber(v.version) > tonumber(localscripts[check.ID].version) then
						check.checkbut.canupdate = true
					end
				end
			end
		end

		mainwindow.checkbox:updatescroll()
	end
end

-- Check status of self update check
local function check_update_req_status()
	if script_manager_update_req and script_manager_update_req:status() ~= "running" then
		local updateinfo, status_code = script_manager_update_req:finish()
		script_manager_update_req = nil
		if status_code ~= 200 then
			MANAGER.print("更新检查失败,错误码 " .. status_code, 255, 0, 0)
			return
		end
		
		updatetable = readScriptInfo(updateinfo)
		if not updatetable[1] then return end
		if tonumber(updatetable[1].version) > scriptversion then
			local updatebutton = ui_button.new(278,127,40,10,ui_button.doupdate,"UPDATE")
			updatebutton.t:setcolor(25,255,25)
			mainwindow:add(updatebutton)
			MANAGER.print("A script manager update is available! Click UPDATE",25,255,55)
			MANAGER.print(updatetable[1].changelog,25,255,55)
		end
	end
end

-- Check status of pending requests
function check_req_status()
	check_online_req_status()
	check_update_req_status()
end

gen_buttons = function()
	mainwindow.checkbox:clear()
	if online then
		gen_buttons_online()
	else
		gen_buttons_local()
	end
	mainwindow.checkbox:updatescroll()
end
gen_buttons()

--register manager first
tpt.register_step(smallstep)
--load previously running scripts
local started = ""
for prev,v in pairs(running) do
	local status,err = pcall(dofile,TPT_LUA_PATH..PATH_SEP..prev)
	if not status then
		MANAGER.print(err,255,0,0)
		running[prev] = nil
	else
		started=started.." "..prev
		local newbut = mainwindow.checkbox:add(ui_button.pressed,prev,nil,false)
		newbut.selected=true
	end
end
save_last()
if started~="" then
	MANAGER.print("Auto started"..started)
end
tpt.register_mouseevent(mouseclick)
tpt.register_keypress(keypress)
