if (not _G.LibPie) then
	_G.LibPie = {};

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
			table.insert(tmp, string.format("%f,%f", v[1],v[2]));
		end
		return table.concat(tmp, " ");
	end

	function LibPie:newLineGraph(object)
		local root = self;
		local child = {};
		child.type = "linegraph";
		child.points = {};
		child.data = {};
		child.addPoint = LibPie.addPoint;
		child.printPoints = LibPie.printPoints;
		table.insert(root.children, child);
		return true;
	end

	function LibPie:init(obj)
		obj.children = newTable();
		obj.newLineGraph = LibPie.newLineGraph
	end

	function LibPie:new(width, height)
		local obj = LibPie:init(newTable());
		return obj;
	end

end

