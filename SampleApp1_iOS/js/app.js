/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

var zoomDataSize = 100,
    fullDataSize = 10000;
var yRangeLeft = [0, 200];
var yRangeRight = [0, 20];
var xAxis, yAxisLeft, yAxisRight, heartRateLine, bodyTempLine, bloodsugarLine, stepsLine, caloriesLine, svg, pane, steps, stepsGroup;
var touchStartX, touchStartY, touchMoveX, touchMoveY;
var svgSteps;
var graphInstance = new tsdvjs.Graph();
var sqlFormat = d3.time.format("%Y-%m-%d %H:%MZ");
var sqlFormatSec = d3.time.format("%Y-%m-%d %H:%M:%S:%LZ");
var bisectDate = d3.bisector(function(d) {
    return d.date;
}).left;
var LOG_TO_FILE = "LOGGING_ENABLED";
var DISP_RENDER_TIME = "DISPLAY_RENDER_TIME";
var DISP_FETCH_TIME = "DISPLAY_FETCH_TIME";
var actionDataArray;
var activityLoadStartTime;
var currSelectedActivity;

/**
    Converts date string to Javascript Date object
 */
function parseDate(dateString) {
    var t = dateString.replace(/\Z/, "").split(/[- :]/);
    var date = new Date(t[0], t[1] - 1, t[2], t[3] || 0, t[4] || 0, t[5] || 0);

    var newHour = date.getHours();
    var oldHour = parseInt(t[3] || 0);

    // Hack to adjust hours due to daylight saving changes
    if (newHour !== oldHour) {
        var diff = oldHour - newHour;
        newHour > oldHour ? date.setHours(date.getHours() - 1 + diff) : date.setHours(date.getHours() - 1 + diff);
    }

    return date;
}

/**
    Formats the d3js tick text
*/
function formatTick(time) {
    //Format the tick times
    var hours = time.getHours();
    var minutes = time.getMinutes();
    var zeroMin = minutes < 10 ? "0" : "";
    var amPm;

    if (hours > 12) {
        hours -= 12;
        amPm = "p";
    } else
        amPm = hours === 12 ? "p" : "a";

    hours = hours === 0 ? 12 : hours;
    var tickTime = hours + ":" + zeroMin + minutes + amPm;

    return tickTime;
}

function errorCB(error) {
    console.log("TSDV get data error : " + error);
}

function drawActivity(activityData) {
    var startDate = new Date(activityData.start);
    var endDate = new Date(activityData.end);
    var xDom = graphInstance.xScale.domain();

    if ((startDate < xDom[0] && endDate < xDom[0]) ||
        (startDate > xDom[1] && endDate > xDom[1])) {
        return;
    }

    var activityStart = startDate > xDom[0] ? startDate : xDom[0];
    var activityEnd = endDate < xDom[1] ? endDate : xDom[1];

    var startPosition = startDate < xDom[0] ? margin.left : x(parseDate(sqlFormat(activityStart))) + margin.left;
    var endPosition = endDate > xDom[1] ? ($(window).width() - margin.right) : x(parseDate(sqlFormat(activityEnd))) + margin.left;

    var activityOverlay = d3.select("#chart")
        .append("div")
        .attr("id", "activity-" + sqlFormat(activityData.start))
        .attr("class", "activityOverlay")
        .style("top", $("#chart").position().top + "px")
        .style("left", startPosition + "px")
        .style("right", $(window).width() - endPosition + "px")
        .style("height", this.height + this.margin.top - 2 + "px");

    activityOverlay.append("div")
        .attr("class", "activityOverlayBar");
    activityOverlay.append("div")
        .attr("class", "activityOverlayLabel")
        .text(activityData.type);

    if (currSelectedActivity && activityOverlay[0][0].id === currSelectedActivity) {
        activityOverlay.classed("selected", true);
        activityOverlay.select(".activityOverlayBar").classed("selected", true);
        activityOverlay.select(".activityOverlayLabel").classed("selected", true);
    }
}

