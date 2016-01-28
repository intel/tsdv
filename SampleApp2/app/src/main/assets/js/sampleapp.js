/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
var sqlFormat = d3.time.format("%Y-%m-%d %H:%MZ");
var visibleData;
var zoomEnabled = true;
var zoomDirection;

var margin = {top: 20, right: 20, bottom: 85, left: 50};
var width = $(window).width()- margin.left - margin.right;
var height = $(window).height() - margin.top - margin.bottom;

var previousZoomScale = 1;
var previousStartTime;
var previousEndTime;
var visibleStartTime;
var visibleEndTime;

var loadStartTime,loadTime;

var x, y, zoom, svg, xAxis, yAxis, candlestick;
var first = true;

// We've decided a factor of 2 provides an acceptable level of variance when the number of points are calculated.
// Therefore, our downsampling levels are all at most doubling the previous level.

var downsamplingLevels = {};

// 1 month
downsamplingLevels[2629746000] = 31;
// 2 months
downsamplingLevels[5256900000] = 31;
// 3 months
downsamplingLevels[7889000000] = 40;
// 6 months
downsamplingLevels[15778476000] = 40;
// 1 year
downsamplingLevels[31556952000] = 70 ;
// 2 years
downsamplingLevels[63113904000] = 70 ;
// 4 years
downsamplingLevels[126227808000] = 70 ;
// 5 years
downsamplingLevels[157784760000] = 100 ;
// 10 years
downsamplingLevels[315569520000] = 200 ;

// Note: These keys are sorted for calculateTimeThreshold
var downsamplingKeys = Object.keys(downsamplingLevels).sort(function(a,b){return a - b});

var currentThresholdLevel = 315569520000;


function newDataReceived(jsonData, direction)
{
    var loadTime = new Date().getTime() - loadStartTime;
    if (!jsonData || jsonData.trim().length === 0) return;

    try {
        var parsedData = JSON.parse(jsonData);
    } catch (e) {
        console.log("Invalid json string: " + jsonData);
        return;
    }

    var accessor = candlestick.accessor();

    var parseDate = d3.time.format("%Y-%m-%d %H:%MZ").parse;

    var data = parsedData.points.map(function(d) {
        return {
          date: parseDate(d.Date),
          open: +d.Open,
          high: +d.High,
          low: +d.Low,
          close: +d.Close,
          volume: +d.Volume
        };
    }).sort(function(a, b) { return d3.ascending(accessor.d(a), accessor.d(b)); });

    y.domain(techan.scale.plot.ohlc(data, accessor).domain());

    if(first){
    // First time getting data
        x.domain(data.map(accessor.d));
        y.domain(techan.scale.plot.ohlc(data, accessor).domain());

     // Associate the zoom with the scale after a domain has been applied
        zoom.x(x.zoomable().clamp(false));
        var dbStart = data[0].date.getTime();//1104739200000;
        var dbEnd = data[data.length - 1].date.getTime();//1446710400000;
        getMoreData(dbStart, dbEnd, null, downsamplingLevels[currentThresholdLevel], currentThresholdLevel, "newDataReceived");

        first = false;
        return;
    }
    svg.select("g.candlestick").datum(data);
    visibleData = data;
    $("#statistics").text(loadTime + " ms to load");
    draw();
}


