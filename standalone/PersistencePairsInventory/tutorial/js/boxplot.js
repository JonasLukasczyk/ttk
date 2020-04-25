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
        if ( Window.PPI['boxplot-timestamp-range-mode']['enable'] ) {
            if ( group_idx >= Window.PPI['boxplot-timestamp-range-mode']['start'] && group_idx <= Window.PPI['boxplot-timestamp-range-mode']['end']) {
                format_data.push([com_idx, data[i], "group" + group_idx]) 
            }
        } else {
            format_data.push([com_idx, data[i], "group" + group_idx])
        }
        com_idx += 1;
        if (com_idx == numberofcomponents) {
            com_idx = 0;
            group_idx += 1;
        }
    }
    return format_data;
}

function drawBoxplotCurve(numberofcomponents, data) {
    $("#box_dataviz").html("")

    if ( ! Window.PPI['image-object'] ) {
        alert("please load data at first") ;
        return ;
    }

    var persistence_values = {
        xScale: Window.box_plot_config? Window.box_plot_config['xScale'].domain(): undefined,
        yScale: Window.box_plot_config ? Window.box_plot_config['yScale'].domain(): undefined, 
        switch_y_checked: $("#customSwitches--y").is(':checked'),
        boxplot_checkbox_checked: $("#boxplot-checkbox-lines").prop("checked"),
        boxplot_input_val: $("#boxplot-select-lines").val(),
        boxplot_polygon_checked: $("#boxplot-checkbox-polygon").prop("checked"),
    }

    Window.box_plot_config = {
        min_max_color: "#ef8a62",
        q1_q3_color: "#fddbc7",
        background_color: "white",
        median_line_color: "black",
        containerWidth: $("#box_dataviz").outerWidth(),  // 561
        containerHeight: 0.4 * (Window.PPI['main-container-height'] - $("#left-continer-middle").outerHeight()) ,
        margin: {
            top: 10,
            right: 10,
            bottom: 40,
            left: 54
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
        x_domain: undefined,
        y_domain: undefined,
        xScale: undefined,
        yScale: undefined,
    }

    function coverShadow() {
        $("[name=box_optimal_left]").remove() ;
        $("[name=box_optimal_right]").remove() ;
        // cover the shadow
        let threshold_idx = parseInt($("#hist-threshold").val()) * Window.PPI['thresholdRatio'] ;
        let max_threshold_window = parseInt($("#threshold-window").val()) * Window.PPI['thresholdRatio'] ;

        if ( Window.box_plot_config['xScale'](threshold_idx) >= 0 ) {
            g.append("line")
            .attr("name", "box_optimal_left")
            .attr("class", "zero")
            .attr("x1", Window.box_plot_config['xScale'](threshold_idx))
            .attr("y1", 0)
            .attr("x2", Window.box_plot_config['xScale'](threshold_idx))
            .attr("y2", height)
            .style("stroke", "red")
            .style("stroke-dasharray", ("5, 5"))
            .style("stroke-width", 1);

            g.append("circle")
                .attr("name", "box_optimal_left")
                .attr("r", 5)
                .attr("fill", "red")
                .attr("cx", function(d) { return Window.box_plot_config['xScale'](threshold_idx) })
                .attr("cy", function(d) { return height; });
        }

        if ( Window.box_plot_config['xScale'](max_threshold_window) >= 0 ) {
            g.append("line")
            .attr("name", "box_optimal_right")
            .attr("class", "zero")
            .attr("x1", Window.box_plot_config['xScale'](max_threshold_window))
            .attr("y1", 0)
            .attr("x2", Window.box_plot_config['xScale'](max_threshold_window))
            .attr("y2", height)
            .style("stroke", "black")
            .style("stroke-dasharray", ("5, 5"))
            .style("stroke-width", 1);

            g.append("circle")
                .attr("name", "box_optimal_right")
                .attr("fill", "black")
                .attr("r", 5)
                .attr("cx", function(d) { return Window.box_plot_config['xScale'](max_threshold_window) })
                .attr("cy", function(d) { return height; });
        }
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
    Window.box_plot_config.x_domain = [0, d3.max(format_data, function (d) {
        return d[0];
    })];

    if ( persistence_values['xScale'] !== undefined ) {
        
        Window.box_plot_config['xScale'] = d3.scaleLinear()
        .domain(persistence_values['xScale']) 
        .range([0, width]);
    } else {
        Window.box_plot_config['xScale'] = d3.scaleLinear()
        .domain(Window.box_plot_config.x_domain)
        .range([0, width]);
    }

    // Add Y axis
    // https://stackoverflow.com/questions/11322651/how-to-avoid-log-zero-in-graph-using-d3-js
    Window.box_plot_config.y_domain = [1, d3.max(format_data, function (d) {
        return d[1];
    })];

    if ( persistence_values['yScale'] !== undefined ) {
        if ( persistence_values['switch_y_checked'] ) {
            Window.box_plot_config['yScale'] = d3.scaleLog()
            .clamp(true)
            .domain(persistence_values['yScale'])
            .range([height, 0]);
        } else {
            Window.box_plot_config['yScale'] = d3.scaleLinear()
            .domain(persistence_values['yScale'])
            .range([height, 0]);
        }
        
    } else {
        if ( persistence_values['switch_y_checked'] ) {
            Window.box_plot_config['yScale'] = d3.scaleLog()
            .clamp(true)
            .domain(Window.box_plot_config.y_domain)
            .range([height, 0]);
        } else {
            Window.box_plot_config['yScale'] = d3.scaleLinear()
            .domain(persistence_values['yScale'])
            .range([height, 0]);
        }
    }

    // https://github.com/d3/d3-format
    var ticks_xAxis = 15;
    var xAxis = d3.axisBottom(Window.box_plot_config['xScale']).ticks(ticks_xAxis).tickFormat(function(d) {  return parseInt(d) == d? d: d.toFixed(1); }),
        yAxis = d3.axisLeft(Window.box_plot_config['yScale']).tickFormat(function(d) {  return (parseInt(d) == d? d: d.toFixed(1)); });

    var brush = d3.brush().on("end", brushended),
        idleTimeout,
        idleDelay = 350;

    var drag = d3.drag()
            .on("start", dragstarted)
            .on("drag", dragged)
            .on("end", dragended);

    g.append("g")
        .attr('class', 'axis--x')
        .style("font-size", "16px")
        .attr("transform", "translate(0," + height + ")")
        .call(xAxis);

    g.append("g")
        .attr('class', 'axis--y')
        .style("font-size", "16px")
        .attr("transform", "translate(-1,0)")
        .call(yAxis);

    g.append('defs')
        .append('clipPath')
        .attr('id', 'clip')
        .append('rect')
        .attr('x', 0)
        .attr('y', 0)
        .attr('width', width)
        .attr('height', height);

    // Add x-axis title
    g.append("text")
        .attr("class", "axisLabel--x")
        .attr("transform",
            "translate(" + (width / 2.5 + 10) + " ," + (height + 40) + ")")
        .style("text-anchor", "middle")
        .text("Threshold")
        .on("click", function(e) {
            $("#boxplotRange").attr("data-source", "x") ;
            $("#boxplotRangeLabel").text("the range of X axis") ;
            $("#notification_xxyy").click() ;

            d3.select(".mouse-line")
                    .style("opacity", "0");
                d3.selectAll(".mouse-per-line circle")
                    .style("opacity", "0");
                d3.selectAll(".mouse-per-line text")
                    .style("opacity", "0");
                return d3.select("#tooltipBoxplot").style("visibility", "hidden");
        });

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
        .on("click", function(e) { 
            $("#boxplotRange").attr("data-source", "y") ;
            $("#boxplotRangeLabel").text("the range of Y axis") ;
            $("#notification_xxyy").click() ;

            d3.select(".mouse-line")
                    .style("opacity", "0");
                d3.selectAll(".mouse-per-line circle")
                    .style("opacity", "0");
                d3.selectAll(".mouse-per-line text")
                    .style("opacity", "0");
                return d3.select("#tooltipBoxplot").style("visibility", "hidden");
        }) ;

    // scale region
    var main = g.append("g")
        .attr("id", "scale-region")
        .attr("width", width)
        .attr("height", height)
        .attr('class', 'main')
        .attr('clip-path', 'url(#clip)')
        .attr("transform", "scale(1)") ;

    svg.call(drag) ;

    function dragstarted(d) {
        d3.event.sourceEvent.stopPropagation();
        d3.select(this).classed("dragging", true);
        // without hover
        d3.select(".mouse-line").style("opacity", "0");
        d3.selectAll(".mouse-per-line circle").style("opacity", "0");
        d3.selectAll(".mouse-per-line text").style("opacity", "0");
        d3.select("#tooltipBoxplot").style("visibility", "hidden");
    }

    function dragged(d) {
        dx = Window.box_plot_config['xScale'].invert(d3.event.dx) - Window.box_plot_config['xScale'].domain()[0];
        if ($("#customSwitches--y").is(':checked')) {
            dy = 0 ;  // logarithmic
        } else {
            dy = Window.box_plot_config['yScale'].invert(d3.event.dy) - Window.box_plot_config['yScale'].domain()[1];
        }

        if ( Window.box_plot_config['xScale'].domain()[0] - dx >= 0 - 0.1) {
            Window.box_plot_config['xScale'].domain([Window.box_plot_config['xScale'].domain()[0] - dx, Window.box_plot_config['xScale'].domain()[1] - dx]) ;
        } 
        
        if ( Window.box_plot_config['yScale'].domain()[0] - dy >= 1 - 0.1 ) {
            Window.box_plot_config['yScale'].domain([Window.box_plot_config['yScale'].domain()[0] - dy, Window.box_plot_config['yScale'].domain()[1] - dy]);
        }
        
        zoom();
        
        $("#hidden-optimal-threshold").click();
        // $("#hidden-notification_xxyy").click();
    }

    function dragended(d) {
        d3.select(this).classed("dragging", false);
        d3.select(".mouse-line").style("opacity", "1");
        d3.selectAll(".mouse-per-line circle").style("opacity", "1");
        d3.select("#tooltipBoxplot").style("visibility", "visible") ;
    }

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
            if (Window.PPI['boxplot-timestamp-range-mode']['enable']) {
                if ( j >= Window.PPI['boxplot-timestamp-range-mode']['start'] && j <= Window.PPI['boxplot-timestamp-range-mode']['end'] ) {
                    items.push(data[i + j * numberofcomponents]);
                }
            } else {
                items.push(data[i + j * numberofcomponents]);
            }
        }
        var ret = drawBoxPlot(i, items);
        points['max'].push(ret['max']);
        points['q1'].push(ret['q1']);
        points['median'].push(ret['median']);
        points['q3'].push(ret['q3']);
        points['min'].push(ret['min']);
        points['data'].push(ret['data']);
    }

    drawPolygon();
    drawLine();

    function brushended() {
        var s = d3.event.selection;
        if (!s) {
        } else {
            // processing the s
            sx = [[s[0][0], s[0][1]], [s[1][0], s[1][1]]]
            sx[0][0] = Math.min(Math.max(0, s[0][0] - Window.box_plot_config.margin.left), Window.box_plot_config['xScale'].range()[1])
            sx[1][0] = Math.min(Math.max(0, s[1][0] - Window.box_plot_config.margin.left), Window.box_plot_config['xScale'].range()[1])

            sx[0][1] = Math.min(Math.max(0, s[0][1] - Window.box_plot_config.margin.top), Window.box_plot_config['yScale'].range()[0])
            sx[1][1] = Math.min(Math.max(0, s[1][1] - Window.box_plot_config.margin.top), Window.box_plot_config['yScale'].range()[0])

            Window.box_plot_config['xScale'].domain([sx[0][0], sx[1][0]].map(Window.box_plot_config['xScale'].invert));
            Window.box_plot_config['yScale'].domain([sx[1][1], sx[0][1]].map(Window.box_plot_config['yScale'].invert));
            
            svg.select(".brush").call(brush.move, null);
        }
        zoom();
        
        $("#hidden-optimal-threshold").click();
        // $("#hidden-notification_xxyy").click();
    }

    $("#hidden-optimal-threshold").unbind().click(function() {
        coverShadow() ;
    })

    $("#hidden-zoom-back").unbind().click(function () {
        Window.box_plot_config['xScale'].domain(Window.box_plot_config.x_domain);
        Window.box_plot_config['yScale'].domain(Window.box_plot_config.y_domain);
        zoom();

        $("#hidden-optimal-threshold").click();
        // $("#hidden-notification_xxyy").click();
    });

    $("#customSwitches--y").unbind().click(function () {
        if ($("#customSwitches--y").is(':checked')) {
            Window.box_plot_config['yScale'] = d3.scaleLog()
                .clamp(true)
                .domain(Window.box_plot_config.y_domain)
                .range([height, 0]);

        } else {
            Window.box_plot_config['yScale'] = d3.scaleLinear()
                .domain(Window.box_plot_config.y_domain)
                .range([height, 0]);
        }

        yAxis = d3.axisLeft(Window.box_plot_config['yScale']).tickFormat(function(d) {  return (parseInt(d) == d? d: d.toFixed(1)); });
        zoom();
        removeTicksByDistance() ;
    });

    // function idled() {
    //     idleTimeout = null;
    // }

    $("#hidden-brush-mode").unbind().click(function () {
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

    // $("#hidden-notification_xxyy").unbind().click(function () {
    //     $("#notification_xxyy").text(Window.box_plot_config['yScale'].domain()[0].toFixed(1) + "-" + Window.box_plot_config['yScale'].domain()[1].toFixed(1) +
    //         ", " + Window.box_plot_config['xScale'].domain()[0].toFixed(1) + "-" + Window.box_plot_config['xScale'].domain()[1].toFixed(1));
    // });

    $("#hidden-notification_xxyy-zoom").unbind().click(function () {
        // var x = $("#x-y-range-change").val().replace(" ", "");
        // Window.box_plot_config['yScale'].domain([x.split(",")[0].split("-")[0], x.split(",")[0].split("-")[1]]);
        // Window.box_plot_config['xScale'].domain([x.split(",")[1].split("-")[0], x.split(",")[1].split("-")[1]]);

        if ( $("#boxplotRange").attr("data-source") === 'x') {
            Window.box_plot_config['xScale'].domain([$("#x-y-range-left").val(), $("#x-y-range-right").val()]);
        } else {
            Window.box_plot_config['yScale'].domain([$("#x-y-range-left").val(), $("#x-y-range-right").val()]);
        }

        zoom();

        // $("#hidden-notification_xxyy").click();
        $("#boxplotRangeLabel-close").click();

    });

    function zoom() {
        // var t = svg.transition().duration(tx);
        svg.select(".axis--x").call(xAxis);
        svg.select(".axis--y").call(yAxis);

        g.selectAll("[name=box_polygon]") //.transition(t)
            .attr("d", function (d) {
                return Window.box_plot_config.line_area(Window.box_plot_config['xScale'], Window.box_plot_config['yScale'], Window.box_plot_config['yScale'](0))(d);
            });

        g.selectAll("[name=box_polygon_line]") // .transition(t)
            .attr("d", function (d) {
                return Window.box_plot_config.line(Window.box_plot_config['xScale'], Window.box_plot_config['yScale'])(d);
            });

        g.selectAll("[name=box_line]") // .transition(t)
            .attr("d", function (d) {
                return Window.box_plot_config.line_area(Window.box_plot_config['xScale'], Window.box_plot_config['yScale'], Window.box_plot_config['yScale'](0))(d.values);
            });

        replaceTicks(".axis--x g", getThresholdArray()) ;
        removeTicksByDistance(".axis--x g")
        removeTicksByDistance(".axis--y g")
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
        var center = Window.box_plot_config['xScale'](idx) + 1
        var width = 8

        ret['max'] = [idx, max, "group_max"];
        ret['q1'] = [idx, q1, "group_q1"];
        ret['median'] = [idx, median, "group_median"];
        ret['q3'] = [idx, q3, "group_q3"];
        ret['min'] = [idx, min, "group_min"];

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
                return Window.box_plot_config.line_area(Window.box_plot_config['xScale'], Window.box_plot_config['yScale'], Window.box_plot_config['yScale'](0))(d);
            })

        main.append("path")
            .attr("name", "box_polygon")
            .datum(points['q3'])
            .attr("fill", Window.box_plot_config.q1_q3_color)
            .attr("stroke", "none")
            .attr("stroke-width", 1.5)
            .attr("d", function (d) {
                return Window.box_plot_config.line_area(Window.box_plot_config['xScale'], Window.box_plot_config['yScale'], Window.box_plot_config['yScale'](0))(d);
            })

        main.append("path")
            .attr("name", "box_polygon")
            .datum(points['q1'])
            .attr("fill", Window.box_plot_config.min_max_color)
            .attr("stroke", "none")
            .attr("stroke-width", 1.5)
            .attr("d", function (d) {
                return Window.box_plot_config.line_area(Window.box_plot_config['xScale'], Window.box_plot_config['yScale'], Window.box_plot_config['yScale'](0))(d);
            })

        main.append("path")
            .attr("name", "box_polygon")
            .datum(points['min'])
            .attr("fill", Window.box_plot_config.background_color)
            .attr("stroke", "none")
            .attr("stroke-width", 1.5)
            .attr("d", function (d) {
                return Window.box_plot_config.line_area(Window.box_plot_config['xScale'], Window.box_plot_config['yScale'], Window.box_plot_config['yScale'](0))(d);
            })

        main.append("path")
            .attr("name", "box_polygon_line")
            .datum(points['median'])
            .attr("fill", "none")
            .attr("stroke", Window.box_plot_config.median_line_color)
            .attr("stroke-width", 1)
            .attr("d", function (d) {
                return Window.box_plot_config.line(Window.box_plot_config['xScale'], Window.box_plot_config['yScale'])(d);
            })

        var lineStroke = "2px"

        var mouseG = main.append("g")
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
                d3.select(".mouse-line").style("opacity", "1");
                d3.selectAll(".mouse-per-line circle").style("opacity", "1");
                return d3.select("#tooltipBoxplot").style("visibility", "visible");
            })
            .on('mousemove', function () { // update tooltip content, line, circles and text when mouse moves
                var mouse = d3.mouse(this)
                var bisect = d3.bisector(function (d) {
                    return d[0];
                }).left

                d3.selectAll(".mouse-per-line")
                    .attr("transform", function (d, i) {
                        var xIdx = Window.box_plot_config['xScale'].invert(mouse[0]) // use 'invert' to get date corresponding to distance from mouse position relative to svg
                        var idx = bisect(d.values, xIdx);
                        d3.select(".mouse-line")
                            .attr("d", function () {
                                var data = "M" + Window.box_plot_config['xScale'](d.values[idx][0]) + "," + (height);
                                data += " " + Window.box_plot_config['xScale'](d.values[idx][0]) + "," + 0;
                                return data;
                            });
                        return "translate(" + Window.box_plot_config['xScale'](d.values[idx][0]) + "," + Window.box_plot_config['yScale'](d.values[idx][1]) + ")";
                    });

                $("#tooltipBoxplot").html("");
                var xIdx = Window.box_plot_config['xScale'].invert(mouse[0]);
                var idx = -1;
                var hhtml = "" ;
                res_nested.forEach(function (d) {
                    if (idx === -1) {
                        idx = bisect(d.values, xIdx);
                    }
                    value = d.key.replace("group_", "") + ": <code>" + d.values[idx][1] + "</code>"
                    if ( hhtml === "") {
                        hhtml = "<strong>Threshold: </strong> <code>" + idx + "</code><br>- " + value ;
                    } else {
                        hhtml += "<br>- " + value ;
                    }
                })
                $("#tooltipBoxplot").html(hhtml) ;
                return d3.select("#tooltipBoxplot").style("top", (event.pageY - 10) + "px").style("left", (event.pageX + 10) + "px");
            })

        if ( ! persistence_values['boxplot_polygon_checked']) {
            $("[name^='box_polygon']").attr("visibility", "hidden");
            Window.PPI['boxplot-element-visibility']['box_polygon'] = false;
        }
    }

    function drawLine() {
        // color palette
        var res = sumstat.map(function (d) {
            return d.key
        }) ; // list of group names

        $("#boxplot-select-lines").html("<option value='all'>all</option>");
        for (var i = 0; i < res.length; i++) {
            if ( persistence_values['boxplot_input_val'] === res[i]) {
                $('#boxplot-select-lines').append('<option selected value=' + res[i] + '>' + res[i] + '</option>');
            } else {
                $('#boxplot-select-lines').append('<option value=' + res[i] + '>' + res[i] + '</option>');
            }
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
                return Window.box_plot_config.line_area(Window.box_plot_config['xScale'], Window.box_plot_config['yScale'], Window.box_plot_config['yScale'](0))(d.values);
            })
        
        if ( ! persistence_values['boxplot_checkbox_checked'] ) {
            $("[name='box_line']").attr("visibility", "hidden");
            Window.PPI['boxplot-element-visibility']['box_line'] = false;
        }
        $('#boxplot-select-lines').change() ;
    }
    $("#hidden-optimal-threshold").click();
    
    replaceTicks(".axis--x g", getThresholdArray()) ;
    removeTicksByDistance(".axis--x g")
    removeTicksByDistance(".axis--y g")
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
    // $("#x-y-range-change").val($("#notification_xxyy").text());
    if ( $("#boxplotRange").attr("data-source") === 'x') {
        $("#x-y-range-left").val(Window.box_plot_config['xScale'].domain()[0]) ;
        $("#x-y-range-right").val(Window.box_plot_config['xScale'].domain()[1]) ;
    } else {
        $("#x-y-range-left").val(Window.box_plot_config['yScale'].domain()[0]) ;
        $("#x-y-range-right").val(Window.box_plot_config['yScale'].domain()[1]) ;
    }
});

$("#save-xy-axis").unbind().click(function () {
    // var x = $("#x-y-range-change").val().replace(" ", "");
    // if (x.split(",").length !== 2 && x.split(",")[0].split("-").length != 2 && x.split(",")[1].split("-").length != 2) {
    //     alert("Format must be yMin-yMax, xMin-yMax");
    // } else {
    //     $("#hidden-notification_xxyy-zoom").click();
    // }
    if ( parseInt($("#x-y-range-left").val()) > parseInt($("#x-y-range-right").val()) ) {
        alert("Please double click min and max value");
    } else {
        $("#hidden-notification_xxyy-zoom").click();
    }
});