function drawAllActivities(allActivityData) {
    var startTime = new Date().getTime();
    var dataLength = allActivityData !== undefined && allActivityData.length !== undefined ? allActivityData.length : 0;

    d3.selectAll(".activityOverlay").remove();

    if (allActivityData && allActivityData.length > 0) {
        allActivityData.forEach(function(d) {
            drawActivity(d);
        });
    }

    tsdvjs.LOG(startTime, dataLength, "drawActivity()");
}

function getActivityData(startDate, endDate) {
    var reqObj = {
        "startDate": sqlFormat(new Date(startDate)),
        "endDate": sqlFormat(new Date(endDate))
    };


    activityLoadStartTime = new Date().getTime();

    if (reqObj.startDate == "2015-03-03 00:00Z") {
        newActivityDataReceived("[{" +
            "\"type\": \"walk\"," +
            "\"start\": \"2015-03-03 07:30:09Z\"," +
            "\"end\": \"2015-03-03 07:40:47Z\"," +
            "\"calories\": 88.7," +
            "\"heart_rate\": 119.15135135135137," +
            "\"steps\": 617.0" +
            "},{" +
            "\"type\": \"run\"," +
            "\"start\": \"2015-03-03 08:49:52Z\"," +
            "\"end\": \"2015-03-03 09:56:33Z\"," +
            "\"calories\": 95.3," +
            "\"heart_rate\": 130.5609756097561," +
            "\"steps\": 640.0" +
            "},{" +
            "\"type\": \"bike\"," +
            "\"start\": \"2015-03-03 11:50:37Z\"," +
            "\"end\": \"2015-03-03 12:30:49Z\"," +
            "\"calories\": 145.2," +
            "\"heart_rate\": 125.3904109589041," +
            "\"steps\": 1051.0" +
            "},{" +
            "\"type\": \"activity\"," +
            "\"start\": \"2015-03-03 15:10:47Z\"," +
            "\"end\": \"2015-03-03 16:23:07Z\"," +
            "\"calories\": 182.4," +
            "\"heart_rate\": 132.22238163558106," +
            "\"steps\": 1197.0" +
            "}]");
    }
}

function activitySelected(activityID) {
    console.log("User selected activity " + activityID);
    TSDV.emitSignal("ActivitySelected", JSON.stringify({
        "activityID": activityID
    }));
}

function appLoaded() {
    console.log("App has loaded");
    TSDV.emitSignal("AppLoaded", "true");
}

function getNextDay() {
    graphInstance.clearGraph();
    currDate[0].setDate(currDate[0].getDate() + 1);
    currDate[1].setDate(currDate[1].getDate() + 1);
    graphInstance.loadReducedStartDate = new Date().getTime();
    graphInstance.getMoreData(currDate[0], currDate[1], null, zoomDataSize, "newDataReceived", "Reduced");
    d3.selectAll(".activityOverlay").remove();
}

function getPrevDay() {
    graphInstance.clearGraph();
    currDate[0].setDate(currDate[0].getDate() - 1);
    currDate[1].setDate(currDate[1].getDate() - 1);
    graphInstance.loadReducedStartDate = new Date().getTime();
    graphInstance.getMoreData(currDate[0], currDate[1], null, zoomDataSize, "newDataReceived", "Reduced");
    d3.selectAll(".activityOverlay").remove();
}

/**
    Highlight or un-highlight an activity on the graph. If the item has a callback,
    execute it.
*/

function toggleActivity(positionX, positionY, callBack) {
    var activityFound = false;

    d3.selectAll(".activityOverlay").each(function(d, i) {
        if (positionX >= this.getBoundingClientRect().left &&
            positionX <= this.getBoundingClientRect().right) {
            activityFound = true;

            currSelectedActivity = this.id;

            if (d3.select(this).classed("selected")) {
                return;
            }

            d3.selectAll(".activityOverlay").classed("selected", false);
            d3.selectAll(".activityOverlayBar").classed("selected", false);
            d3.selectAll(".activityOverlayLabel").classed("selected", false);
            d3.select(this).classed("selected", true);
            d3.select(this).select(".activityOverlayBar").classed("selected", true);
            d3.select(this).select(".activityOverlayLabel").classed("selected", true);

            if (callBack && typeof callBack === "function") {
                callBack(this.id);
            }
        }
    });

    if (!activityFound) {
        currSelectedActivity = "";
        d3.selectAll(".activityOverlay").classed("selected", false);
        d3.selectAll(".activityOverlayBar").classed("selected", false);
        d3.selectAll(".activityOverlayLabel").classed("selected", false);
    }
}

