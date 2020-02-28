// define a global variable to control everything
Window.PPI = {
    "DEV": false,
    "selected-bin-id": "",
    "APPIAttrName": "",
    "color-green": ["#edf8e9", "#bae4b3", "#74c476", "#31a354", "#006d2c"],
    "color-gray": ["#f7f7f7", "#d9d9d9", "#bdbdbd", "#969696", "#636363"],
    "color-red": ["#fee5d9", "#fcae91", "#fb6a4a", "#de2d26", "#a50f15"],
    "histogram-width": 0,
    "histogram-height": 0,
    "image-object": null,
    "numberOfThreshold-histogram": 0,
    "numberOfThreshold-boxplot": 0,
    "boxplot-element-visibility": {
        "box_line": false,
        "box_box": false,
        "box_polygon": true,
        "box_optimal": false
    }
} ;

// Add tooltips for histogram
d3.select("body")
    .append("div")
    .attr("id", "tooltip")
    .style("background", "white")
    .style("opacity", 0.8)
    .style("color", "white")
    .style("border-radius", "5px")
    .style("position", "absolute")
    .style("z-index", "10")
    .style("visibility", "hidden")
    .append("svg")
    .attr("id", "tooltipSvg")
    .style("height", "160px")
    .style("padding-top", "4px");

 // Add tooltips for boxplot
 d3.select("body")
    .append("div")
    .attr("id", "tooltipBoxplot")
    .style("background", "black")
    .style("opacity", 0.6)
    .style("color", "white")
    .style("border-radius", "5px")
    .style("padding", "10px")
    .style("position", "absolute")
    .style("z-index", "10")
    .style("visibility", "hidden");

// items: the # of bins over threshold, in the test dataset, it would be 50
// iComponent: the start index of calculating, the range of the calculated window would be [iCompent, iCompent + window.size] 
function calculateReliability(items, iComponent, windowSize) {
    var sliceItems = items.slice(iComponent, iComponent + windowSize), sum = 0;
    for (var i = 0; i < sliceItems.length; i++) {
        sum += sliceItems[i];
    }
    if (Math.max(...sliceItems) === 0) {
        return 1;
    }
    // get the multiple trpezoid area
    return ( 2 * sum - sliceItems[0] - sliceItems[sliceItems.length - 1] ) / ( ( sliceItems.length - 1 )  * Math.max(...sliceItems) * 2 );
}

// callback when getting an ImageData object from Paraview
function objectCallback(msg) {
    if (msg.VtkDataObjectType.Values[0] !== 2) {
        alert("MUST be ImageData") ;
    }
    // MUST be ImageData
    // initialize the global variable 
    Window.PPI['histogram-width'] = msg.Extent.Values[1] + 1 ;
    Window.PPI['histogram-height'] = msg.Extent.Values[3] + 1 ;
    Window.PPI['image-object'] = msg ;
    // MUST contain one element
    Window.PPI['APPIAttrName'] = Object.keys(msg.PointData)[0] ;
    Window.PPI['numberOfThreshold-histogram'] = msg.PointData[Window.PPI['APPIAttrName']].NumberOfComponents ;
    Window.PPI['numberOfThreshold-boxplot'] = msg.FieldData.PersistenceCurves.NumberOfComponents ;
    Window.PPI['image-object'] = msg ;

    // clear action
    d3.select("#box_dataviz *").remove() ;
    d3.select("#box_dataviz_total *").remove() ;
    $("#customSwitches--x").prop('checked', false);
    $("#customSwitches--y").prop('checked', true);
    $("#boxplot-checkbox-lines").prop("checked", false);
    $("#boxplot-select-lines").html("<option>lines</option>");
    $("#boxplot-checkbox-polygon").prop("checked", true);
    $("#download_raw_data").attr("href", "") ;
    $("#threshold").val("");
    $("#notification_xxyy").text("0-0, 0-0");
    $("#brush_mode").text("view mode").attr("mode", "view");
    drawBoxplotCurve(msg.FieldData.PersistenceCurves.NumberOfComponents, msg.FieldData.PersistenceCurves.Values);
    $("#hidden-notification_xxyy").click();
    
    $("[name='box_line']").attr("visibility", "hidden");
    $("[name='box_box']").attr("visibility", "hidden");    
    // END clear action

    $("#hist-threshold").html("");
    $("#hist-threshold").attr("title", "");
    $('#hist-threshold').attr("title", 'for ' + Window.PPI['APPIAttrName']);
    for (let vv = 0; vv < msg['PointData'][Window.PPI['APPIAttrName']].NumberOfComponents; vv++) {
        $('#hist-threshold').append('<option class="histogram-selector" value=' + vv + '>' + vv + '</option>');
    }
    $("#rel-window").attr("title", "range: [2 - " + msg['PointData'][Window.PPI['APPIAttrName']].NumberOfComponents + "]") ;
    drawHistogram(0, Window.PPI['DEV']? null: ttk.getSocketObject()) ;
    triggerEnterInput("#rel-window") ;

    $('body').plainOverlay('hide');
}

