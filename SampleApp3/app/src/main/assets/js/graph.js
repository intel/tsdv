/*
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

var tsdvjs = (function(args) {

    var TSDVJS = {};
	var PlotItem = function(){}

	TSDVJS.sqlFormatSec = d3.time.format("%Y-%m-%d %H:%M:%S:%LZ");
   	TSDVJS.sqlFormat = d3.time.format("%Y-%m-%d %H:%MZ");

	/**
		Logging options object
	*/
	TSDVJS.loggingOptions =
	{
		logToFile:false,
		displayRenderTime:false,
		displayFetchTime:false,
		displayFPS:false
	}

	/**
		Set function for log to file
	*/
	TSDVJS.setLogToFile = function(checked)
	{
		TSDVJS.loggingOptions.logToFile = checked;
		TSDV.setLoggingOption(LOG_TO_FILE, TSDVJS.loggingOptions.logToFile);
	}

	/**
		Display time it took to render the last frame
	*/
	TSDVJS.setDisplayRenderTime = function(checked)
	{
		TSDVJS.loggingOptions.displayRenderTime = checked;
		TSDV.setLoggingOption(DISP_RENDER_TIME, TSDVJS.loggingOptions.displayRenderTime);
	}

	/**
		Display time it took to get the last requested dataset
	*/
	TSDVJS.setDisplayFetchTime = function(checked)
	{
		TSDVJS.loggingOptions.displayFetchTime = checked;
		TSDV.setLoggingOption(DISP_FETCH_TIME, TSDVJS.loggingOptions.displayFetchTime);
	}

	/**
		Display Frames Per Second
	*/
	TSDVJS.setDisplayFPS = function(checked)
	{
		TSDVJS.loggingOptions.displayFPS = checked;
		$("#fps").toggle();
		//TODO: Set up bridge to remember settings choice.
		//TSDV.setLoggingOption(DISP_FPS, TSDVJS.loggingOptions.displayFPS);
	}

	/**
		Delete all log files except the current one
	*/
	TSDVJS.deleteOldLogs = function()
	{
		TSDV.deleteOldLogs();
	}

	TSDVJS.LOG = function(startTime, dataLength, method)
	{
		if (TSDVJS.loggingOptions.logToFile)	//Possibly add a check to only log if it takes more than 0 time?
		{
			var currDate = new Date();
			TSDV.logToFile(TSDVJS.sqlFormatSec(currDate), currDate.getTime() - startTime, dataLength, method);
		}
	}

	/* Graph object */
	TSDVJS.Graph = function(args)
	{
		if(!args)
			var args = {};

		this.data = args.data;
		this.height = args.height;
		this.width = args.width;
		this.reducedData = args.reducedData;
		this.xAxis = args.xAxis;
		this.visibleData = args.visibleData;
		this.drawGraph = function() {console.log("TSDVJS: drawGraph function is not defined for Graph object!")};	//TODO add a default graph draw
		this.plotItems = {};
		this.zoom = d3.behavior.zoom()
                    		.on("zoomstart", function() {this.zoomStart()}.bind(this))
                    		.on("zoomend", function () {this.zoomEnd();}.bind(this))
                    		.on("zoom", function () {this.zoomDraw();}.bind(this));
		this.barSize = 0;
		this.xScale = args.xScale;
		this.lastTouchEvent = 0;
		this.dblClickTime = 500;
		this.animationDuration = 500;
		this.xCoordName = args.xCoordName ? args.xCoordName : "date";
		this.zoomDirection;
		this.lastZoomTime = 0;
		this.zoomVelocity = [];
		this.panning = false;
		this.prevScale;
		this.prevTime;
		this.zoomEnabled = true;
		this.zooming = false;
		this.savedScale;
		this.savedTranslation;
		this.graphUpdateListener = undefined;		//Optional user defined callback to notify that the graph has been redrawn
		this.loadStartTime;
		this.touchPane = args.touchPane;
		this.svg = args.svg;
	}

	drawLineGraph = function(thisGraph, isAnimated)
	{
		var startTime = new Date().getTime();
		var dataLength = thisGraph.visibleData !== undefined && thisGraph.visibleData.length !== undefined ? thisGraph.visibleData.length : 0;
		var yCoord = this.name;
		var xFunc = this.xScale;
		var yFunc = this.yFunc;

		// creates the line drawing function
		this.lineFunc = d3.svg.line()
			.defined(function(d) { return (d[yCoord] !== undefined && d[yCoord] > 0); })
			.interpolate("cardinal")
			.tension(1)
			.x(function(d){ return xFunc(d[thisGraph.xCoordName]); })
			.y(function(d){ return yFunc(d[yCoord]); });

		// stop previous animations
		this.selector.transition();

		// executes the line function on the graphData
		if (isAnimated) {
			var beginLineFunc = d3.svg.line()
				.defined(function(d) { return (d[yCoord] !== undefined && d[yCoord] > 0); })
				.interpolate("cardinal")
				.tension(1)
				.x(function(d){ return xFunc(d[thisGraph.xCoordName]); })
				.y(function(d){ return thisGraph.height; });

			this.selector.attr("d", beginLineFunc(thisGraph.visibleData))
				.transition()
				.duration(thisGraph.animationDuration)
				.attr("d", this.lineFunc(thisGraph.visibleData));
		} else {
			this.selector.attr("d", this.lineFunc(thisGraph.visibleData));
		}

		TSDVJS.LOG (startTime, dataLength, "drawLineGraph()");
    }

	drawBarGraph = function(thisGraph, isAnimated)
	{
			var startTime = new Date().getTime();
			var dataLength = thisGraph.visibleData !== undefined && thisGraph.visibleData.length !== undefined ? thisGraph.visibleData.length : 0;
			var yCoord = this.name;
			var xFunc = this.xScale;
			var yFunc = this.yFunc;
			var barSize = thisGraph.barSize;
			var bars = this.selector.selectAll("." + yCoord).data(thisGraph.visibleData);

			// stop previous animations
			bars.transition();

			if (isAnimated) {
				//Draw bars being updated
				bars.attr("x", function(d) { return xFunc(d[thisGraph.xCoordName]); })
					.attr("height", function(d) { return 0; })
					.attr("width", function(d) {return barSize; })
					.attr("y", function(d) { return thisGraph.height; })
					.transition()
					.duration(thisGraph.animationDuration)
					.attr("y", function(d) { return yFunc(d[yCoord]);})
					.attr("height", function(d) { return thisGraph.height - yFunc(d[yCoord]); });

				//Draw new bars being created
				bars.enter()
					.append("rect")
					.attr("x", function(d) { return xFunc(d[thisGraph.xCoordName]); })
					.attr("height", function(d) { return 0; })
					.classed(yCoord,true)
					.attr("y", function(d) { return thisGraph.height; })
					.attr("width", function(d) { return barSize;})
					.transition()
					.duration(thisGraph.animationDuration)
					.attr("y", function(d) { return yFunc(d[yCoord]);})
					.attr("height", function(d) { return thisGraph.height - yFunc(d[yCoord]); });
			} else {
				//Draw bars being updated
				bars.attr("x", function(d) { return xFunc(d[thisGraph.xCoordName]); })
					.attr("height", function(d) { return thisGraph.height - yFunc(d[yCoord]); })
					.attr("width", function(d) { return barSize; })
					.attr("y", function(d) { return yFunc(d[yCoord]); });

				//Draw new bars being created
				bars.enter()
					.append("rect")
					.attr("x", function(d) { return xFunc(d[thisGraph.xCoordName]); })
					.attr("height", function(d) { return thisGraph.height - yFunc(d[yCoord]); })
					.classed(yCoord,true)
					.attr("y", function(d) { return yFunc(d[yCoord]); })
					.attr("width", function(d) { return barSize;});
			}
			//Delete bars that are no longer in the graph
			bars.exit().remove();

			TSDVJS.LOG (startTime, dataLength, "drawBarGraph()");
	}

	drawAxis = function(thisGraph, axisType)
	{
		var startTime = new Date().getTime();
		var dataLength = thisGraph.visibleData !== undefined && thisGraph.visibleData.length !== undefined ? thisGraph.visibleData.length : 0;

		if (axisType === "x")
			thisGraph.svg.select(".x.axis").call(thisGraph.xAxis);
		else if (axisType === "y")
			thisGraph.svg.select(".y.axis").call(thisGraph.yAxis);

		TSDVJS.LOG (startTime, dataLength, "drawAxis()");
	}

	/**
		Add an item to plot on the graph.
		selector - d3js selection or DOM obj
		name - plot item name
		type - type of graph to draw (line,bar,axis,custom, etc)
		yFunc - d3js yScale
		optionalArgs object
			customDrawFunc - a custom draw function (optional)
			classNames - any classes you want the item to have
			clipPath - clip path you want the item to follow
	*/

	TSDVJS.Graph.prototype.addPlotItem = function (name, type, yFunc, optionalArgs)
	{
		this.plotItems[name] = new PlotItem();

		if (type === "line") {
			this.plotItems[name].selector =
				this.svg.append("path")
						.attr("class","line " + optionalArgs.classNames)
						.attr("clip-path", "url(#" + optionalArgs.clipPath + ")");
        }
        else if (type === "bar") {
       		this.plotItems[name].selector =
        	this.svg.append("g")
                    .attr("class", "bar " + optionalArgs.classNames)
                    .attr("clip-path", "url(#" + optionalArgs.clipPath + ")");
        }

		this.plotItems[name].name = name;
		this.plotItems[name].type = type;
		this.plotItems[name].yFunc = yFunc;
		this.plotItems[name].visibility = true;
		this.plotItems[name].xScale = this.xScale;
		this.plotItems[name].barSize = this.barSize;

		if (type === "line")
			this.plotItems[name].draw = drawLineGraph;
		else if (type === "bar")
			this.plotItems[name].draw = drawBarGraph;
		else if (type === "axis")
			this.plotItems[name].draw = drawAxis;
		else if (type === "custom" && !!customDrawFunc)
			this.plotItems[name].draw = customDrawFunc;
	}

	/**
		Method for getting more data from the database.

		startDate - Start date for data range
		endDate - End date for data range
		metrics - Which plot item types you want data for
		numOfPoints - Requested number of data points we would like (Could get back more or less)
		callBack - Function the bridge layer will call when the results are ready.
		args - a string (can be a JSON string) that contains the args you want passed to the callBack function
	*/
	TSDVJS.Graph.prototype.getMoreData = function (startDate, endDate, metrics, numOfPoints, callback, args)
	{
		var startTime = new Date().getTime();
		var dataLength = this.visibleData !== undefined && this.visibleData.length !== undefined ? this.visibleData.length : 0;

		var defaultMetrics = ["*"];
		var noTimeFormat = d3.time.format("%A, %b %e");

		$("#current-date").text(noTimeFormat(new Date(startDate)));

		if (metrics == null) {
			metrics = defaultMetrics;
		} else if (metrics.length == 0) {
			var emptyResult = {
				"startDate" : startDate,
				"endDate" : endDate,
				"points" : []
			};

			callback(JSON.stringify(emtpyResult), args);
			return;
		}

		var reqObj =
		{
			"startDate" : TSDVJS.sqlFormat(new Date(startDate)),
			"endDate" : TSDVJS.sqlFormat(new Date(endDate)),
			"metrics" : metrics,
			"numOfPoints" : numOfPoints
		};

		this.loadStartTime = new Date().getTime();
		TSDV.loadData(JSON.stringify(reqObj), callback, args, "errorCB");
		TSDVJS.LOG (startTime, dataLength, "getMoreData()");
	}

	/**
		Given the incoming data, return the subset of the data that is currently visible
		newData - array of datapoints
		return - array of datapoints that are currently visible
	*/
	TSDVJS.Graph.prototype.findTimeRange = function(newData)
	{
		var startTime = new Date().getTime();
		var xDom = this.xScale.domain();

		//Check that any of the available data is visible before finding the range of visible data
		if (!((xDom[0] > newData[0].date && xDom[0] > newData[newData.length - 1].date) &&
			  (xDom[1] > newData[0].date && xDom[1] > newData[newData.length - 1].date)) &&
			!((xDom[0] < newData[0].date && xDom[0] < newData[newData.length - 1].date) &&
			  (xDom[1] < newData[0].date && xDom[1] < newData[newData.length - 1].date)) )
		{
			var third = Math.floor(newData.length / 3);
			var start = 0;
			var end = newData.length + 1;
			var frontDirection = newData[third].date < xDom[0] ? 1 : -1;
			var rearDirection = newData[2 * third].date < xDom[1] ? 1 : -1;
			var padStart = 0, padEnd = 0;

			if (newData.length > 2)
			{
				for (var i = third + 1; i < newData.length && i >= 0; i += frontDirection)
				{
					if (frontDirection === 1 && newData[i].date >= xDom[0] ||
						frontDirection === -1 && newData[i].date <= xDom[0])
					{
						start = i > 2 ? i - 2 : i;
						break;
					}
				}

				for (var i = (2 * third) + 1; i < newData.length && i >= 0; i += rearDirection)
				{
					if (rearDirection === 1 && newData[i].date >= xDom[1] ||
						rearDirection === -1 && newData[i].date <= xDom[1])
					{
						end = i < newData.length - 2 ? i + 2 : newData.length + 1;
						break;
					}
				}
			}

			padStart = start === 0 ? 2 : 0;
			padEnd = end === newData.length + 1 ? 2 : 0;
			newData = newData.slice(start, end);

			//Zooming changes the space bars have to draw - resize them
			if(!this.panning)
			{
			   this.barSize = this.width/(newData.length + padStart + padEnd);
			}
		}
		else    //No data currently visible
		{
			newData = [];
		}

		var dataLength = this.newData !== undefined && this.newData.length !== undefined ? this.newData.length : 0;
		TSDVJS.LOG (startTime, dataLength, "findTimeRange()");
		return newData;
	}

	/**
		Update the data on the graph and re-draw
		isAnimated - specifies if there is a transition associated with the draw
	*/
	TSDVJS.Graph.prototype.updateData = function(isAnimated)
	{
		var animated = (typeof isAnimated === "undefined") ? false : isAnimated;

		startTime = new Date().getTime();
		var dataLength = this.visibleData !== undefined && this.visibleData.length !== undefined ? this.visibleData.length : 0;

		if (TSDVJS.loggingOptions.displayRenderTime) {
			$("#statistics").append("<br> Render " + (new Date().getTime() - startTime) + " ms");
		}

		if (this.graphUpdateListener)
			this.graphUpdateListener(isAnimated);

		TSDVJS.LOG (startTime, dataLength, "updateData()");
	}

	/**
		Sets the visibility of a plot item on the graph
		itemName - name of the plot item
		visibility - bool indicating if visible or not
	*/
	TSDVJS.Graph.prototype.setItemVisibility = function(itemName, visibility, isAnimated)
	{
		isAnimated = (typeof isAnimated === "undefined") ? false : isAnimated;

		if (typeof (this.plotItems[itemName]) !== "undefined" && this.plotItems[itemName].visibility != visibility) {

			this.plotItems[itemName].visibility = visibility;

			if (!visibility) {
				if (this.plotItems[itemName].type === "line") {

					var thisItem = this.plotItems[itemName];
					this.plotItems[itemName].selector.transition()
											.duration(0)
											.each("end",
											function()	{
                                            	if(!thisItem.visibility)
                                                   	thisItem.selector.attr("d",null);
                                            }.bind(thisItem));

				} else if (this.plotItems[itemName].type === "bar") {
					this.plotItems[itemName].selector.selectAll("." + itemName).remove();
				}
			} else if (this.visibleData) {
				  this.plotItems[itemName].draw(this,isAnimated);
			}
		}
	}

	TSDVJS.Graph.prototype.drawItem = function(itemToDraw)
	{
		this.plotItems[itemToDraw].draw;
	}

	TSDVJS.Graph.prototype.drawAllItems = function(isAnimated)
	{
		var startTime = new Date().getTime();
		var dataLength = this.visibleData !== undefined && this.visibleData.length !== undefined ? this.visibleData.length : 0;

		if (this.visibleData && this.visibleData.length >= 0)
		{
			if (this.xAxis !== undefined)
				drawAxis(this, "x");

			for (var key in this.plotItems)
			{
				if (this.plotItems.hasOwnProperty(key) && this.plotItems[key].visibility === true)
				{
                    this.plotItems[key].draw(this,isAnimated);
				}
			}
		}

		TSDVJS.LOG (startTime, dataLength, "drawAllItems()");
	}

	/**
		Remove all visible data from the graph
	*/
	TSDVJS.Graph.prototype.removeAllVisibleData = function()
	{
		this.data = [];
		this.reducedData = [];
		this.visibleData = [];

		var emptyData = {date:parseDate(TSDVJS.sqlFormat(currDate[0])),steps:0, calories:0, body_temp:0, heart_rate:0, gsr:0};
		var emptyData2 = {date:parseDate(TSDVJS.sqlFormat(currDate[1])),steps:0, calories:0, body_temp:0, heart_rate:0, gsr:0};

		this.data.push(emptyData);
		this.reducedData.push(emptyData);
		this.visibleData.push(emptyData);
		this.data.push(emptyData2);
		this.reducedData.push(emptyData2);
		this.visibleData.push(emptyData2);

		if (this.xScale)
		{
			this.xScale.domain([this.data[0].date, this.data[this.data.length - 1].date]);

			if (this.zoom)
				this.zoom.x(this.xScale).scaleExtent([-1, 200]);
		}

		this.updateData(false);
	}

	/**
		NOTE: Do not add more than 1 listener to this function, otherwise the count will get broken

		When called this returns true if the last click was a double click.  Otherwise return false
	*/
	TSDVJS.Graph.prototype.dblClickCheck = function(dblClickCB)
	{
		if (d3.event !== null)
		{
			if ((d3.event.timeStamp - this.lastTouchEvent) < this.dblClickTime && d3.event.touches.length === 1)
			{
				return true;
			}

			//This is only for double tap, not zoom
			if (d3.event.touches.length === 1)
			{
				this.lastTouchEvent = d3.event.timeStamp;
			}
			else
				this.lastTouchEvent = 0;
		}
		return false;
	}

	/**
		d3js zoom graph into the given domain.
		newDomain - expected to be a two item array containing the start and end dates.
		reqAccuracy - the number of milliseconds the result needs to be within to be acceptable.
	*/
	TSDVJS.Graph.prototype.zoomToDomain = function (newDomain, reqAccuracy)
	{
		if (reqAccuracy === undefined)
			reqAccuracy = 60000;        //Set default to 1 min

		var tries = 1;
		var maxZoom = this.zoom.scaleExtent()[1];
		var zoomJump = 0.5;
		var newScale = maxZoom / 2;
		var minScale = 0, maxScale = maxZoom;
		var targetDomDist, currDomDist;
		targetDomDist = newDomain[1].getTime() - newDomain[0].getTime();

		//Keep trying until a scale is found that satisfies the reqAccuracy
		do
		{
			this.xScale.domain(newDomain);
			this.zoom.scale(newScale);
			currDomDist = this.xScale.domain()[1].getTime() - this.xScale.domain()[0].getTime();

			if (targetDomDist > currDomDist)
			{
				maxScale = newScale;
				newScale = (newScale * zoomJump) > minScale ? newScale * zoomJump : newScale - ((newScale - minScale) / 2);
			}
			else
			{
				minScale = newScale;
				newScale = newScale * (1 + zoomJump) < maxScale ? newScale * (1 + zoomJump) : ((maxScale - newScale) / 2) + newScale;
			}

			if (tries % 10 == 0 && zoomJump > 0.1)
				zoomJump = zoomJump * 0.75 >= 0.1 ? zoomJump * 0.75 : 0.1;

			tries++;
		} while(Math.abs(targetDomDist - currDomDist) > reqAccuracy)

		this.zoom.translate([-this.xScale(newDomain[0]), -this.xScale(newDomain[0])]);
		this.zooming = true;
		this.updateData();
	}

	/*
	function customDrawFuncTemplate(graphData)
	{

	}
	*/

    /**
        d3js zoom drawing function.
    */
    TSDVJS.Graph.prototype.zoomDraw = function()
    {
    if (this.zoom)
    {
    	var scale = this.zoom.scale();

               this.lastZoomTime = new Date().getTime();

        if (scale !== 1 && scale == this.prevScale)
        {
            var newTime = this.xScale.domain()[0].getTime();
            this.zoomVelocity.push(Math.abs(this.prevTime - newTime));
            this.zoomDirection = this.prevTime < newTime ? 1 : -1;
            this.panning = true;
            this.zooming = false;
            this.prevTime = newTime;
            this.lastTouchEvent = 0;	//Prevent swipe being counted as a tap for double tap check
            this.dblClick = false;
        }
        else
        {
            this.zooming = true;
            this.panning = false;
        }

        var startTime = new Date().getTime();
        var dataLength = this.visibleData !== undefined && this.visibleData.length !== undefined ? this.visibleData.length : 0;
        var translate = this.zoom.translate();
        var tx = Math.min(0, Math.max(width * (1 - scale), translate[0]));

        //prevents zooming out past the available data
        if (this.zoom.scale() < 1)
            this.zoom.scale(1);

        //prevents panning past available data
        if (tx !== translate[0])
            this.zoom.translate([tx, tx]);

        if (!this.zoomEnabled) {
            this.zoom.scale(this.savedScale);
            this.zoom.translate(this.savedTranslation);
        }

        if (this.zoom.scale() !== 1 && this.zoomEnabled) {
            this.updateData();
        }

        tsdvjs.LOG (startTime, dataLength, "zoomDraw()");
        }
    }

    /**
        d3js zoom start function.
    */
    TSDVJS.Graph.prototype.zoomStart= function(thisGraph)
    {
    	if (this.zoom)
    	{
        if (this.zoom.scale() === 1)
            return;

        this.prevScale = this.zoom.scale();
        this.zoomVelocity = [];
        this.zoomDirection = 0;

        var startTime = new Date().getTime();
        var dataLength = this.visibleData !== undefined && this.visibleData.length !== undefined ? this.visibleData.length : 0;

        this.prevTime = this.xScale.domain()[0].getTime();
        tsdvjs.LOG (startTime, dataLength, "zoomStart()");
        }
    }

    /**
        d3js zoom end function.
    */
    TSDVJS.Graph.prototype.zoomEnd = function()
    {
    	if (this.zoom)
    	{
			var thisGraph = this;
			if (this.zoom.scale() !== 1) {

				var timeSinceLastZoom = new Date().getTime() - this.lastZoomTime;
				var totalTime = this.xScale.domain()[1].getTime() - this.xScale.domain()[0].getTime();
				var totalDist = totalTime / timeSinceLastZoom;

				if (this.panning && timeSinceLastZoom < 250)
				{
					var startTime = new Date().getTime();
					var dataLength = this.visibleData !== undefined && this.visibleData.length !== undefined ? this.visibleData.length : 0;
					this.zooming = false;
					var translate = this.zoom.translate();

					this.touchPane.transition().ease("quad-out").duration(800).tween("pan", function()
					{
						var i = d3.interpolate(0, 10);
						return function(t)
						{
							var currVelocity = 0;
							var translate = thisGraph.zoom.translate();
							var velocityLen = thisGraph.zoomVelocity.length > 3 ? 3 : thisGraph.zoomVelocity.length;
							for (var v = thisGraph.zoomVelocity.length - 1; v > -1; v--)
							{
								currVelocity += thisGraph.zoomVelocity[v];
							}
							if (currVelocity !== NaN && currVelocity !== undefined && currVelocity > 0)
							{
								currVelocity = Math.floor(currVelocity / thisGraph.zoomVelocity.length);
								var dist = (currVelocity / i(t)) * thisGraph.zoomDirection;
								var newDate1 = thisGraph.xScale(new Date(thisGraph.xScale.domain()[0].getTime() + dist));
								newDate1 = translate[0] - newDate1;
								var tx = Math.min(0, Math.max(width * (1 - thisGraph.zoom.scale()),  newDate1));
								thisGraph.zoom.translate([tx, tx]);
								thisGraph.updateData();
							}
						}
					});
				}
			}

			this.panning = false;
			this.zooming = false;
			tsdvjs.LOG (startTime, dataLength, "zoomEnd()");
        }
    }

    /**
        Click and zoom events and controls
    */
    TSDVJS.Graph.prototype.enableZoom = function()
    {
        if (!this.zoomEnabled) {
            this.savedScale = null;
            this.savedTranslation = null;
            this.zoomEnabled = true;
        }
    }

    TSDVJS.Graph.prototype.disableZoom = function()
    {
        if (this.zoomEnabled) {
            this.savedScale = this.zoom.scale();
            this.savedTranslation = this.zoom.translate();
            this.zoomEnabled = false;
        }
    }

    TSDVJS.Graph.prototype.clearGraph = function()
    {
   		for (var key in this.plotItems)
    	{
			if (this.plotItems[key].type === "line") {

				var thisItem = this.plotItems[key];
				this.plotItems[key].selector.transition()
												.duration(0)
												.each("end",
													function() {
														thisItem.selector.attr("d",null);
													}.bind(thisItem));
				thisItem.selector.attr("d",null);
			} else if (this.plotItems[key].type === "bar") {
					this.plotItems[key].selector.selectAll("." + key).remove();
			}
		}
    }

	return TSDVJS;
}());