function setupGraph()
{
    width = $(window).width()- margin.left - margin.right;
    height = $(window).height() - margin.top - margin.bottom;

    x = techan.scale.financetime()
        .range([0, width]);


    y = d3.scale.linear()
        .range([height, 0]);

    zoom = d3.behavior.zoom()
        .on("zoomend", zoomEnd)
        .on("zoom", zoomDraw);


    candlestick = techan.plot.candlestick()
        .xScale(x)
        .yScale(y);

    xAxis = d3.svg.axis()
        .scale(x)
        .ticks(4)
        .orient("bottom");


    yAxis = d3.svg.axis()
        .scale(y)
        .orient("left");

    svg = d3.select("#chart").append("svg")
        .attr("width", width + margin.left + margin.right)
        .attr("height", height + margin.top + margin.bottom)
        .append("g")
        .attr("transform", "translate(" + margin.left + "," + margin.top + ")");

    svg.append("clipPath")
        .attr("id", "clip")
        .append("rect")
        .attr("x", 0)
        .attr("y", y(1))
        .attr("width", width)
        .attr("height", y(0) - y(1));

    svg.append("g")
        .attr("class", "candlestick")
        .attr("clip-path", "url(#clip)");

    svg.append("g")
        .attr("class", "x axis")
        .attr("transform", "translate(0," + height + ")");

    svg.append("g")
        .attr("class", "y axis")
        .append("text")
        .attr("transform", "rotate(-90)")
        .attr("y", 6)
        .attr("dy", ".71em")
        .style("text-anchor", "end")
        .text("Price ($)");

    svg.append("rect")
        .attr("class", "pane")
        .attr("width", width)
        .attr("height", height)
        .call(zoom);
}

function draw() {
    svg.select("g.candlestick").call(candlestick);
    svg.select("g.x.axis").call(xAxis);
    svg.select("g.y.axis").call(yAxis);

}

function zoomDraw()
{
    draw();
}

function zoomEnd()
{
    var zoomFactor = zoom.scale() / previousZoomScale;
    previousZoomScale = zoom.scale();
    var visibleStartTime = x.domain()[0].getTime();
    var visibleEndTime = x.domain()[x.domain().length - 1].getTime();

    // Calculate visible time
    var visibleTime = visibleEndTime - visibleStartTime;

    // Figure out where the current visible time falls in the threshold levels
    var desiredThreshold = calculateTimeThreshold(visibleTime, zoomFactor);

    var numPoints = Math.floor(visibleTime / desiredThreshold * downsamplingLevels[desiredThreshold]);

    // Compare if that is different than the current threshold level
    // If so, get more data.
    if (desiredThreshold != currentThresholdLevel || zoomFactor <= 1){
        getMoreData(visibleStartTime, visibleEndTime, null, numPoints, desiredThreshold, "newDataReceived");
        currentThresholdLevel = desiredThreshold;
    }

}


function getMoreData(startDate, endDate, metrics, numOfPoints, direction, callback)
{
    var defaultMetrics = ["*"];

    if (metrics == null) {
        metrics = defaultMetrics;
    } else if (metrics.length == 0) {
        var emptyResult = {
            "startDate" : startDate,
            "endDate" : endDate,
            "points" : []
        };

        newDataReceived(JSON.stringify(emptyResult), direction);
        return;
    }

    var reqObj =
    {
        "startDate" : sqlFormat(new Date(startDate)),
        "endDate" : sqlFormat(new Date(endDate)),
        "metrics" : metrics,
        "numOfPoints" : numOfPoints
    };

    loadStartTime = new Date().getTime();
    TSDV.loadData(JSON.stringify(reqObj), callback, direction, "errorCB");
}

function errorCB(error)
{
    console.log("TSDV get data error : " + error);
}



function calculateTimeThreshold(visibleTimeMs, zoom){
    // zooming in
    if(zoom > 1){
        for(var i = downsamplingKeys.length-1; i >=0; i--){
            if(visibleTimeMs < downsamplingKeys[i]){
                continue;
            }
            else return downsamplingKeys[i];
        }
        // If we've made it all the way through the list return the smallest threshold
        return downsamplingKeys[0];
    }

    // zooming out
    if(zoom < 1){
        var ret;
        for(var i = 0; i < downsamplingKeys.length-1; i++){
            ret = downsamplingKeys[i];
            if(visibleTimeMs > downsamplingKeys[i]){
              // It's possible for us to try to zoom to an area bigger than the data
              continue;

            }
            else break;
        }
        return ret;
    }

    if(zoom == 1){
        return currentThresholdLevel;
    }
}

function testime(){
    var range = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    for (var i = 0; i < range.length; i++) {
        range[i] = mapRange([0, 10], [-1, 0], range[i]);
    }

}


var mapRange = function(from, to, s) {
    return to[0] + (s - from[0]) * (to[1] - to[0]) / (from[1] - from[0]);
};