// iComponent: used for determining the # of bins and start point of the shade window
// socket: webSocketIO object
// returned: used for retrieving data purely
function drawHistogram(iComponent, socket) {
    let data = Window.PPI['image-object']['PointData'][Window.PPI['APPIAttrName']].Values ;
    let nComponents = Window.PPI['numberOfThreshold-histogram'] ;
    let fieldData = Window.PPI['image-object']['FieldData'] ;

    // get the max PPI since iComponents
    // cut off the part where threshold is less than $("#threshold").val() 
    let startIdx = $("#threshold").val().replace(" ", "") === "" ? 0: 
                        Math.ceil(parseFloat($("#threshold").val()) / (Window.PPI['numberOfThreshold-boxplot'] / Window.PPI['numberOfThreshold-histogram']));

    var max_persistence_pairs = 0 ;
    for (let i = 0; i < data.length; i++) {
        if (i % nComponents >= startIdx && data[i] > max_persistence_pairs) {
            max_persistence_pairs = data[i] ;
        }
    }

    let vData = [];
    let dist_data = [] ;
    
    // extract information
    let window_size = parseInt($("#rel-window").val())
    for (let i = 0; i < Window.PPI['histogram-height']; i++) {
        for (let j = 0; j < Window.PPI['histogram-width']; j++) {
            let idx = (i * Window.PPI['histogram-width'] + j) * nComponents + iComponent;
            let items = data.slice((i * Window.PPI['histogram-width'] + j) * nComponents, (i * Window.PPI['histogram-width'] + j) * nComponents + nComponents);
            let cal = calculateReliability(items, iComponent, window_size) ;
            // (x-axis, y-axis, PPI, tuples, idx in the entire data, reliability)
            vData.push([j + "", i + "", data[idx], items, idx, cal]);
            dist_data.push([cal, data[idx]]) ;
        }
    }

    drawLegend(dist_data, max_persistence_pairs) ;

    // draw histogram
    d3.select("#my_dataviz *").remove() ;
    let myGroups = getArray( Window.PPI['histogram-width']);
    let myVars = getArray( Window.PPI['histogram-height']);

    let containerWidth = 1201;
    let containerHeight = containerWidth * Window.PPI['histogram-height'] / Window.PPI['histogram-width'];
    if (containerHeight > 724) {
        containerHeight = 724;
    }
    let margin = {
        top: 2,
        right: 0,
        bottom: 40,
        left: 50
    } ;
    var width = containerWidth - margin.left - margin.right,
        height = containerHeight - margin.top - margin.bottom;
    let container = d3.select("#my_dataviz")
        .append("svg")
        .attr("width", containerWidth)
        .attr("height", containerHeight) ;

    var svg = container
                .append("g")
                .attr("transform",
                    "translate("+margin.left+", "+margin.top+")")
                .attr('overflow', 'hidden');

    let x = d3.scaleBand()
        .range([0, width])
        .domain(myGroups) ;

    svg.append("g")
        .attr('class', 'axis--hist--x')
        .attr("transform", "translate(0," + height + ")")
        .call(d3.axisBottom(x).ticks(2, "s"));

    let y = d3.scaleBand()
        .range([height, 0])
        .domain(myVars) ;

    svg.append("g")
        .attr('class', 'axis--hist--y')
        .call(d3.axisLeft(y).ticks(2, "s"));

    svg.selectAll()
        .data(vData)
        .enter()
        .append("rect")
        .attr("class", "bin")
        .attr("id", function(d, i) { return "hist-bin-" + i ; })
        .style("stroke-width", 0.2)
        .style("stroke", "black")
        .attr("x", function (d) {
            return x(d[0]);
        })
        .attr("y", function (d) {
            return y(d[1])
        })
        .attr("width", x.bandwidth())
        .attr("height", y.bandwidth())
        .style("fill", function (d) {
            return customColor(d[5], d[2], max_persistence_pairs);
        })
        .on("mouseover", function (d, i) {
            d3.select("#tooltipSvg").selectAll("*").remove();
            drawCurveLine("tooltipSvg", d[3], iComponent, d[5], max_persistence_pairs, d[2]);
            return d3.select("#tooltip").style("visibility", "visible");
        })
        .on("mousemove", function () {
            return d3.select("#tooltip").style("top", (d3.event.pageY - 10) + "px").style("left", (d3.event.pageX + 10) + "px");
        })
        .on("mouseout", function () {
            return d3.select("#tooltip").style("visibility", "hidden");
        })
        .on("click", function (d, i) {
            // SELECT one bin
            Window.PPI['selected-bin-id'] = $(this).attr("id")
            d3.selectAll(".bin").style("stroke-width", 0.2).attr("bin-selected", "off");
            $(this).parent()[0].append($(this)[0]) ;
            d3.select(this).style("stroke-width", 2).attr("bin-selected", "on");
            var actual_scalar = ((fieldData['ScalarBounds'].Values[1] - fieldData['ScalarBounds'].Values[0]) * parseInt(d[1]) / (Window.PPI['histogram-height'] - 1) + fieldData['ScalarBounds'].Values[0]) ;
            var actual_time = fieldData['Time'].Values[parseInt(d[0])] ;
            var backMsg = 'updateUnstructuredGrid:{"FieldData": ' +
                '{"idx_time": [' + d[0] + '], "actual_time": [' + actual_time + '], ' +
                '"idx_scalar": [' + d[1] + '], "actual_scalar": [' + actual_scalar + '],' +
                '"PPI": [' + d[2] + '] }}';

            $("#histogram-notification-placeholder-0").html($("#histogram-notification").attr("data-pattern-0").replace("{Scalar}", actual_scalar.toFixed(2)).replace("{Time}", actual_time.toFixed(2)) + ", &nbsp;") ;

            console.log(backMsg) ;
            if (!Window.PPI['DEV']) {
                Window.socket = socket ;
                socket.send(backMsg) ;
            }
     }) ;

    // Add y-axis title
    svg.append("text")
        .attr("class", "hist-yaxis-title")
        .attr("transform", "rotate(-90)")
        .attr("y", 0 - 42 )
        .attr("x", 0 - (height / 2))
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("text-anchor", "middle")
        .text("Scalar") ;

    // // Add x-axis title
    svg.append("text")
        .attr("class", "hist-xaxis-title")
        .attr("y", (height + 24) )
        .attr("x", (width / 2.5 + 110))
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("text-anchor", "middle")
        .text("Time");

    // reset x-axis, y-axis, TRICKY
    removeNiceByKicks(".axis--hist--x g") ;
    removeNiceByKicks(".axis--hist--y g") ;
}