function click() {
    toggleActivity(d3.mouse(this)[0], d3.mouse(this)[1], activitySelected);
}

function touchStart() {
    touchStartX = touchMoveX = d3.mouse(this)[0];
    touchStartY = touchMoveY = d3.mouse(this)[1];

    if (graphInstance.isLegendVisible()) {
        graphInstance.closeLegend();
        graphInstance.enableZoom();
    }
}

function touchEnd() {
    if (graphInstance.isLegendVisible()) {
        setTimeout(function() {
            graphInstance.hideLegend(function() {
                graphInstance.enableZoom();
            });
        }, 3000);
    }
}

function touchMove() {
    graphInstance.lastTouchEvent = 0; //Previous touch shouldn't count towards double tap
    touchMoveX = d3.mouse(this)[0];
    touchMoveY = d3.mouse(this)[1];

    if (graphInstance.isLegendVisible() && !graphInstance.zoomEnabled) {
        graphInstance.moveLegend(d3.mouse(this)[0], $("#chart").position().top);
    }
}

function zoomToFullDay() {
    if (d3.event !== null) {
        d3.event.stopImmediatePropagation();
        graphInstance.zooming = true;
        graphInstance.xScale.domain([parseDate(sqlFormat(currDate[0])), parseDate(sqlFormat(currDate[1]))]);
        graphInstance.zoom.x(x).scaleExtent([-1, 200]);
        graphInstance.updateData();
        drawAllActivities(actionDataArray);
        graphInstance.zooming = false;
    }
}

