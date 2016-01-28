var zoomDataSize = 100, fullDataSize = 10000;
var yRange = [0, 200];
var currDate,width,height,graphInstance;

function init()
{
    currDate = [new Date(2015,2,3,0,0), new Date(2015,2,4,0,0)];

	//Create the TSDV d3js graph object
    graphInstance = new tsdvjs.Graph();

    width = $(document).width();
    height = $(document).height();

    //Create the graph's SVG
    graphInstance.svg = d3.select("#chart").append("svg")
        .attr("width", width)
        .attr("height", height);

    graphInstance.xScale = d3.time.scale().range([0, width]);
	graphInstance.yScale = d3.scale.linear().range([height, 50]);

    //Set up d3js x and y axis scales
	graphInstance.xAxis = d3.svg.axis()
		.scale(graphInstance.xScale)
		.orient("top")
		.ticks(4)
		.tickFormat(formatTick);

	graphInstance.yAxis = d3.svg.axis()
		.scale(graphInstance.yScale)
		.orient("right")
		.tickSize(-width)
		.tickPadding(6);

    //Add d3js x and y axis to the SVG
	graphInstance.svg.append("g")
		.attr("class", "y axis")
		.attr("transform", "translate(" + (width - 35) + ",0)");

	graphInstance.svg.append("g")
		.attr("class", "x axis")
		.attr("transform", "translate(-30, 20 )");

    //Create clip path to keep graph from covering the y axis number
	graphInstance.svg.append("clipPath")
		.attr("id", "clip")
		.append("rect")
		.attr("x", graphInstance.xScale(0))
		.attr("y", graphInstance.yScale(1))
		.attr("width", width - 30)
		.attr("height", graphInstance.yScale(0) - graphInstance.yScale(1));

	graphInstance.height = height;
	graphInstance.width = width;

	graphInstance.addPlotItem( "heart_rate", "line", graphInstance.yScale, {classNames:"heartRateLine", clipPath:"clip"});
    graphInstance.addPlotItem("steps", "bar", graphInstance.yScale, {classNames:"stepsGroup", clipPath:"clip"});
    graphInstance.addPlotItem("calories", "bar", graphInstance.yScale, {classNames:"caloriesGroup", clipPath:"clip"});
	graphInstance.addPlotItem("body_temp", "line", graphInstance.yScale, {classNames:"bodyTempLine", clipPath:"clip"});
	graphInstance.addPlotItem("blood_sugar", "line", graphInstance.yScale, {classNames:"bloodsugarLine", clipPath:"clip"});

	graphInstance.graphUpdateListener = graphUpdated;

    //Specify the part of the screen to listen for touch and zoom events on
	graphInstance.touchPane = graphInstance.svg.append("rect")
		.attr("id", "touchPane")
		.attr("width", width)
		.attr("height", height)
		.on("touchstart", function() {
			if (graphInstance.dblClickCheck())
			{
				//TODO Check if an event is currently selected, if so zoom into it
				zoomToFullDay();
			}
		})
		.call(graphInstance.zoom);

	graphInstance.yScale.domain(yRange);
	graphInstance.svg.select("g.y.axis").call(graphInstance.yAxis);

	//Raise the 0 up a bit so it doesn't go offscreen
	graphInstance.svg.select("g.y.axis .tick text").style({"transform": "translateY(-5px)"});
	graphInstance.getMoreData(currDate[0], currDate[1], null, zoomDataSize, "newDataReceived", "Reduced");
}


/**
    Converts date string to Javascript Date object
 */
function parseDate(dateString)
{
    var t = dateString.replace(/\Z/, "").split(/[- :]/);
    var date = new Date(t[0], t[1]-1, t[2], t[3] || 0, t[4] || 0, t[5] || 0);

    var newHour = date.getHours();
    var oldHour = parseInt(t[3] || 0);

    // Hack to adjust hours due to daylight saving changes
    if (newHour !== oldHour) {
        var diff = oldHour - newHour;
        newHour > oldHour ? date.setHours(date.getHours() - 1 + diff) : date.setHours(date.getHours() -1 + diff);
    }

    return date;
}