// draw a curve for histogram of each bins when hovering it
function drawCurveLine(key, data, iComponent, reliability, max_persistence_pairs, pp) {
    let svg = d3.select("#" + key);
    let x = d3.scaleLinear().range([0, 240]);
    let y = d3.scaleLinear().range([132, 0]);

    let dl = d3.line()
        .x(function (d, i) {
            return x(i * Window.PPI['numberOfThreshold-boxplot'] / Window.PPI['numberOfThreshold-histogram']);
        })
        .y(function (d) {
            return y(d);
        });

    // Scale the range of the data
    x.domain(d3.extent(data, function (d, i) {
        return i * Window.PPI['numberOfThreshold-boxplot'] / Window.PPI['numberOfThreshold-histogram'];
    })).nice();

    y.domain([0, d3.max(data, function (d) {
        return d;
    })]);

    svg.append("path")
        .attr("class", "line2")
        .attr("d", dl(data))
        .style("stroke-width", 2)
        .style("stroke", "red")
        .attr("transform", "translate(28" + ", 4" + ")");

    var idx_first = x(iComponent * Window.PPI['numberOfThreshold-boxplot'] / Window.PPI['numberOfThreshold-histogram']);
    var idx_sec = iComponent * Window.PPI['numberOfThreshold-boxplot'] / Window.PPI['numberOfThreshold-histogram'] + (x.domain()[1] - x.domain()[0]) * ( (parseFloat($("#rel-window").val()) - 1) / Window.PPI['numberOfThreshold-histogram']);
    
    if (idx_sec >= x.domain()[1] - (x.domain()[1] - x.domain()[0]) / Window.PPI['numberOfThreshold-histogram']) {
        idx_sec = x.domain()[1] - (x.domain()[1] - x.domain()[0]) / Window.PPI['numberOfThreshold-histogram'] ;
    }
    idx_sec = x(idx_sec);

    svg.append("line")
        .attr("class", "zero")
        .attr("x1", idx_first)
        .attr("y1", 0)
        .attr("x2", idx_first)
        .attr("y2", 136)
        .style("stroke", "black")
        .style("stroke-width", 1)
        .attr("transform", "translate(28, 0)");

    svg.append("line")
        .attr("class", "zero")
        .attr("x1", idx_sec)
        .attr("y1", 0)
        .attr("x2", idx_sec)
        .attr("y2", 136)
        .style("stroke", "black")
        .style("stroke-width", 1)
        .attr("transform", "translate(28, 0)");

    svg.append("rect")
        .attr("class", "zero")
        .attr("x", idx_first)
        .attr("y", 0)
        .attr("height", 136)
        .attr("width", idx_sec - idx_first)
        .style("opacity", 0.3)
        .attr("transform", "translate(28, 0)");

    svg.append("text")
        .attr("x", 132)
        .attr("y", 16)
        .attr("text-anchor", "middle")
        // sans-serif font-family, size 16px, bolded, and underlined
        .style("font-size", "12px")
        .style("font-family", "sans-serif")
        .text("reliability: " + reliability.toFixed(2) + ", max_PPI: " + max_persistence_pairs + ", PPI: " + pp);

    svg.append("g")
        .attr("transform", "translate(28, 136)")
        .call(d3.axisBottom(x));

    // Add the Y Axis
    svg.append("g")
        .attr("transform", "translate(28" + ", 4" + ")")
        .call(d3.axisLeft(y));
}