function init() {
    var startTime = new Date().getTime();
    var logEnabled = TSDV.loggingOptionEnabled(LOG_TO_FILE);
    $("#logToFile").attr("checked", logEnabled);
    tsdvjs.setLogToFile(logEnabled);

    if (TSDV.loggingOptionEnabled(DISP_RENDER_TIME)) {
        $("#displayRenderTime").attr("checked", true);
        tsdvjs.loggingOptions.displayRenderTime = true;
    }

    if (TSDV.loggingOptionEnabled(DISP_FETCH_TIME)) {
        $("#displayFetchTime").attr("checked", true);
        tsdvjs.loggingOptions.displayFetchTime = true;
    }

    xAxis = d3.svg.axis()
        .scale(x)
        .orient("top")
        .ticks(4)
        .tickFormat(formatTick);

    yAxisLeft = d3.svg.axis()
        .scale(yLeft)
        .orient("left")
        .tickSize(-width)
        .tickPadding(6);

    yAxisRight = d3.svg.axis()
        .scale(yRight)
        .orient("right")
        .tickSize(-width)
        .tickPadding(6);

    svg = d3.select("#chart").append("svg")
        .attr("width", width + margin.left + margin.right)
        .attr("height", height + margin.top + margin.bottom)
        .append("g")
        .attr("id", "mainG")
        .attr("transform", "translate(" + margin.left + ",0)");

    var topPos = height + margin.top + margin.bottom + 10 + "px";

    $("#buttonRow").css({
        position: "absolute",
        top: topPos
    });

    svg.append("clipPath")
        .attr("id", "clip")
        .append("rect")
        .attr("x", x(0))
        .attr("y", yLeft(1))
        .attr("width", x(1) - x(0))
        .attr("height", yLeft(0) - yLeft(1));

    svg.append("clipPath")
        .attr("id", "clip2")
        .append("rect")
        .attr("x", x(0))
        .attr("y", yLeft(1))
        .attr("width", x(1) - x(0))
        .attr("height", yLeft(0) - yLeft(1));

    svg.append("g")
        .attr("class", "y axis left");

    svg.append("g")
        .attr("class", "y axis right")
        .attr("transform", "translate(" + width + ",0)");

    svg.append("g")
        .attr("class", "x axis")
        .attr("transform", "translate(0, 20 )");

    var steps = svg.append("path")
        .attr("class", "stepsLine")
        .classed("cline", true)
        .attr("clip-path", "url(#clip2)");

    var cal = svg.append("path")
        .attr("class", "caloriesLine")
        .classed("cline", true)
        .attr("clip-path", "url(#clip)");

    var hr = svg.append("path")
        .attr("class", "heartRateLine")
        .classed("cline", true)
        .attr("clip-path", "url(#clip)");

    var bt = svg.append("path")
        .attr("class", "bodyTempLine")
        .classed("cline", true)
        .attr("clip-path", "url(#clip2)");

    var blood_sugar = svg.append("path")
        .attr("class", "bloodsugarLine")
        .classed("cline", true)
        .attr("clip-path", "url(#clip2)");

    graphInstance.xScale = x;
    graphInstance.zoomDataSize = zoomDataSize;
    graphInstance.xAxis = xAxis;
    graphInstance.yAxisLeft = yAxisLeft;
    graphInstance.yAxisRight = yAxisRight;
    graphInstance.height = height;
    graphInstance.width = width;

    graphInstance.addPlotItem(hr, "heart_rate", "line", yLeft);
    graphInstance.addPlotItem(steps, "steps", "line", yLeft);
    graphInstance.addPlotItem(cal, "calories", "line", yRight);
    graphInstance.addPlotItem(bt, "body_temp", "line", yLeft);
    graphInstance.addPlotItem(blood_sugar, "blood_sugar", "line", yLeft);
    graphInstance.graphUpdateListener = graphUpdated;
    graphInstance.addLegend();

    $("#chart").on("taphold", function(e) {
        if (touchMoveX !== "undefined" &&
            touchMoveY !== "undefined" &&
            Math.abs(touchMoveX - touchStartX) > 5 || Math.abs(touchMoveY - touchStartY) > 5) {
            return;
        }

        graphInstance.showLegend(touchStartX, $("#chart").position().top, function() {
            graphInstance.disableZoom();
        });
    });

    graphInstance.drag = d3.select("#chart")
        .on("click", click)
        .on("touchstart", touchStart)
        .on("touchend", touchEnd)
        .on("touchmove", touchMove);

    graphInstance.zoom = d3.behavior.zoom()
        .on("zoomstart", function() {
            graphInstance.zoomStart()
        })
        .on("zoomend", function() {
            graphInstance.zoomEnd();
        })
        .on("zoom", function() {
            graphInstance.zoomDraw();
        });

    pane = svg.append("rect")
        .attr("class", "pane")
        .attr("width", width)
        .attr("height", height)
        .on("touchstart", function() {
            if (graphInstance.dblClickCheck()) {
                //TODO Check if an event is currently selected, if so zoom into it
                zoomToFullDay();
            }
        })
        .call(graphInstance.zoom);

    yLeft.domain(yRangeLeft);
    yRight.domain(yRangeRight);

    //Draw y axis - only need to do once
    svg.select("g.y.axis.left").call(yAxisLeft);
    svg.select("g.y.axis.right").call(yAxisRight);

    //Raise the 0 up a bit so it doesn't go offscreen
    svg.select("g.y.axis.left .tick text").style({
        "transform": "translateY(-5px)"
    });
    svg.select("g.y.axis.right .tick text").style({
        "transform": "translateY(-5px)"
    });

    var dataLength = graphInstance.visibleData !== undefined && graphInstance.visibleData.length !== undefined ? graphInstance.visibleData.length : 0;
    tsdvjs.LOG(startTime, dataLength, "init()");
}