/**
    Formats the d3js tick text
*/
function formatTick(time)
{
    //Format the tick times
    var hours = time.getHours();
    var minutes = time.getMinutes();
    var zeroMin = minutes < 10 ? "0" : "";
    var amPm;

    if (hours > 12)
    {
        hours -= 12;
        amPm = "p";
    }
    else
        amPm = hours === 12 ? "p" : "a";

    hours = hours === 0 ? 12 : hours;
    var tickTime = hours + ":" + zeroMin + minutes + amPm;

    return tickTime;
}

function zoomToFullDay()
{
    if (d3.event !== null)
    {
        d3.event.stopImmediatePropagation();
        graphInstance.zooming = true;
        graphInstance.xScale.domain([parseDate(tsdvjs.sqlFormat(currDate[0])), parseDate(tsdvjs.sqlFormat(currDate[1]))]);
        graphInstance.zoom.x(graphInstance.xScale).scaleExtent([-1, 200]);
        graphUpdated(false);
        graphInstance.zooming = false;
    }
}

function getNextDay()
{
    graphInstance.clearGraph();
    currDate[0].setDate(currDate[0].getDate() + 1);
    currDate[1].setDate(currDate[1].getDate() + 1);
    graphInstance.getMoreData(currDate[0], currDate[1], null, zoomDataSize, "newDataReceived", "Reduced");
}

function getPrevDay()
{
    graphInstance.clearGraph();
    currDate[0].setDate(currDate[0].getDate() - 1);
    currDate[1].setDate(currDate[1].getDate() - 1);
    graphInstance.getMoreData(currDate[0], currDate[1], null, zoomDataSize, "newDataReceived", "Reduced");
}

/**
    Callback function for when new data has been retrieved from the database
*/
function newDataReceived(jsonData, args)
{
    var parsedData;

    if (!jsonData || jsonData.trim().length === 0) return;

    try
    {
        parsedData = JSON.parse(jsonData);
    } catch (e)
    {
        console.log("Invalid json string: " + jsonData);
        return;
    }

    if (parsedData.startDate === undefined ||
        parsedData.endDate === undefined ||
        parsedData.points === undefined)
    {
        console.log("Cannot parse malformed response: " + jsonData);
        return;
    }

    var newData = parsedData.points;

    if (args === "Reduced")
    {
        graphInstance.getMoreData(currDate[0], currDate[1], null, fullDataSize, "newDataReceived", "Full");
    }

    if (newData.length && newData.length > 0)
    {
        newData.forEach(function(d) {
            d.date = parseDate(d.date);
        });

        graphInstance.xScale.domain([parseDate(tsdvjs.sqlFormat(currDate[0])), parseDate(tsdvjs.sqlFormat(currDate[1]))]);

        if (args === "Full")
        {
            graphInstance.data = newData;
        }
        else if (args === "Reduced")
        {
            graphInstance.reducedData = newData;
            graphUpdated(true);
        }

        graphInstance.zoom.x(graphInstance.xScale).scaleExtent([-1, 200]);
    }
    else
    {
        graphInstance.removeAllVisibleData();
    }
}

/**
	Callback function for when the data visible by the graph has been changed.
	This could happen because a new set of data has been loaded or a zooming / panning
	event has occurred.
*/
function graphUpdated(isAnimated)
{
    if ((graphInstance.reducedData && graphInstance.reducedData.length > 0) && ((graphInstance.xScale.domain()[1] - graphInstance.xScale.domain()[0]) / 60000 > zoomDataSize))
    {
        graphInstance.visibleData = graphInstance.findTimeRange(graphInstance.reducedData);
	}
	else if ((graphInstance.data && graphInstance.data.length > 0) && ((graphInstance.xScale.domain()[1] - graphInstance.xScale.domain()[0]) / 60000 < zoomDataSize))
	{
        graphInstance.visibleData = graphInstance.findTimeRange(graphInstance.data);
	}

    graphInstance.drawAllItems(isAnimated);
}

$(document).ready(init);