let ttk, ttk_render;
RENDERER = new vtkRenderer('RendererContainer', 581, 321);

function Connect() {
    $('body').plainOverlay("show");
    if ( ttk && ttk.getSocketObject().readyState !== 3) {
        alert("please try it again after closing current connection") ;
        return ;
    }
    Window.PPI['DEV'] = false;
    var btn = $("#connect");
    btn.html('<span class="spinner-border spinner-border-sm" role="status" aria-hidden="true"></span>Loading...') ;
    btn.attr("disabled", true);
    let PORT = parseInt(document.getElementById("msg").value);
    let PORT_RENDER = parseInt(document.getElementById("msg-render").value);
    ttk = new ttkWebSocketIO(PORT,
        function () {
            console.log("on_open");
        },
        function () {
            alert("check if listening port is available")
            console.log("on_error");
            btn.html('Connect');
            btn.attr("disabled", false);
        },
        function (msg) { // do what you want for on_message
            console.log("browser receives a msg:", msg);
            btn.html('Connect');
            $("#load_test").attr("disabled", true);
        },
        function () {
            console.log("on_close");
        },
        objectCallback,
        ip = $("#msg-host").val());

    ttk_render = new ttkWebSocketIO(PORT_RENDER, function(){
        console.log("on_open for render") ;
     }, ()=>{}, ()=>{}, ()=>{}, obj=>RENDERER.setScene(obj), $("#msg-host").val(), true) ;
}

function Request() {
    if (ttk)
        ttk.send("requestData");
}