/**
    Callback function for when new data has been retrieved from the database

    This might be able to be moved to the Generic library
*/
function newDataReceived(jsonData, args) {
    var startTime = new Date().getTime();
    var dataLength = graphInstance.visibleData !== undefined && graphInstance.visibleData.length !== undefined ? graphInstance.visibleData.length : 0;
    var parsedData;

    if (!jsonData || jsonData.trim().length === 0) return;

    try {
        parsedData = JSON.parse(jsonData);
    } catch (e) {
        console.log("Invalid json string: " + jsonData);
        return;
    }

    if (parsedData.startDate === undefined ||
        parsedData.endDate === undefined ||
        parsedData.points === undefined) {
        console.log("Cannot parse malformed response: " + jsonData);
        return;
    }

    var newData = parsedData.points;

    tsdvjs.LOG(graphInstance.loadStartTime, newData.length, "loadData() - Javascript to Native layer");

    if (args === "Full") {
        var loadTime = new Date().getTime() - graphInstance.loadFullStartDate;
    } else {
        var loadTime = new Date().getTime() - graphInstance.loadReducedStartDate;
        graphInstance.barSize = graphInstance.width / newData.length;
        graphInstance.loadFullStartDate = new Date().getTime();
        graphInstance.getMoreData(currDate[0], currDate[1], null, fullDataSize, "newDataReceived", "Full");
    }

    if (newData.length && newData.length > 0) {
        newData.forEach(function(d) {
            d.date = parseDate(d.date);
        });

        x.domain([parseDate(sqlFormat(currDate[0])), parseDate(sqlFormat(currDate[1]))]);

        if (args === "Full") {
            graphInstance.timeToLoadFullData = loadTime;
            graphInstance.data = newData;
        } else if (args === "Reduced") {
            graphInstance.timeToLoadReducedData = loadTime;
            graphInstance.reducedData = newData;
            graphInstance.updateData(true);
        }

        graphInstance.zoom.x(x).scaleExtent([-1, 200]);
    } else {
        graphInstance.removeAllVisibleData();
    }

    tsdvjs.LOG(startTime, dataLength, "newDataReceived()");
}

function graphUpdated(isAnimated) {
    $("#statistics").text("");
    if ((graphInstance.reducedData && graphInstance.reducedData.length > 0) && ((graphInstance.xScale.domain()[1] - graphInstance.xScale.domain()[0]) / 60000 > zoomDataSize)) {
        if (tsdvjs.loggingOptions.displayFetchTime) {
            $("#statistics").text("REDUCED");
            $("#statistics").append("<br>" + graphInstance.timeToLoadReducedData + " ms to load from DB");
        }
        graphInstance.visibleData = graphInstance.findTimeRange(graphInstance.reducedData);
    } else if ((graphInstance.data && graphInstance.data.length > 0) && ((graphInstance.xScale.domain()[1] - graphInstance.xScale.domain()[0]) / 60000 < zoomDataSize)) {
        if (tsdvjs.loggingOptions.displayFetchTime) {
            $("#statistics").text("FULL");
            $("#statistics").append("<br>" + graphInstance.timeToLoadFullData + " ms to load from DB");
        }
        graphInstance.visibleData = graphInstance.findTimeRange(graphInstance.data);
    }

    graphInstance.drawAllItems(isAnimated);
    drawAllActivities(actionDataArray);
}

function newActivityDataReceived(jsonData) {
    var activityData;

    if (!jsonData || jsonData.trim().length === 0) return;

    try {
        activityData = JSON.parse(jsonData);
    } catch (e) {
        console.log("Invalid json string: " + jsonData);
        return;
    }

    tsdvjs.LOG(activityLoadStartTime, activityData.length, "loadActivityData() - Javascript to Native layer");

    if (activityData.length && activityData.length > 0) {
        activityData.forEach(function(d) {
            d.start = d3.time.format("%Y-%m-%d %H:%M:%SZ").parse(d.start);
            d.end = d3.time.format("%Y-%m-%d %H:%M:%SZ").parse(d.end);
        });

        actionDataArray = activityData;
        drawAllActivities(actionDataArray);
    }
}
