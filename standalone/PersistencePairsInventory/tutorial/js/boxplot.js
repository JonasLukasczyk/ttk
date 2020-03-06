 // Add tooltips for boxplot
 d3.select("body")
    .append("div")
    .attr("id", "tooltipBoxplot")
    .attr("class", "tooltipsBoxplot")

function formatInput(numberofcomponents, data) {
    let format_data = [],
        group_idx = 0,
        com_idx = 0;
    for (let i = 0; i < data.length; i++) {
        com_idx += 1;
        format_data.push([com_idx, data[i], "group" + group_idx]) // index: start from 1
        if (com_idx == numberofcomponents) {
            com_idx = 0;
            group_idx += 1;
        }
    }
    return format_data;
}

function drawBoxplotCurve(numberofcomponents, data) {
    Window.box_plot_config = {
        min_max_color: "#abdda4",
        q1_q3_color: "#3288bd",
        background_color: "#f8f9fa",
        containerWidth: 561,
        containerHeight: 334,
        margin: {
            top: 10,
            right: 10,
            bottom: 40,
            left: 50
        },
        line_area: function (x_func, y_func, y0) {
            return d3.area()
                .x(function (d) {
                    return x_func(d[0]) + 1;
                })
                .y0(y0)
                .y1(function (d) {
                    return y_func(d[1]);
                })
                .curve(d3.curveMonotoneX);
        },
        line: function (x_func, y_func) {
            return d3.line()
                .x(function (d) {
                    return x_func(d[0]) + 1;
                })
                .y(function (d) {
                    return y_func(d[1]);
                })
        },
        x_domain: [0, 0],
        y_domain: [0, 0],
    }

    var format_data = formatInput(numberofcomponents, data);

    let csvContent = "data:text/csv;charset=utf-8," +
        "Threshold,Number of Persistence Pairs,Line Name\n" +
        format_data.map(e => e.join(",")).join("\n");
    var encodedUri = encodeURI(csvContent);
    $("#download_raw_data").attr("href", encodedUri);

    var width = Window.box_plot_config.containerWidth - Window.box_plot_config.margin.left - Window.box_plot_config.margin.right,
        height = Window.box_plot_config.containerHeight - Window.box_plot_config.margin.top - Window.box_plot_config.margin.bottom;

    var ratio = height / width;

    var svg = d3.select("#box_dataviz")
        .append("svg")
        .attr("width", Window.box_plot_config.containerWidth)
        .attr("height", Window.box_plot_config.containerHeight);

    var g = svg
        .append("g")
        .attr("id", "region-id")
        .attr("transform",
            "translate(" + Window.box_plot_config.margin.left + "," + Window.box_plot_config.margin.top + ")")
        .attr('overflow', 'hidden');

    var sumstat = d3.nest()
        .key(function (d) {
            return d[2];
        })
        .entries(format_data);

    // Add X axis
    Window.box_plot_config.x_domain = [1, d3.max(format_data, function (d) {
        return d[0];
    })];
    var xScale = d3.scaleLinear()
        .domain(Window.box_plot_config.x_domain)
        .range([0, width]);

    // Add Y axis
    // https://stackoverflow.com/questions/11322651/how-to-avoid-log-zero-in-graph-using-d3-js
    Window.box_plot_config.y_domain = [1, d3.max(format_data, function (d) {
        return d[1];
    })];
    var yScale = d3.scaleLog()
        .clamp(true)
        .domain(Window.box_plot_config.y_domain)
        .range([height, 0]);

    // https://github.com/d3/d3-format
    var xAxis = d3.axisBottom(xScale).ticks(20).tickFormat(d3.format("20")),
        yAxis = d3.axisLeft(yScale).tickFormat(d3.format("20"));

    var brush = d3.brush().on("end", brushended),
        idleTimeout,
        idleDelay = 350;

    var drag = d3.drag().on('drag', dragged);

    g.append("g")
        .attr('class', 'axis--x')
        .attr("transform", "translate(0," + height + ")")
        .call(xAxis);

    g.append("g")
        .attr('class', 'axis--y')
        .call(yAxis);

    g.append('defs')
        .append('clipPath')
        .attr('id', 'clip')
        .append('rect')
        .attr('x', 1)
        .attr('y', 0)
        .attr('width', width)
        .attr('height', height);

    // Add x-axis title
    g.append("text")
        .attr("class", "axisLabel--x")
        .attr("transform",
            "translate(" + (width / 2.5 + 10) + " ," + (height + 40) + ")")
        .style("text-anchor", "middle")
        .text("Threshold");

    // Add y-axis title
    g.append("text")
        .attr("class", "axisLabel--y")
        .attr("transform", "rotate(-90)")
        .attr("y", 0 - Window.box_plot_config.margin.left)
        .attr("x", 0 - (height / 2))
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("text-anchor", "middle")
        .text("Number of Persistence Pairs")

    // scale region
    var main = g.append("g")
        .attr("id", "scale-region")
        .attr("width", width)
        .attr("height", height)
        .attr('class', 'main')
        .attr('clip-path', 'url(#clip)')
        .attr("transform", "scale(1)")

    var points = {
        "max": [],
        "q1": [],
        "median": [],
        "q3": [],
        "min": [],
        "data": [],
    }

    for (let i = 0; i < numberofcomponents; i++) {
        let items = [];
        for (let j = 0; j < data.length / numberofcomponents; j++) {
            items.push(data[i + j * numberofcomponents]);
        }
        var ret = drawBoxPlot(i + 1, items);
        points['max'].push(ret['max']);
        points['q1'].push(ret['q1']);
        points['median'].push(ret['median']);
        points['q3'].push(ret['q3']);
        points['min'].push(ret['min']);
        points['data'].push(ret['data']);
    }

    drawPolygon();
    drawLine();

    // function drawOptimalLine(idx) {
    //     // a few features for the box
    //     main.append("line")
    //         .attr("name", "box_optimal")
    //         .attr("class", "optimal-threshold")
    //         .attr("x1", xScale(idx))
    //         .attr("y1", 0)
    //         .attr("x2", xScale(idx))
    //         .attr("y2", Window.box_plot_config.containerHeight)
    //         .style("stroke", "green")
    //         .style("stroke-width", 5);
    // }

    function brushended() {
        var s = d3.event.selection;
        if (!s) {
        } else {
            xScale.domain([s[0][0] * ratio, s[1][0]].map(xScale.invert, xScale));
            yScale.domain([s[1][1], s[0][1] * ratio].map(yScale.invert, yScale));

            svg.select(".brush").call(brush.move, null);
        }
        zoom();
        
        // $("#hidden-optimal-threshold").click();
        $("#hidden-notification_xxyy").click();
    }

    $("#hidden-zoom-back").unbind().click(function () {
        xScale.domain(Window.box_plot_config.x_domain);
        yScale.domain(Window.box_plot_config.y_domain);
        zoom();

        // $("#hidden-optimal-threshold").click();
        $("#hidden-notification_xxyy").click();
    });

    // $("#hidden-optimal-threshold").unbind().click(function () {
    //     d3.select(".optimal-threshold").remove();
    //     if ($("#boxplot-threshold").val().replace(" ", "") !== "") {
    //         drawOptimalLine(parseFloat($("#boxplot-threshold").val()));
    //     }
    // });

    $("#customSwitches--x").unbind().click(function () {
        if ($("#customSwitches--x").is(':checked')) {
            xScale = d3.scaleLog()
                .clamp(true)
                .domain(Window.box_plot_config.x_domain)
                .range([0, width]);

        } else {
            xScale = d3.scaleLinear()
                .domain(Window.box_plot_config.x_domain)
                .range([0, width]);
        }

        xAxis = d3.axisBottom(xScale).ticks(20).tickFormat(d3.format("20"));
        zoom();
    });

    $("#customSwitches--y").unbind().click(function () {
        if ($("#customSwitches--y").is(':checked')) {
            yScale = d3.scaleLog()
                .clamp(true)
                .domain(Window.box_plot_config.y_domain)
                .range([height, 0]);

        } else {
            yScale = d3.scaleLinear()
                .domain(Window.box_plot_config.y_domain)
                .range([height, 0]);
        }

        yAxis = d3.axisLeft(yScale).tickFormat(d3.format("20"));
        zoom();
    });

    function idled() {
        idleTimeout = null;
    }

    $("#hidden-brush-mode").unbind().unbind().click(function () {
        if ($("#brush_mode").attr("mode") == "brush") {
            $(".brush").remove();
            $("#brush_mode").attr("mode", "view");
            $("#brush_mode").text("view mode");
        } else {
            svg.append("g")
                .attr("class", "brush")
                .call(brush);
            $("#brush_mode").attr("mode", "brush");
            $("#brush_mode").text("brush mode");
        }
    });

    $("#hidden-notification_xxyy").unbind().click(function () {
        $("#notification_xxyy").text(yScale.domain()[0].toFixed(1) + "-" + yScale.domain()[1].toFixed(1) +
            ", " + xScale.domain()[0].toFixed(1) + "-" + xScale.domain()[1].toFixed(1));
    });

    $("#hidden-notification_xxyy-zoom").unbind().click(function () {
        var x = $("#x-y-range-change").val().replace(" ", "");
        yScale.domain([x.split(",")[0].split("-")[0], x.split(",")[0].split("-")[1]]);
        xScale.domain([x.split(",")[1].split("-")[0], x.split(",")[1].split("-")[1]]);

        zoom();

        $("#hidden-notification_xxyy").click();
        $("#boxplotRangeLabel-close").click();
        // $("#hidden-optimal-threshold").click();
    });

    function zoom() {
        var t = svg.transition().duration(750);
        svg.select(".axis--x").transition(t).call(xAxis);
        svg.select(".axis--y").transition(t).call(yAxis);

        g.selectAll("[name=box_polygon]").transition(t)
            .attr("d", function (d) {
                return Window.box_plot_config.line_area(xScale, yScale, yScale(0))(d);
            });

        g.selectAll("[name=box_polygon_line]").transition(t)
            .attr("d", function (d) {
                return Window.box_plot_config.line(xScale, yScale)(d);
            });

        g.selectAll("[name=box_line]").transition(t)
            .attr("d", function (d) {
                return Window.box_plot_config.line_area(xScale, yScale, yScale(0))(d.values);
            });

    }

    function dragged() {
        d3.selectAll('.line')
            .attr('transform', `translate(${d3.event.x}, ${d3.event.y})`);
        svg.select(".axis--x").call(xAxis);
        svg.select(".axis--y").call(yAxis);
    }

    function drawBoxPlot(idx, data) {
        // Compute summary statistics used for the box:
        var data_sorted = data.sort(d3.ascending)
        var ret = {
            "max": [],
            "q1": [],
            "median": [],
            "q3": [],
            "min": [],
            "data": data_sorted,
        };
        var q1 = d3.quantile(data_sorted, .25)
        var median = d3.quantile(data_sorted, .5)
        var q3 = d3.quantile(data_sorted, .75)
        var min = d3.min(data_sorted)
        var max = d3.max(data_sorted)

        // a few features for the box
        var center = xScale(idx) + 1
        var width = 8

        ret['max'] = [idx, max, "group_max"];
        ret['q1'] = [idx, q1, "group_q1"];
        ret['median'] = [idx, median, "group_median"];
        ret['q3'] = [idx, q3, "group_q3"];
        ret['min'] = [idx, min, "group_min"];

        // Show the main vertical line
        main
            .append("line")
            .attr("name", "box_box")
            .attr("x1", center)
            .attr("x2", center)
            .attr("y1", yScale(min))
            .attr("y2", yScale(max))
            .attr("stroke", "black")

        // Show the box
        main
            .append("rect")
            .attr("name", "box_box")
            .attr("x", center - width / 2)
            .attr("y", yScale(q3))
            .attr("height", (yScale(q1) - yScale(q3)))
            .attr("width", width)
            .attr("stroke", "black")
            .style("fill", "#69b3a2")

        // show median, min and max horizontal lines
        main
            .selectAll("toto")
            .data([min, median, max])
            .enter()
            .append("line")
            .attr("name", "box_box")
            .attr("x1", center - width / 2)
            .attr("x2", center + width / 2)
            .attr("y1", function (d) {
                return (yScale(d))
            })
            .attr("y2", function (d) {
                return (yScale(d))
            })
            .attr("stroke", "black");
        return ret;
    }

    function drawPolygon() {
        main.append("path")
            .datum(points['max'])
            .attr("name", "box_polygon")
            .attr("class", "")
            .attr("fill", Window.box_plot_config.min_max_color)
            .attr("stroke", "none")
            .attr("stroke-width", 1.5)
            .attr("d", function (d) {
                return Window.box_plot_config.line_area(xScale, yScale, yScale(0))(d);
            })

        main.append("path")
            .attr("name", "box_polygon")
            .datum(points['q3'])
            .attr("fill", Window.box_plot_config.q1_q3_color)
            .attr("stroke", "none")
            .attr("stroke-width", 1.5)
            .attr("d", function (d) {
                return Window.box_plot_config.line_area(xScale, yScale, yScale(0))(d);
            })

        main.append("path")
            .attr("name", "box_polygon")
            .datum(points['q1'])
            .attr("fill", Window.box_plot_config.min_max_color)
            .attr("stroke", "none")
            .attr("stroke-width", 1.5)
            .attr("d", function (d) {
                return Window.box_plot_config.line_area(xScale, yScale, yScale(0))(d);
            })

        main.append("path")
            .attr("name", "box_polygon")
            .datum(points['min'])
            .attr("fill", Window.box_plot_config.background_color)
            .attr("stroke", "none")
            .attr("stroke-width", 1.5)
            .attr("d", function (d) {
                return Window.box_plot_config.line_area(xScale, yScale, yScale(0))(d);
            })

        main.append("path")
            .attr("name", "box_polygon_line")
            .datum(points['median'])
            .attr("fill", "none")
            .attr("stroke", "red")
            .attr("stroke-width", 1)
            .attr("d", function (d) {
                return Window.box_plot_config.line(xScale, yScale)(d);
            })

        var lineStroke = "2px"

        var mouseG = g.append("g")
            .attr("class", "mouse-over-effects");

        mouseG.append("path") // create vertical line to follow mouse
            .attr("class", "mouse-line")
            .style("stroke", "#A9A9A9")
            .style("stroke-width", lineStroke)
            .style("opacity", "0");

        // format the nested data for polygon
        var res_nested = [];
        var keys = ['max', 'q3', 'median', 'q1', 'min'];

        keys.forEach(function (key) {
            res_nested.push({
                "key": points[key][0][2],
                "values": points[key],
            })
        });

        var mousePerLine = mouseG.selectAll('.mouse-per-line')
            .data(res_nested)
            .enter()
            .append("g")
            .attr("class", "mouse-per-line");

        mousePerLine.append("circle")
            .attr("r", 4)
            .style("stroke", function (d) {
                return "black";
            })
            .style("fill", "none")
            .style("stroke-width", lineStroke)
            .style("opacity", "0");

        mouseG.append('svg:rect') // append a rect to catch mouse movements on canvas
            .attr('width', width)
            .attr('height', height)
            .attr('fill', 'none')
            .attr('pointer-events', 'all')
            .on('mouseout', function () { // on mouse out hide line, circles and text
                d3.select(".mouse-line")
                    .style("opacity", "0");
                d3.selectAll(".mouse-per-line circle")
                    .style("opacity", "0");
                d3.selectAll(".mouse-per-line text")
                    .style("opacity", "0");
                return d3.select("#tooltipBoxplot").style("visibility", "hidden");
            })
            .on('mouseover', function () { // on mouse in show line, circles and text
                d3.select(".mouse-line")
                    .style("opacity", "1");
                d3.selectAll(".mouse-per-line circle")
                    .style("opacity", "1");

                return d3.select("#tooltipBoxplot").style("visibility", "visible");
            })
            .on('mousemove', function () { // update tooltip content, line, circles and text when mouse moves
                var mouse = d3.mouse(this)
                var bisect = d3.bisector(function (d) {
                    return d[0];
                }).left

                d3.selectAll(".mouse-per-line")
                    .attr("transform", function (d, i) {
                        var xIdx = xScale.invert(mouse[0]) // use 'invert' to get date corresponding to distance from mouse position relative to svg
                        var idx = bisect(d.values, xIdx);
                        d3.select(".mouse-line")
                            .attr("d", function () {
                                var data = "M" + xScale(d.values[idx][0]) + "," + (height);
                                data += " " + xScale(d.values[idx][0]) + "," + 0;
                                return data;
                            });
                        return "translate(" + xScale(d.values[idx][0]) + "," + yScale(d.values[idx][1]) + ")";
                    });

                $("#tooltipBoxplot").html("");
                var xIdx = xScale.invert(mouse[0]);
                var idx = -1;
                res_nested.forEach(function (d) {
                    if (idx === -1) {
                        idx = bisect(d.values, xIdx);
                    }
                    value = d.key.replace("group_", "") + ": <code>" + d.values[idx][1] + "</code>"
                    if ($("#tooltipBoxplot").html() == "") {
                        $("#tooltipBoxplot").html("<strong>Threshold: </strong> <code>" + (idx + 1) + "</code><br>- " + value);
                    } else {
                        $("#tooltipBoxplot").html($("#tooltipBoxplot").html() + "<br>- " + value);
                    }
                })

                return d3.select("#tooltipBoxplot").style("top", (event.pageY - 10) + "px").style("left", (event.pageX + 10) + "px");
            })
    }

    function drawLine() {

        // color palette
        var res = sumstat.map(function (d) {
            return d.key
        }) ; // list of group names

        $("#boxplot-select-lines").html("<option value='all'>all</option>");
        for (var i = 0; i < res.length; i++) {
            $('#boxplot-select-lines').append('<option value=' + res[i] + '>' + res[i] + '</option>');
        }

        $("#boxplot-select-lines").change(function () {
            $("#boxplot-select-lines option:selected").each(function () {
                if ($(this).val() == "all") {
                    $("[group_idx^=group]").each(function () {
                        $(this).css({
                            opacity: 1
                        })
                    });
                } else {
                    $("[group_idx^=group]").each(function () {
                        $(this).css({
                            opacity: 0.1
                        })
                    });
                    $("[group_idx=" + $(this).val() + "]").css({
                        opacity: 1
                    });
                }
            });
        });

        var color = d3.scaleOrdinal()
            .domain(res)
            .range(d3.schemeCategory20)

        // Draw the line
        main.selectAll(".line")
            .data(sumstat)
            .enter()
            .append("path")
            .attr("name", "box_line")
            .attr("group_idx", function (d) {
                return d.key;
            })
            .attr("fill", "none")
            .attr("stroke", function (d) {
                return color(d.key)
            })
            .attr("stroke-width", 1.5)
            .attr("d", function (d) {
                return Window.box_plot_config.line_area(xScale, yScale, yScale(0))(d.values);
            })
    }
}

