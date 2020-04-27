// Add tooltips for histogram
d3.select("body")
    .append("div")
    .attr("id", "tooltip")
    .attr("class", "tooltipsHist")
    .append("svg")
    .attr("id", "tooltipSvg")
    .style("height", "160px")
    .style("padding-top", "4px");

// iComponent: for the threshold index
// socket: webSocketIO object
// returned: used for retrieving data purely
function drawHistogram(iComponent, socket, tag=0) {
    $("#hist-clip-highlight rect").each(function() {
        $(this).detach().appendTo("#hist-clip-opacity");
    }) ;

    function getHistogramKey(iComponent) {
        var tmp = getRangeOfPPI()
        min_persistence_pairs = tmp[0] ;
        max_persistence_pairs = tmp[1] ;
        // key: iComponent + left-right relibility + min-max threshold + min-max range of the bins
        return iComponent + ":" + getFloatValue("#histogram_viz_legend", "data-x_0") + "-" + getFloatValue("#histogram_viz_legend", "data-x_1") +
                    $("#hist-threshold").val() + "-" + $("#threshold-window").val() + ":" +
                    min_persistence_pairs + "-" + max_persistence_pairs ;

    }

    // whatever we need to do
    $("#hist-clip-opacity").css("opacity", 1) ;

    var key = getHistogramKey(iComponent) ;
    if ( key === Window.PPI['redraw-MDM-key'] ) {
        return ;
    }
    Window.PPI['redraw-MDM-key'] = key ;

    $("#histogram_viz").loading({theme: 'light'});
    // tag: default 0, 1 => use current medium for top and right bar
    if ( ! Window.PPI['image-object'] ) {
        alert("please load data at first") ;
        return ;
    }

    var tx = 0, ty = 0, scale = 0 ;
    console.log("invoke draw histogram functionaility") ;

    function coverShadow(time_idx) {
        // cover shadow in a new method
        time_idx = parseInt(time_idx) ;
        $("#hist-clip-highlight rect").each(function() {
            $(this).detach().appendTo("#hist-clip-opacity");
        }) ;

        $("#hist-clip-opacity").css("opacity", 0.1) ;

        $("[mdm-bin-time-idx=" + time_idx + "]").each(function() {
            $(this).detach().appendTo("#hist-clip-highlight") ;
        }) ;
    }

    function drawCoverBox() {
        $(".mdm-rect-box-cover").remove() ;
        // cover the shadow
        if ( Window.PPI['boxplot-timestamp-range-mode']['enable'] ) {
            var left = Window.PPI['boxplot-timestamp-range-mode']['start'] ;
            var right = Window.PPI['boxplot-timestamp-range-mode']['end'] ;
            var gap = x(1) - x(0) ;
            var idx_first = gap * left ;
            var idx_sec = gap * (right + 1) ;

            g_main.append("rect")
                .attr("class", "mdm-rect-box-cover")
                .attr("x", idx_first)
                .attr("y", -10000)
                .attr("height", 20000)
                .attr("width", idx_sec - idx_first)
                .style("fill", "none")
                .style("stroke", "black")
                .style("stroke-dasharray", ("5, 5"))
                .style("stroke-width", '0.15em')
                .attr("transform", transFormApply(d3.select("#hist-clip-opacity").attr("transform")));
            }
    }

    $("#hidden-mdm-add-window").unbind().click(function() {
        drawCoverBox() ;
    }) ;

    let data = Window.PPI['image-object']['PointData'][Window.PPI['APPIAttrName']].Values ;
    let nComponents = Window.PPI['numberOfThreshold-histogram'] ;
    let fieldData = Window.PPI['image-object']['FieldData'] ;

    var cx = 0,
        cy = 0 ;

    var tmp = getRangeOfPPI()
    min_persistence_pairs = tmp[0] ;
    max_persistence_pairs = tmp[1] ;

    let max_threshold_window = parseInt($("#threshold-window").val())
    let vData = getHistogramFrameData(min_persistence_pairs, iComponent, max_threshold_window, max_persistence_pairs, nComponents) ;
    let dist_data = getLegendData(vData) ;

    var t00 = performance.now() ;
    drawLegend(dist_data, min_persistence_pairs, max_persistence_pairs, tag === 1 ? 1: 0) ;
    var t11 = performance.now() ;
    console.log("the cost of time for rendering legend: " + (t11 - t00) + " milliseconds");

    // FLAG, draw histogram, remove all previous data
    var hist_clip_highlight_transform = $("#hist-clip-highlight").attr("transform") ;
    var mdm_rect_box_cover_transform = $(".mdm-rect-box-cover").attr("transform");
    var axis_hist_y = $(".axis--hist--y").attr("transform");
    var axis_hist_x = $(".axis--hist--x").attr("transform");
    var axis_hist_y_g = $(".axis--hist--y g").length > 0? $($(".axis--hist--y g")[0]).attr("transform"): undefined ;
    var axis_hist_x_g = $(".axis--hist--x g").length > 0? $($(".axis--hist--x g")[0]).attr("transform"): undefined ;

    d3.select("#histogram_viz *").remove() ;
    let myGroups = getArray( Window.PPI['histogram-width']);
    let myVars = getArray( Window.PPI['histogram-height']);

    var containerHeight = Window.PPI['main-container-height'] ;
    var hBase = containerHeight / Window.PPI['histogram-height'] ;
    hBase = Math.min(60, hBase) ;
    containerHeight = hBase * Window.PPI['histogram-height'] ;
    var containerWidth = hBase * Window.PPI['histogram-width'] + 50;

    let margin = {
        top: 2,
        right: 0,
        bottom: 40,
        left: 70
    } ;
    // var fixedContainerWidth = 1251,
    //     fixedContainerHeight = 841 ;
    var fixedContainerWidth = $("#histogram_viz").outerWidth(),
         fixedContainerHeight = Window.PPI['main-container-height']  ;

    var width = containerWidth - margin.left - margin.right,
        height = containerHeight - margin.top - margin.bottom;

    var fixedWidth = fixedContainerWidth - margin.left - margin.right,
        fixedHeight = fixedContainerHeight - margin.top - margin.bottom;

    let container = d3.select("#histogram_viz")
        .append("svg")
        .attr("width", Math.max(containerWidth, fixedContainerWidth))
        .attr("height", Math.max(containerHeight, fixedContainerHeight+30)) ;

    var svg = container
                .append("g")
                .attr("id", "hist-id-g")
                .attr("transform",
                    "translate("+margin.left+", "+margin.top+")")
                .attr('overflow', 'hidden');

    let x = d3.scaleBand()
        .range([0, width])
        .domain(myGroups) ;

    svg.append('defs')
        .append('clipPath')
        .attr('id', 'hist-clip-x')
        .append('rect')
        .attr('x', 0)
        .attr('y', fixedHeight)
        .attr('width', fixedWidth)
        .attr('height', margin.bottom);

    var xg = svg.append("g")
        .attr('clip-path', 'url(#hist-clip-x)');

    xg.append("g")
        .attr('class', 'axis--hist--x')
        .attr("transform", "translate(0," + fixedHeight + ")")
        .style("font-size", "16px")
        .call(d3.axisBottom(x)) ;

    let y = d3.scaleBand()
        .range([height, 0])
        .domain(myVars) ;

    svg.append('defs')
        .append('clipPath')
        .attr('id', 'hist-clip-y')
        .append('rect')
        .attr('x', -50)
        .attr('y', 0)
        .attr('width', margin.left)
        .attr('height', fixedHeight);

    var yg = svg.append("g")
        .attr('clip-path', 'url(#hist-clip-y)');

    yg.append("g")
        .attr('class', 'axis--hist--y')
        .attr("transform", "translate(0,0)")
        .style("font-size", "16px")
        .call(d3.axisLeft(y));

    svg.append('defs')
        .append('clipPath')
        .attr('id', 'hist-clip')
        .append('rect')
        .attr('x', 0)
        .attr('y', 0)
        .attr('width', fixedWidth)
        .attr('height', fixedHeight);

    // scale region
    var t000 = performance.now() ;
    var parentSvg = svg.append("svg") ;
    parentSvg.on("mouseover", function () {
        var tmp = d3.mouse(this) ;
        cx = tmp[0] ;
        cy = tmp[1] ;
    } )
    .on("mousemove", function() {
        var tmp = d3.mouse(this) ;
        cx = tmp[0] ;
        cy = tmp[1] ;
    });
    var g_main = parentSvg.append("g").attr('clip-path', 'url(#hist-clip)');

    var main = g_main.append("g").attr("id", "hist-clip-opacity") ;
    g_main.append("g").attr("id", "hist-clip-highlight") ;

    const crosshairSize = 100000;
    const crosshairV = main.append('line')
        .attr('x1', 0)
        .attr('y1', -crosshairSize)
        .attr('x2', 0)
        .attr('y2', crosshairSize)
        .attr('stroke','black')
        .attr('stroke-width',1);

    const crosshairH = main.append('line')
        .attr('x1', -crosshairSize)
        .attr('y1', 0)
        .attr('x2', crosshairSize)
        .attr('y2', 0)
        .attr('stroke','black')
        .attr('stroke-width',1);

    const mainJQ = $(main.node());
    mainJQ.empty();
    const binWidth = x.bandwidth();
    const binHeight = y.bandwidth();
    const bins = [];

    for(let i=0; i<vData.length; i++){
        const d = vData[i];
        const bin = $(document.createElementNS("http://www.w3.org/2000/svg", 'rect'));
        bin.attr("x",x(d[0]));
        bin.attr("y",y(d[1]));
        bin.attr("width",binWidth);
        bin.attr("height",binHeight);
        bin.attr("name", "bin") ;
        bin.attr("mdm-bin-time-idx", parseInt(d[0])) ;
        bin.attr("id", "hist-bin-" + i ) ;
        bin.prop("data",d);
        bin.addClass( customColor(d[5], d[2], undefined, undefined, undefined, true) );
        bins.push(bin);
    }

    // using global variable to speed up
    Window.PPI['histogram-bins'] = bins;

    mainJQ.append(bins); // append at once

    let selectedBin = null;
    // TODO: tooltip interaction
    // mainJQ
    //     .on("mouseover", e => {
    //         if(e.target.nodeName!=='rect')
    //             return 1;

    //         const bin =  $(e.target);
    //         const d = bin.prop('data');

    //         if (!e.ctrlKey) {
    //             if (Window.PPI["X-mode"] === 0) {
    //                 d3.select("#tooltipSvg").selectAll("*").remove();
    //                 drawCurveLine("tooltipSvg", d[3], iComponent, d[5], d[2]);
    //                 d3.select("#tooltip").style("visibility", "visible");
    //             }
    //         }
    //     })
    //     .on("mousemove", e => {
    //         if(e.target.nodeName!=='rect')
    //             return 1;
    //         const bin =  $(e.target);
    //         const d = bin.prop('data');
    //         d3.select("#tooltip").style("top", (e.pageY - 10) + "px").style("left", (e.pageX + 10) + "px");
    //     })
        // .on("mouseout", e => {
        //     d3.select("#tooltip").style("visibility", "hidden");
        // })
    // ;

    var drag = d3.drag()
            .on("start", dragstarted)
            .on("drag", dragged)
            .on("end", dragended);

    var zoom = d3.zoom()
                 .scaleExtent([0.1, 10])
                 .on("zoom", zoomed)
                 .on("start", zoomstart)
                 .on("end", zoomend) ;

    var slider = d3.select("#range_input")
        .datum({})
        .attr("value", 10)
        .attr("min", 1)
        .attr("max", 20)
        .attr("step", 0.1)
        .on("input", slided);

    function callFunc() { // most time-consuming part, avg time is 1.5 seconds
        main.call(drag).call(zoom);
    }
    setTimeout(callFunc, 0) ;

    var t111 = performance.now() ;
    console.log("the cost of time for rendering bins of histogram: " + (t111 - t000) + " milliseconds");

    // Add y-axis title
    svg.append("text")
        .attr("class", "hist-yaxis-title")
        .attr("transform", "rotate(-90)")
        .attr("y", 0 - 70 )
        .attr("x", 0 - (fixedHeight / 2))
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("font-size", "1.5em")
        .style("text-anchor", "middle")
        .style("font-weight", "bold")
        .text("Scalar") ;

    // Add x-axis title
    svg.append("text")
        .attr("class", "hist-xaxis-title")
        .attr("y", (fixedHeight + 24) )
        .attr("x", (fixedWidth / 2.5 + 110))
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("font-size", "1.5em")
        .style("text-anchor", "middle")
        .style("font-weight", "bold")
        .text("Time");

    // reset x-axis, y-axis
    replaceTicks(".axis--hist--x g", getTimeArray()) ;
    removeTicksByDistance(".axis--hist--x g", 24) ;
    replaceTicks(".axis--hist--y g", getScalarArray()) ;
    removeTicksByDistance(".axis--hist--y g", 24) ;


    d3.selectAll(".axis--hist--y path").each(function() {
        d3.select(this).remove() ;
    }) ;

    d3.selectAll(".axis--hist--x path").each(function() {
        d3.select(this).remove() ;
    }) ;

    function zoomed() {
        console.log("zooomed") ;
    }

    function zoomstart() {
        console.log("zoomstart") ;
     }

    function slided(d) {
        zoom.scaleTo(svg, scaleConvert(d3.select(this).property("value")));
    }

    $("#histogram_zoom_back").unbind().click(function() {
        zoomend("zoom_back") ;
    }) ;

    function zoomend(d) {
        console.log("zoomend") ;
        var currentTransform ;
        if (d === "zoom_back") {
            currentTransform = {"x":0, "y": (fixedHeight - height), "k":1}
        } else {
            // https://gist.github.com/KarolAltamirano/b54c263184be0516a59d6baf7f053f3e
            var preTmp = transFormApply(d3.select(this).attr("transform"), undefined, undefined, undefined, true) ;
            var curTmp = d3.event.transform ;
            newTx = cx - (cx - preTmp[0]) * curTmp.k / preTmp[2];
            newTy = cy - (cy - preTmp[1]) * curTmp.k / preTmp[2];
            currentTransform = {"x":newTx, "y":newTy, "k":curTmp.k}
        }
        main.attr("transform", "translate(" + currentTransform.x+"," +currentTransform.y+ ") scale(" + currentTransform.k + ")");

        d3.select(".mdm-rect-box-cover").attr("transform", "translate(" + currentTransform.x+"," +currentTransform.y+ ") scale(" + currentTransform.k + ")");

        d3.select(".axis--hist--y").attr("transform", "translate(0,"+currentTransform.y+") scale("+currentTransform.k+")");
        d3.select(".axis--hist--x").attr("transform", "translate("+currentTransform.x+","+fixedHeight+") scale("+currentTransform.k+")");

        d3.selectAll(".axis--hist--y g").each(function() {
            d3.select(this).attr("transform", transFormApply(d3.select(this).attr("transform"), undefined, undefined, 1 / currentTransform.k)) ;
        }) ;

        d3.selectAll(".axis--hist--x g").each(function() {
            d3.select(this).attr("transform", transFormApply(d3.select(this).attr("transform"), undefined, undefined, 1 / currentTransform.k)) ;
        }) ;

        slider.property("value", scaleReverse(currentTransform.k));
        $("#hist-clip-highlight").attr("transform", $("#hist-clip-opacity").attr("transform")) ;
    }

    let dragMode = -1; // -1: inactive, 0:started, 1:moved

    function dragstarted(d) {
        console.log("dragstarted");
        const e = d3.event.sourceEvent;
        if(e.ctrlKey){
            if(e.target.nodeName!=='rect')
                return 1;

            const bin =  $(e.target);
            const d = bin.prop('data');

            Window.PPI['selected-time-id'] = d[0] ;
            coverShadow(d[0]) ;
        } else {
            dragMode = 0;
            d3.event.sourceEvent.stopPropagation();
            d3.select(this).classed("dragging", true);
        }
    }

    function dragged(d) {
        console.log("dragged") ;

        dragMode = 1;

        var scale = scaleConvert(d3.select("#range_input").property("value"))
        var trans = transFormApply($(this).attr("transform"), undefined, undefined, undefined, true) ;
        var x = trans[0];
        var y = trans[1];
        x += d3.event.dx;
        y += d3.event.dy;
        // bins of histogram
        d3.select(this).attr("transform", "translate(" + x + "," + y + ") scale(" + scale + ")" );

        var yTrans = transFormApply($(".axis--hist--y").attr("transform"), undefined, undefined, undefined, true) ;
        var yX = yTrans[0] ;
        var yY = yTrans[1] ;
        yX += d3.event.dx ;
        yY += d3.event.dy ;
        var v = transFormApply($(".axis--hist--y").attr("transform"), 0, yY, scale) ;
        d3.select(".axis--hist--y").attr("transform", v);

        var xTrans = transFormApply($(".axis--hist--x").attr("transform"), undefined, undefined, undefined, true) ;
        var xX = xTrans[0] ;
        var xY = xTrans[1] ;
        xX += d3.event.dx ;
        xY += d3.event.dy ;
        var v = transFormApply($(".axis--hist--x").attr("transform"), xX, undefined, scale) ;
        d3.select(".axis--hist--x").attr("transform", v);

        if ( $(".mdm-rect-box-cover").length > 0) {
            var coverTrans = transFormApply($(".mdm-rect-box-cover").attr("transform"), undefined, undefined, undefined, true) ;
            var coverX = coverTrans[0] ;
            var coverY = coverTrans[1] ;
            coverX += d3.event.dx;
            coverY += d3.event.dy;
            // bins of histogram
            d3.select(".mdm-rect-box-cover").attr("transform", "translate(" + coverX + "," + coverY + ") scale(" + scale + ")" );
        }

         $("#hist-clip-highlight").attr("transform", $("#hist-clip-opacity").attr("transform")) ;
    }

    $("#hidden-click-event-APPI").unbind().click(function() {
        binClick($("#" + $(this).prop("hist-id"))) ;
    }) ;

    function binClick(ssB) {
        if(selectedBin){
            d3.select(selectedBin[0])
                .style("stroke-width", 0.1)
                .attr("bin-selected", "off");
        }

        $("#RendererContainer").loading({theme: "light"});

        Window.PPI['selected-bin-id'] = ssB.attr("id")
        // d3.selectAll("[name=bin]").style("stroke-width", 0.1).attr("bin-selected", "off");
        const parent = ssB.parent()[0];

        // console.log(crosshairV);
        parent.append(crosshairV.node());
        parent.append(crosshairH.node());
        parent.append(ssB[0]);

        d3.select(ssB[0])
            .style("stroke-width", 2)
            .attr("bin-selected", "on");

        const cX = parseFloat(ssB.attr('x'))+x.bandwidth()/2;
        const cY = parseFloat(ssB.attr('y'))+y.bandwidth()/2;

        crosshairV
            .attr('x1',cX)
            .attr('x2',cX);
        crosshairH
            .attr('y1',cY)
            .attr('y2',cY);

        const d = ssB.prop('data');

        var actual_scalar = getScalarArray(parseInt(d[1]));
        if (fieldData.hasOwnProperty("Time") ) {
            var actual_time = fieldData['Time'].Values[parseInt(d[0])] ;
        } else {
            var actual_time = fieldData['t'].Values[parseInt(d[0])] ;
        }
        var actual_threshold = $("#hist-threshold :selected").text()
        var idx_threshold = $("#hist-threshold :selected").val()
        var backMsg = 'updateUnstructuredGrid:{"FieldData": ' +
            '{"idx_time": [' + d[0] + '], "actual_time": [' + actual_time + '], ' +
            '"idx_scalar": [' + d[1] + '], "actual_scalar": [' + actual_scalar + '],' +
            '"idx_threshold": [' + idx_threshold + '], "actual_threshold": [' + actual_threshold + '],' +
            '"PPI": [' + d[2] + '] }}';

        showupLineBoxplot(d[0]) ;
        console.log(backMsg) ;

        $("#histogram-notification-placeholder-default").html("") ;
        $("#histogram-notification-placeholder-0").html($("#histogram-notification").attr("data-pattern-0").replace("{Scalar}", actual_scalar.toFixed(2)).replace("{Time}", actual_time.toFixed(2)).replace("{Threshold}", actual_threshold)) ;

        if (!Window.PPI['DEV']) {
            Window.socket = socket ;
            socket.send(backMsg) ;
        }

        selectedBin = ssB ;
    }

    function dragended(d) {
        console.log("dragended") ;

        const moved = dragMode===1;
        dragMode = -1;

        const e = d3.event.sourceEvent;
        if(e.ctrlKey){
            triggerCtrlV();
            return 1;
        } else {
            if(e.target.nodeName!=='rect' || moved)
                return 1;

            if(selectedBin){
                d3.select(selectedBin[0])
                    .style("stroke-width", 0.1)
                    .attr("bin-selected", "off");
            }

            selectedBin = $(e.target);
            binClick(selectedBin) ;

            d3.select(this).classed("dragging", false);
            return false;
        }
    }

    $("#histogram_zoom_back").click() ;

    // replay previous settings
    if (hist_clip_highlight_transform) {
        $("#hist-clip-highlight").attr("transform", hist_clip_highlight_transform) ;
        $("#hist-clip-opacity").attr("transform", hist_clip_highlight_transform) ;
        var y_g_trans = transFormApply(hist_clip_highlight_transform, undefined, undefined, undefined, true) ;
        slider.property("value", scaleReverse(y_g_trans[2]));
    }
    if (mdm_rect_box_cover_transform) {
        $(".mdm-rect-box-cover").attr("transform", mdm_rect_box_cover_transform);
    }
    if (axis_hist_y) {
        $(".axis--hist--y").attr("transform", axis_hist_y) ;

    }
    if (axis_hist_x) {
        $(".axis--hist--x").attr("transform", axis_hist_x) ;
    }

    if (axis_hist_y_g) {
        var y_g_trans = transFormApply(axis_hist_y_g, undefined, undefined, undefined, true) ;
        d3.selectAll(".axis--hist--y g").each(function() {
            d3.select(this).attr("transform", transFormApply(d3.select(this).attr("transform"), undefined, undefined, y_g_trans[2])) ;
        }) ;
    }

    if (axis_hist_x_g) {
        var x_g_trans = transFormApply(axis_hist_x_g, undefined, undefined, undefined, true) ;
        d3.selectAll(".axis--hist--x g").each(function() {
            d3.select(this).attr("transform", transFormApply(d3.select(this).attr("transform"), undefined, undefined, x_g_trans[2])) ;
        }) ;
    }

    $("#hidden-mdm-add-window").click() ;
    if (Window.PPI['selected-bin-id']) {
        // d3.select("#" + Window.PPI['selected-bin-id']).dispatch("click") ;
        jClick("#" + Window.PPI['selected-bin-id']) ;
    }
    $("#histogram_viz").loading("stop");
}