function LoadTest() {
    Window.PPI['DEV'] = true;
    var testdataset = loadTestDataFromString();
    objectCallback(testdataset);
    $("#load_test").attr("disabled", "disabled");
    $("#connect").attr("disabled", true);
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

$('#threshold').on('keypress', function (e) {
    if (e.which === 13) {
        if ($(this).val().replace(" ", "") == "") {
            $("#hist-threshold").val("0").change();
            $(".histogram-selector").prop("disabled", false);
            d3.select(".optimal-threshold").remove();
        } else if (isNaN($(this).val())) {
            alert("this is not valid number");
        } else {
            // interaction with histogram View
            if (Window.PPI['numberOfThreshold-boxplot'] && Window.PPI['numberOfThreshold-histogram']) {
                let v = Math.ceil(parseFloat($(this).val()) / (Window.PPI['numberOfThreshold-boxplot'] / Window.PPI['numberOfThreshold-histogram']));
                $(".histogram-selector").each(function () {
                    if (parseInt($(this).val()) < v) {
                        $(this).prop("disabled", true);
                    } else {
                        $(this).prop("disabled", false);
                    }
                });
                $("#hist-threshold").val("" + v).change();
                $("#hidden-optimal-threshold").click();
            }
        }
    }
});

function rel_window_keydown(event) {
    let ele = $("#rel-window") ;
    if (event.key === "ArrowUp") {
        ele.val(parseInt(ele.val()) + 1) ;
        triggerEnterInput("#rel-window") ;
    } else if ( event.key === "ArrowDown") {
        ele.val(parseInt(ele.val()) - 1) ;
        triggerEnterInput("#rel-window") ;
    }
    ele.focus() ;
}

$('#rel-window').on('keypress', function (e) {
    if (e.which === 13) {
        if (isNaN($(this).val())) {
            alert("this is not valid number");
        } else {
            if (typeof Window.PPI['numberOfThreshold-histogram'] != 'undefined') {
                if (parseInt($(this).val()) > Window.PPI['numberOfThreshold-histogram']) {
                    $(this).val(Window.PPI['numberOfThreshold-histogram']) ; 
                 }

                 if (parseInt($(this).val()) < 2) {
                    alert("must be integer and larger than 1") ;
                    $(this).val("2") ;
                } else  {
                    drawHistogram(parseInt($("#hist-threshold").val()), Window.PPI['DEV']? null: ttk.getSocketObject());
                } 
            }
        }
        $(this).blur();
    }
});

$('#exampleModal').on('show.bs.modal', function (event) {
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

$("#hist-threshold").change(function () {
    $('#histogram-view-container').plainOverlay("show");
    $("#hist-threshold option:selected").each(function () {
    drawHistogram(parseInt($(this).text()), Window.PPI['DEV']? null: ttk.getSocketObject()); });
    $('#histogram-view-container').plainOverlay('hidden');
    if (Window.PPI['selected-bin-id']) {
        d3.select("#" + Window.PPI['selected-bin-id']).dispatch("click") ;
    }
});

$(document).keydown(function(e) {
    if (e.ctrlKey) {
        if (typeof Window.PPI['histogram-height'] != 'undefined' && typeof Window.PPI['histogram-width'] != 'undefined') {
            var selected = false ;
            var g_idx = 0 ;
            if ($("[bin-selected=on]").length > 0) {
                g_idx =  parseInt($("[bin-selected=on]").attr("id").replace("hist-bin-", "")) ;
                var j = Math.floor(g_idx / Window.PPI['histogram-width']) ; 
                var i = g_idx % Window.PPI['histogram-width'] ;
                selected = true ;
                console.log(i, j, e.key) ;
            }

            switch(e.key) {
                case "b":
                    $("#hidden-brush-mode").click();
                    break ;
                
                case "ArrowRight":
                    if (selected) {
                        if (i + 1 <= Window.PPI['histogram-width'] - 1) {
                            d3.select("#hist-bin-" + (g_idx + 1)).dispatch("click") ;
                        }
                    }
                    break ;
                
                case "ArrowLeft":
                    if (selected) {
                        if (i - 1 >= 0) {
                            d3.select("#hist-bin-" + (g_idx - 1)).dispatch("click") ;
                        }
                    }
                    break ;
    
                case "ArrowUp":
                    if (selected) {
                        if (j + 1 <= Window.PPI['histogram-height'] - 1) {
                            d3.select("#hist-bin-" + (g_idx + Window.PPI['histogram-width'])).dispatch("click") ;
                        }
                    }    
                    break ;
                
                case "ArrowDown":
                    if (selected) {
                        if (j - 1 >= 0) {
                            d3.select("#hist-bin-" + (g_idx - Window.PPI['histogram-width'])).dispatch("click") ;
                        }
                    }
                    break ;
                
                default:
                    console.log("do nothing!") ;
            }
        }
    }
});