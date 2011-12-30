--if (not _G.LibPie) then
	_G.LibPie = {};
	LibPie.SVGTemplate = [[
<!D]]..[[OCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
<svg width="{WIDTH}px" height="{HEIGHT}px" version="1.1" xmlns="http://www.w3.org/2000/svg">
<defs>
		<linearGradient id="backgroundGradient" gradientTransform="rotate(90)">
			<stop offset="10%" stop-color="#e1e9a0" />
			<stop offset="90%" stop-color="#eff4be" />
		</linearGradient>

</defs>
<g id="background" fill="url(#backgroundGradient)" >
	<rect x="0" y="0" width="{WIDTH}px" height="{HEIGHT}px" stroke-width="2" stroke="black"/>
</g>
{DATA}
</svg>
	]]
	function LibPie.Hue2RGB(n1,n2,hue)
		local HLSMax = 240;
		if (hue < 0) then
			hue = hue + HLSMax;
		end

		if (hue > HLSMax) then
			hue = hue - HLSMax;
		end

		if (hue < (HLSMax/6)) then
			return ( n1 + (((n2-n1)*hue+(HLSMax/12))/(HLSMax/6)) );
		end
		if (hue < (HLSMax/2)) then
			return ( n2 );
		end
		if (hue < ((HLSMax*2)/3)) then
			return ( n1 +    (((n2-n1)*(((HLSMax*2)/3)-hue)+(HLSMax/12))/(HLSMax/6)) );
        else
			return ( n1 );
		end
	end


	function LibPie.HLS2RGB(hue,lum,sat)
		local HLSMAX = 240;
		local RGBMAX = 255;
		local R,G,B,Magic1,Magic2;
		if (sat == 0) then
			R = (lum*RGBMAX)/HLSMAX;
			G = R;
			B = R;
		else

			if (lum <= (HLSMAX/2)) then
				Magic2 = (lum*(HLSMAX + sat) + (HLSMAX/2))/HLSMAX;
			else
				Magic2 = lum + sat - ((lum*sat) + (HLSMAX/2))/HLSMAX;
			end
			Magic1 = 2*lum-Magic2;


			R = (LibPie.Hue2RGB(Magic1,Magic2,hue+(HLSMAX/3))*RGBMAX + (HLSMAX/2))/HLSMAX;
			G = (LibPie.Hue2RGB(Magic1,Magic2,hue)*RGBMAX + (HLSMAX/2)) / HLSMAX;
			B = (LibPie.Hue2RGB(Magic1,Magic2,hue-(HLSMAX/3))*RGBMAX + (HLSMAX/2))/HLSMAX;
		end
		return(string.format("#%02X%02X%02X", R,G,B));
    end

	function LibPie.makeColors(n)
		if (n == 1) then return ({"#006abe"}); end
		if (n == 2) then return ({"#b60010", "#57b610"}); end
		if (n == 3) then return ({"#006abe","#b60010", "#57b610"}); end
		if (n == 4) then return ({"#006abe","#b60010", "#57b610","#e9be10"}); end
		if (n == 5) then return ({"#006abe","#b60010", "#57b610","#e9be10","#e910e9"}); end
		local hueBit = 240/n;
		local colorTable = {};

		local Hue = 120;
		local Saturation = 92*2.4;
		local Luminosity = 90;


		for i = 1, n, 1 do
			table.insert(colorTable, LibPie.HLS2RGB(Hue, Luminosity, Saturation));
			Hue = Hue + hueBit;

		end
		return colorTable;
	end

	function LibPie:addSeries(x, ...)
		local child = self;
		local t = { point = x, data = {} };
		for i = 1, select("#", ...) do
			t.data[i] = select(i, ...)
		end
		table.insert(child.dataSeries, t);
	end

	function LibPie:addPoint(x,y)
		local child = self;
		local added = false;
		for k, pair in pairs(child.points) do
			if (pair[1] > x) then
				table.insert(child.points, k, {x,y});
				added = true;
				break;
			end
		end
		if (not added) then
			table.insert(child.points, {x,y});
		end
	end

	function LibPie:printPoints()
		local child = self;
		local tmp = {};
		for k, v in pairs(child.points) do
			table.insert(tmp, string.format("%s,%s", v[1],v[2]));
		end
		return table.concat(tmp, " ");
	end

	function LibPie:newLineGraph(dimension, smooth)
		local root = self;
		local child = {};
		child.type = "line";
		child.position = root.lines;
		child.points = {};
		child.x = {};
		child.data = {};
		child.smooth = smooth or false;
		child.dimension = dimension or 1;
		child.addPoint = LibPie.addPoint;
		child.printPoints = LibPie.printPoints;
		table.insert(root.children, child);
		return child;
	end

	function LibPie:newBarGraph(dimension)
		local root = self;
		local child = self:newLineGraph();
		child.type = "bar";
		root.bars = root.bars + 1;
		child.barOrder = root.bars;
		return child;
	end

	function LibPie:addShare(name, value)
		local child = self;
		if (not child.data.shares) then
			child.data.shares = {};
		end
		table.insert(child.data.shares, {name, value});
		child.data.sum = (child.data.sum or 0) + value;
	end

	function LibPie:showNumbers(bool)
		local child = self;
		child._showNumbers = bool;
	end

	function LibPie:showNames(bool)
		local child = self;
		child._showNames = bool;
	end

	function LibPie:plotPie(x,y,w,h)
		local child = self;
		local colors = LibPie.makeColors(#child.data.shares);
		child.data.svg = string.format([[<g id="libpie_pie_%u">
		]], math.random(123456,1234567));
		local angle = 0;
		local radius = math.sqrt((w*w)+(h*h))/2;
		local fontSize = radius / 8;
		local fontSizeTitle = math.floor(radius / 6.5);

		for k, share in pairs(child.data.shares) do
			local start = angle;
			local shareSize = (share[2]/(child.data.sum/100))*3.6;
			if (shareSize < 2) then shareSize = 2; end
			if (shareSize >= 358) then shareSize = 358; end
			local half = angle + (math.rad(shareSize)/2);
			local stop = angle + math.rad(shareSize);
			angle = stop;
			local startpoint = { math.cos(start), math.sin(start)}
			local halfWay =  { math.cos(half), math.sin(half)}
			local stoppoint = {math.cos(stop), math.sin(stop) };

			local longarc = (shareSize > 180 and 1) or 0;
			local B = (( half > math.rad(0) and half <= math.rad(180)) and 12) or -2;
			local A = ( (half > math.rad(270) or half <= math.rad(90)) and "start") or "end";
			local C = math.random(345678,345678987);


			child.data.svg = child.data.svg .. string.format([[
			<path d="M%u,%u L%s,%s A%u,%u 0 %u,1 %s,%s z"
			style="fill:%s;
				fill-opacity: 1;
				stroke:black;
				stroke-width: 1.25"/>
			<path d="M%u,%u L%s,%s" style="
			fill:none;
				fill-opacity: 1;
				stroke:black;
				stroke-width: %f"/>

			<text x="%u" y="%u" text-anchor="%s"
			style="font-size: %upx; font-family: Arial, Helvetica, Sans-Serif;"
			>%s</text>
			]],

			x, y, x+(startpoint[1]*(w/2)), y + (startpoint[2]*(h/2)), w/2,h/2,longarc, x + (stoppoint[1]*(w/2)), y + (stoppoint[2]*(h/2)), colors[k],
			x + (halfWay[1]*((w/2)-5)), y + (halfWay[2]*((h/2)-5)), x + (halfWay[1]*((w/2)+10)), y + (halfWay[2]*((h/2)+10)), (child._showNames and 1.5) or 0,
			x+(halfWay[1]*((w/2)+10)), y + (halfWay[2]*((h/2)+10))+B, A, fontSize, (child._showNames and ((child._showNumbers and share[1].." ("..LibPie.comma(share[2])..")") or share[1])) or ""
			);
		end
		child.data.svg = child.data.svg .. string.format([[<text x="%u" y="%u" text-anchor="middle" style="font-size: %upx; font-family: Arial, Helvetica, Sans-Serif; font-weight: bold;">%s</text>]],
		x, y - (h/2) - fontSizeTitle - ((child._showNames and fontSize) or 0),fontSizeTitle, child.title or "");
		child.data.svg = child.data.svg .. string.format([[
		<defs>
			<linearGradient id="pieGradient">
			<stop offset="15%%" stop-color="#DDDDDD" />
			<stop offset="85%%" stop-color="#000000" />
			</linearGradient>
		</defs>
		<ellipse cx="%s" cy="%s" rx="%s" ry="%s" style="fill:url(#pieGradient);" opacity="0.28"/>
		]],
			x, y, w/2, h/2);
		child.data.svg = child.data.svg .. "</g>\n";
		return child.data.svg;
	end

	function LibPie:newPie(title)
		local root = self;
		local child = {};
		child.type = "pie";
		child.points = {};
		child.title = title;
		child.data = {};
		child.dataSeries = {};
		child.addShare = LibPie.addShare;
		child.plot = LibPie.plotPie;
		child._showNumbers = false;
		child.showNumbers = LibPie.showNumbers;
		child.showNames = LibPie.showNames;
		table.insert(root.children, child);
		return child;
	end


	function LibPie:setMaxY(y)
		local obj = self;
		self._maxY = y;
	end

	function LibPie:setMinY(y)
		local obj = self;
		self._minY = y;
	end


	function LibPie:minMax()
		local obj = self;
		-- Find min and max values
		obj.maxValue = nil;
		obj.minValue = nil;
		for k,v in pairs(obj.dataSeries) do
			local aVal = nil;
			for i, s in pairs(obj.lines) do
				if (obj.minValue == nil or v.data[s] < obj.minValue) then obj.minValue = v.data[s]; end
				if (obj.maxValue == nil or v.data[s] > obj.maxValue) then obj.maxValue = v.data[s]; end
			end
			for i, s in pairs(obj.bars) do
				if (obj.minValue == nil or v.data[s] < obj.minValue) then obj.minValue = v.data[s]; end
				if (obj.maxValue == nil or v.data[s] > obj.maxValue) then obj.maxValue = v.data[s]; end
			end
			for i, s in pairs(obj.areas) do
				aVal = (aVal or 0) + v.data[s];
			end
			if (#obj.areas > 1) then
				if ((obj.minValue) == nil or (aVal < obj.minValue)) then obj.minValue = aVal; end
				if ((obj.maxValue) == nil or (aVal > obj.maxValue)) then obj.maxValue = aVal; end
			end
		end
		if (obj.min) then obj.minValue = obj.min; end
		if (obj.max) then obj.maxValue = obj.max; end
		obj.minValue = obj.minValue or 1;
		obj.maxValue = obj.maxValue or 0;
		local f = false;
		for k, t in pairs({1,10,100,1000,10000,100000,1000000,10000000,100000000,1000000000,10000000000}) do
			for k, x in pairs({1,2,5}) do
				if (obj.maxValue < (x*t)) then
					obj.maxValue = x*t;
					f = true;
					break;
				end
			end
			if (f) then break; end
		end
		return {obj.minValue, obj.maxValue};
	end


	function LibPie:plot(w,h,X,Y,cStart, cEnd)
		X = X or 0;
		Y = Y or 0;
		w = w or 200;
		h = h or 100;
		cStart = cStart or 0;
		cEnd = cEnd or 0;
		local obj = self;
		if (#obj.dataSeries == 0) then return ""; end
		local spacing = (w-50);
		if (#obj.dataSeries > 1) then spacing = (w-25)/ (#obj.dataSeries); end
		local colors = LibPie.makeColors(#obj.lines+#obj.bars+#obj.areas+cStart);
		local zeroPoint = { y = h - 25, x = 35 };
		local pixelsPerPoint = 1;
		local divisor = obj.divisor or 1;

		-- Find min and max values
		obj:minMax();
		local minMaxSpace = math.abs(obj.maxValue-(obj.min or obj.minValue));
		pixelsPerPoint = (h-50)/(minMaxSpace);

		if (printf) then
			--printf("Got max = %s, min = %s<br/>", obj.maxValue, obj.minValue);
		else
			print("Spacing is ", spacing, " pixels (x ", #obj.dataSeries, " ppp is ", pixelsPerPoint, " pixels");


		end

		local svg = "";

		-- Plot areas

		local areaSum = {};
		local oldcoords = ""
		for k, i in pairs(obj.areas) do
			local coords = "";
			local reverseCoords = "";
			local x = zeroPoint.x;
			for k, v in pairs(obj.dataSeries) do
				local sum = (areaSum[k] or 0) + v.data[i];
				areaSum[k] = sum;
				local newcoords = string.format(" %s,%s ", x, zeroPoint.y - ( sum * pixelsPerPoint));
				coords = coords .. newcoords;
--				reverseCoords = newcoords.. reverseCoords ;
				x = x + spacing;
			end
			local line = string.format([[
			<polyline transform="translate(%s,%s)" id="line_%u" fill="%s" fill-opacity="0.8" stroke="black" stroke-width="1.5" points="%s,%s %s %s,%s %s"/>
			]], X,Y,i, colors[i+cStart-cEnd], zeroPoint.x, zeroPoint.y, coords, x-spacing, zeroPoint.y, oldcoords or "");
			svg = line .. svg;
		--	oldcoords = reverseCoords;
		end

		-- Plot lines
		for k, i in pairs(obj.lines) do
			local coords = "";
			local x = zeroPoint.x;
			for k, v in pairs(obj.dataSeries) do
				coords = coords .. string.format(" %s,%s ", x, zeroPoint.y - ( v.data[i] * pixelsPerPoint));
				x = x + spacing;
			end
			local line = string.format([[
			<polyline transform="translate(%s,%s)" id="line_%u" fill="none" stroke="%s" stroke-width="1.5" points="%s"/>
			]], X,Y,i, colors[i+cStart-cEnd] or "nil", coords);
			svg = svg .. line;
		end

		-- Plot bars
		for z, i in pairs(obj.bars) do
			local bar = "";
			local x = zeroPoint.x;
			for k, v in pairs(obj.dataSeries) do
				local width = ((spacing-15)/#obj.bars);
				local height =	(v.data[i] * pixelsPerPoint);
				local center = ((z-1)*width) - ((spacing-10)/2);
				bar = bar .. string.format([[
				<rect transform="translate(%s,%s)" id="bar_%u" fill="%s" stroke="black" opacity="0.95" stroke-width="1" y="%s" x="%s" width="%spx" height="%spx"/>
				]], X,Y,i, colors[i+cStart-cEnd] or "nil", zeroPoint.y-height, x+center, width, height);
				x = x + spacing;
			end

			svg = svg .. bar;
		end

		-- Plot unit lines
		if (obj.units) then

			local lineHeight = (h-50)/5;
			local x =  zeroPoint.x - (spacing/2)
			for i = 0, 5, 1 do
				local value = ((i/5) * (obj.maxValue-obj.minValue))/divisor;
				svg = svg .. string.format([[
			<polyline transform="translate(%s,%s)" id="uline_%u" fill="none" stroke="black" stroke-width="0.667" points="%s,%s %s,%s"/>
			]], X,Y,i, x, zeroPoint.y - (lineHeight*i), x+w-35, zeroPoint.y - (lineHeight*i));
				if (obj.unitAnchor and (obj.unitAnchor == "right")) then
					svg = svg .. string.format([[
					<text transform="translate(%s,%s)"  id="utext_%u" fill="black" style="font-family: Arial, Helvetica, Sans-Serif; font-size: 12px;" stroke="none" text-anchor="start" x="%upx" y="%upx">%s %s</text>
					]],X,Y, i, x+w-31, zeroPoint.y - (lineHeight*i) + 4, LibPie.comma(value), obj.units);
				else
					svg = svg .. string.format([[
					<text transform="translate(%s,%s)" id="utext_%u" fill="black" style="font-family: Arial, Helvetica, Sans-Serif; font-size: 12px;" stroke="none" text-anchor="end" x="%upx" y="%upx">%s %s - </text>
					]],X,Y, i, x, zeroPoint.y - (lineHeight*i) + 4, LibPie.comma(value), obj.units);
				end
			end
		end
		return svg;
	end

	function LibPie:setGraphType(who, gType)
		local obj = self;
		who = (who or 1) ;
		for k,v in pairs(obj.bars) do if (v == who) then table.remove(obj.bars, k); break end end
		for k,v in pairs(obj.lines) do if (v == who) then table.remove(obj.lines, k); break end end
		for k,v in pairs(obj.areas) do if (v == who) then table.remove(obj.areas, k); break end end
		if (gType == "line") then table.insert(obj.lines, who); end
		if (gType == "bar") then table.insert(obj.bars, who); end
		if (gType == "area") then table.insert(obj.areas, who); end
	end

	function LibPie:init(obj)
		obj.children = {};
		obj.dataSeries = {};
		obj.newLineGraph = LibPie.newLineGraph
		obj.newBarGraph = LibPie.newBarGraph
		obj.newPie = LibPie.newPie;
		obj.addDimension = LibPie.addDimension;
		obj.addSeries = LibPie.addSeries;
		obj.setGraphType = LibPie.setGraphType;
		obj.minMax = LibPie.minMax;
		obj.plot = LibPie.plot;
		obj.bars = {};
		obj.lines = {};
		obj.areas = {};
		obj.series = {};
		return obj;
	end



	function LibPie:new(width, height)
		return LibPie:init({});
	end

	function LibPie.comma(number)
		local left,num,right = string.match(number,'^([^%d]*%d)(%d+)(.-)$');
		local prettyNum = (left and left..(num:reverse():gsub('(%d%d%d)','%1,'):reverse()) or number);
		return prettyNum;
	end

--end