$("#boxplot-checkbox-lines").unbind().click(function () {
    if (Window.PPI['boxplot-element-visibility']['box_line'] == true) {
        $("[name='box_line']").attr("visibility", "hidden");
        Window.PPI['boxplot-element-visibility']['box_line'] = false;
    } else {
        $("[name='box_line']").attr("visibility", "show");
        Window.PPI['boxplot-element-visibility']['box_line'] = true;
    }
});

$("#boxplot-checkbox-polygon").unbind().click(function () {
    if (Window.PPI['boxplot-element-visibility']['box_polygon'] == true) {
        $("[name^='box_polygon']").attr("visibility", "hidden");
        Window.PPI['boxplot-element-visibility']['box_polygon'] = false;
    } else {
        $("[name^='box_polygon']").attr("visibility", "show");
        Window.PPI['boxplot-element-visibility']['box_polygon'] = true;
    }
});

$("#box_zoom_back").unbind().click(function () {
    $("#hidden-zoom-back").click();
});

$("#brush_mode").unbind().click(function() {
    $("#hidden-brush-mode").click(); 
}) ;

$('#boxplotRange').on('show.bs.modal', function (event) {
    $("#x-y-range-change").val($("#notification_xxyy").text());
});

$("#save-xy-axis").unbind().click(function () {
    var x = $("#x-y-range-change").val().replace(" ", "");
    if (x.split(",").length !== 2 && x.split(",")[0].split("-").length != 2 && x.split(",")[1].split("-").length != 2) {
        alert("Format must be yMin-yMax, xMin-yMax");
    } else {
        $("#hidden-notification_xxyy-zoom").click();
    }
});