// draw a curve for histogram of each bins when hovering it
function drawCurveLine(key, data, iComponent, reliability, pp) {
    tmp = getRangeOfPPI();
    let lower = tmp[0]
    let upper = tmp[1]

    // data = trim(data, lower, upper) ;
    let svg = d3.select("#" + key);
    let x = d3.scaleLinear().range([0, 240]);
    let y = d3.scaleLinear().range([132, 0]);
    let dl = d3.line()
        .x(function (d, i) {
            return x(i * Window.PPI['thresholdRatio']);
        })
        .y(function (d) {
            return y(d);
        });

    // Scale the range of the data
    x.domain(d3.extent(data, function (d, i) {
        return i * Window.PPI['thresholdRatio'];
    })).nice();

    // start from 0
    // y.domain([lower, upper]);
    y.domain([0, Math.max(...data)]);

    svg.append("path")
        .attr("class", "line2")
        .attr("d", dl(data))
        .style("stroke-width", 2)
        .style("stroke", "red")
        .attr("transform", "translate(28" + ", 4" + ")");

    let max_threshold_window = parseInt($("#threshold-window").val())
    var idx_first = x(iComponent * Window.PPI['thresholdRatio']);
    var idx_sec = x(max_threshold_window * Window.PPI['thresholdRatio'])

    addSvgLine(svg, idx_first, 0, idx_first, 136, "translate(28, 0)") ;
    addSvgLine(svg, idx_sec, 0, idx_sec, 136, "translate(28, 0)")

    svg.append("rect")
        .attr("class", "zero")
        .attr("x", idx_first)
        .attr("y", 0)
        .attr("height", 136)
        .attr("width", idx_sec - idx_first)
        .style("opacity", 0.3)
        .attr("transform", "translate(28, 0)");

    svg.append("text")
        .attr("x", 140)
        .attr("y", 16)
        .attr("text-anchor", "middle")
        .style("font-size", "12px")
        .style("font-family", "sans-serif")
        // .text("reliability: " + reliability.toFixed(2) + ", PPI: " + pp);
        .text("similarity: " + reliability.toFixed(2) + ", PPI: " + pp);

    svg.append("g")
        .attr("transform", "translate(28, 136)")
        .call(d3.axisBottom(x));

    // Add the Y Axis
    svg.append("g")
        .attr("transform", "translate(28" + ", 4" + ")")
        .call(d3.axisLeft(y));
}