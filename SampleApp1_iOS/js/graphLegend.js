/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

/**
 * This is a demo of how to expand the capabilities of TSDVJS
 * Here we are creating a small popup that provides all the data
 * of the exact point the user is putting their finger on a touchhold event
 */

var tsdvjs = (function(TSDVJS) {

    /**
        Add legend data for plot item types
    */
    TSDVJS.Graph.prototype.addLegend = function() {
        this.vertical = d3.select("#chart")
            .append("div")
            .attr("class", "verticalLineOverlay")
            .style("opacity", 0)
            .style("top", "0px")
            .style("left", "0px");

        $(".verticalLineOverlay").css({
            height: this.height
        });

        this.legend = d3.select("#chart")
            .append("div")
            .attr("class", "legendOverlay")
            .style("opacity", 0)
            .style("top", "0px")
            .style("left", "0px");

        this.legend.append("table").append("tbody");
    }

    /**
        Show legend
    */
    TSDVJS.Graph.prototype.showLegend = function(positionX, positionY, callback) {
        if (this.legend !== "undefined") {
            this.moveLegend(positionX, positionY);
            this.vertical.transition()
                .duration(this.animationDuration)
                .style("opacity", 1);
            this.legend.transition()
                .duration(this.animationDuration)
                .style("opacity", 1)
                .each("end", function() {
                    callback();
                });
        }
    }

    /**
        Hide legend by fading
    */
    TSDVJS.Graph.prototype.hideLegend = function(callback) {
        if (this.legend !== "undefined") {
            this.vertical.transition()
                .duration(this.animationDuration)
                .style("opacity", 0);
            this.legend.transition()
                .duration(this.animationDuration)
                .style("opacity", 0)
                .each("end", function() {
                    callback();
                });
        }
    }

    /**
        Close legend
     */
    TSDVJS.Graph.prototype.closeLegend = function(callback) {
        if (this.legend !== "undefined") {
            this.vertical.style("opacity", 0);
            this.legend.style("opacity", 0);
        }
    }

    /**
        Returns legend visibility
    */
    TSDVJS.Graph.prototype.isLegendVisible = function() {
        return (this.legend !== "undefined" && this.legend.style("opacity") > 0)
    }

    /**
        Move legend
    */
    TSDVJS.Graph.prototype.moveLegend = function(positionX, positionY) {
        if (this.legend !== "undefined") {
            this.updateLegend(this.getLegendData(positionX, positionY));
            this.vertical
                .style("left", positionX + "px")
                .style("top", positionY + "px");
            this.legend
                .style("left", (positionX - (this.legend.node().getBoundingClientRect().width / 2)) + "px")
                .style("top", positionY + "px");
        }
    }

    /**
        Update legend
    */
    TSDVJS.Graph.prototype.updateLegend = function(legendData) {
        if (this.legend !== "undefined" && legendData[0] !== undefined) {
            var table = d3.select("#chart table");
            table.select("tbody").remove();
            var tbody = table.append("tbody");
            var tr = tbody.append("tr");
            var timeFormat = d3.time.format("%I:%M%p");

            tr.append("td").attr("colspan", 2).style("font-weight", "bold").text(timeFormat(legendData[0].xValue));

            for (var key in legendData) {
                if (legendData.hasOwnProperty(key)) {
                    tr = tbody.append("tr");
                    tr.append("td").text(legendData[key].yValue);
                    tr.append("td").text(legendData[key].name);
                }
            }
        }
    }

    TSDVJS.Graph.prototype.getLegendData = function(positionX, positionY) {
        var attributesArray = [];
        var xVal = this.xScale.invert(positionX - margin.left);
        var dataAtX = this.visibleData[bisectDate(this.visibleData, xVal) - 1];

        for (var key in this.plotItems) {
            if (dataAtX !== undefined && this.plotItems.hasOwnProperty(key) && this.plotItems[key].visibility === true) {
                var yVal = dataAtX[key];

                if (key === "body_temp" || key === "calories" || key === "blood_sugar") {
                    yVal = yVal.toFixed(1);
                } else if (key === "gsr") {
                    yVal = (yVal / fullDataSize).toExponential(2);
                }

                var obj = {
                    name: key,
                    xValue: xVal,
                    yValue: (key === "body_temp" || key === "heart_rate" || key === "gsr" || key === "blood_sugar") && (yVal == 0) ? "--" : yVal
                };

                attributesArray.push(obj);
            }
        }

        return attributesArray;
    }

    return TSDVJS;
}(tsdvjs));
