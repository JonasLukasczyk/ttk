// https://htmlcolorcodes.com/
// http://colorbrewer2.org/#type=diverging&scheme=RdYlBu&n=9
let DEV = false;

function customColor(reliability, ppi, max_persistence_pairs) {
    // from light to dark
    let green = ["#edf8e9", "#bae4b3", "#74c476", "#31a354", "#006d2c"]; 
    let gray = ["#f7f7f7", "#d9d9d9", "#bdbdbd", "#969696", "#636363"];
    let red = ["#fee5d9", "#fcae91", "#fb6a4a", "#de2d26", "#a50f15"];
    let range_color = green;
    if (reliability >= 0 && reliability < 0.3) {
        range_color = red;
    } else if (reliability >= 0.3 && reliability < 0.6) {
        range_color = gray;
    } else {
        range_color = green;
    }

    if (ppi == 0) {
        return "white";
    } else {
        return range_color[Math.ceil(ppi / (max_persistence_pairs / 5)) - 1];
    }
}

// Add tooltips
d3.select("body")
    .append("div")
    .attr("id", "tooltip")
    .style("background", "white")
    .style("opacity", 0.8)
    .style("color", "white")
    .style("border-radius", "5px")
    .style("position", "absolute")
    .style("z-index", "10")
    .style("visibility", "hidden");

d3.select("#tooltip").append("svg").attr("id", "tooltipSvg").style("height", "160px").style("padding-top", "4px");

function calculateReliability(items, iComponent) {
    var sum = 0;
    var sliceItems = items.slice(iComponent, iComponent + parseInt($("#rel-window").val()));
    for (var i = 0; i < sliceItems.length; i++) {
        sum += sliceItems[i];
    }
    if (Math.max(...sliceItems) == 0) {
        return 1;
    }
    return ( 2 * sum - sliceItems[0] - sliceItems[sliceItems.length - 1] ) / ( ( sliceItems.length - 1 )  * Math.max(...sliceItems) * 2 );
}

function renderHistogram(extent, data, nComponents, fieldData, iComponent, socket) {
    $("#my_dataviz").html("");
    function getArray(n) {
        let ans = [];
        for (let i = 0; i < n; i++) { ans.push("" + i); }
        return ans;
    }
    let w = extent[1] + 1;
    let h = extent[3] + 1;
    Window.hist_w = w ;
    Window.hist_h = h ;
    let myGroups = getArray(w);
    let myVars = getArray(h);

    let containerWidth = 901;
    let containerHeight = containerWidth * h / w;
    if (containerHeight > 1024) {
        containerHeight = 1024;
        containerWidth = (containerHeight) * w / h;
    }
    let margin = {
        top: 10,
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
        .domain(myGroups)
        .padding(0.01);

    svg.append("g")
        .attr('class', 'axis--hist--x')
        .attr("transform", "translate(0," + height + ")")
        .call(d3.axisBottom(x).ticks(2, "s"));

    let y = d3.scaleBand()
        .range([height, 0])
        .domain(myVars)
        .padding(0.01);

    // FLAG, needs to do
    svg.append("g")
        .attr('class', 'axis--hist--y')
        .call(d3.axisLeft(y).ticks(2, "s"));

    var txt = $("#threshold").val();
    var max_persistence_pairs = 0;
    let vvv = Math.ceil(parseFloat(txt) / (Window.persistence_num / Window.magnitude_num));
    if (txt.replace(" ", "") === "") {
        vvv = 0;
    }

    for (let i = 0; i < data.length; i++) {
        if (i % nComponents >= vvv) {
            if (data[i] > max_persistence_pairs) {
                max_persistence_pairs = data[i]
            }
        }
    }

    let vData = [];
    // extract related information
    for (let i = 0; i < h; i++) {
        for (let j = 0; j < w; j++) {
            let idx = (i * w + j) * nComponents + iComponent;
            let dd = data[idx];
            let items = data.slice((i * w + j) * nComponents, (i * w + j) * nComponents + nComponents);
            // (x-axis, y-axis, PPI, tuples, idx in the entire data, reliability)
            vData.push([j + "", i + "", dd, items, idx, calculateReliability(items, iComponent)]);
        }
    }

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
            return d3.select("#tooltip").style("top", (event.pageY - 10) + "px").style("left", (event.pageX + 10) + "px");
        })
        .on("mouseout", function () {
            return d3.select("#tooltip").style("visibility", "hidden");
        })
        .on("click", function (d, i) {
            // SELECT one bin
            d3.selectAll(".bin").style("stroke-width", 0.2).attr("bin-selected", "off");
            d3.select(this).style("stroke-width", "2").attr("bin-selected", "on");
            var actual_scalar = ((fieldData['ScalarBounds'].Values[1] - fieldData['ScalarBounds'].Values[0]) * parseInt(d[1]) / (h - 1) + fieldData['ScalarBounds'].Values[0]) ;
            var actual_time = fieldData['Time'].Values[parseInt(d[0])] ;
            var mm = 'updateUnstructuredGridWithoutUpdate:{"FieldData": ' +
                '{"idx_time": [' + d[0] + '], "actual_time": [' + actual_time + '], ' +
                '"idx_scalar": [' + d[1] + '], "actual_scalar": [' + actual_scalar + '],' +
                '"PPI": [' + d[2] + '] }}';
            $(".hist-yaxis-title").text("Scalar - " + actual_scalar.toFixed(2)) ;
            $(".hist-xaxis-title").text("Time - " + actual_time.toFixed(2)) ;
            if (!DEV) {
                Window.socket = socket ;
                socket.send(mm) ;
            } 
            console.log(mm) ;
        })
        
        svg.call(d3.zoom().on("zoom", function () {
            svg.attr("transform", d3.event.transform)
        }))
    
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
    removeNiceByKicks(".axis--hist--y g")
}

function removeNiceByKicks(id) {
    var gs = $(id) ;
    if (gs.length >= 40) {
        var size = Math.floor(gs.length / 20) ;
        for (var i = 0; i < gs.length; i ++) {
            if (i % size != 0) {
                gs[i].remove() ;
            }
        }
    }
}

$('#exampleModal').on('show.bs.modal', function (event) {
    $("#x-y-range-change").val($("#notification_xxyy").text());
});

function drawCurveLine(key, data, iComponent, reliability, max_persistence_pairs, pp) {
    let svg = d3.select("#" + key);
    let x = d3.scaleLinear().range([0, 240]);
    let y = d3.scaleLinear().range([132, 0]);

    let dl = d3.line()
        .x(function (d, i) {
            return x(i * Window.persistence_num / Window.magnitude_num);
        })
        .y(function (d) {
            return y(d);
        });

    // Scale the range of the data
    x.domain(d3.extent(data, function (d, i) {
        return i * Window.persistence_num / Window.magnitude_num;
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

    var idx_first = x(iComponent * Window.persistence_num / Window.magnitude_num);
    var idx_sec = iComponent * Window.persistence_num / Window.magnitude_num + (x.domain()[1] - x.domain()[0]) * ( (parseFloat($("#rel-window").val()) - 1) / Window.magnitude_num);
    
    if (idx_sec >= x.domain()[1] - (x.domain()[1] - x.domain()[0]) / Window.magnitude_num) {
        idx_sec = x.domain()[1] - (x.domain()[1] - x.domain()[0]) / Window.magnitude_num ;
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

$("#save-xy-axis").unbind().click(function () {
    var x = $("#x-y-range-change").val().replace(" ", "");
    if (x.split(",").length !== 2 && x.split(",")[0].split("-").length != 2 && x.split(",")[1].split("-").length != 2) {
        alert("Format must be yMin-yMax, xMin-yMax");
    } else {
        $("#hidden-notification_xxyy-zoom").click();
    }
});

function objectCallback(msg) {
    Window.object = msg;

    //reset
    if (msg.hasOwnProperty("FieldData") && msg.FieldData.hasOwnProperty("PersistenceCurves")) {
        $("#box_dataviz").html("");
        $("#box_dataviz_total").html("");

        $("#customSwitches--x").prop('checked', false);
        $("#customSwitches--y").prop('checked', true);
        $("#l1").prop("checked", false);
        $("#b1").html("<option>lines</option>");
        $("#l3").prop("checked", true);
        $("#download_raw_data").attr("href", "") ;
        $("#threshold").val("");
        $("#notification_xxyy").text("0-0, 0-0");
        $("#brush_mode").text("view mode").attr("mode", "view");
        drawBoxplotCurve(msg.FieldData.PersistenceCurves.NumberOfComponents, msg.FieldData.PersistenceCurves.Values);
        Window.persistence_num = msg.FieldData.PersistenceCurves.NumberOfComponents;

        $("[name='box_line']").attr("visibility", "hidden");
        $("[name='box_box']").attr("visibility", "hidden");
    }

    if (msg.hasOwnProperty("structureType")) {
        let name = "";
        if (msg.structureType.Values[0] === 2) {
            console.log("This is a ttkImageData");
            $("#s1").html("");
            $("#s2").html("");
            if (msg.hasOwnProperty("PointData")) {
                for (let v in msg.PointData) {
                    name = v;
                    $('#s1').html('<option value=' + v + '>' + v + '</option>');
                    if (msg['PointData'][v].hasOwnProperty("NumberOfComponents")) {
                        if (msg['PointData'][v].NumberOfComponents > 1) {
                            Window.magnitude_num = msg['PointData'][v].NumberOfComponents;
                            $("#s2").html("");
                            for (let vv = 0; vv < msg['PointData'][v].NumberOfComponents; vv++) {
                                $('#s2').append('<option class="histogram-selector" value=' + vv + '>' + vv + '</option>');
                            }
                            $("#rel-window-span").text("[2 - " + msg['PointData'][v].NumberOfComponents + "]") ;
                        }
                    }
                }
            }
            Window.APPIAttrName = name;
            $("#s1").attr("disabled", "disabled");
            if (DEV) {
                renderHistogram(msg['Extent'].Values,
                    msg['PointData'][name].Values,
                    msg['PointData'][name].NumberOfComponents,
                    msg['FieldData'],
                    0,
                    null);
            } else {
                renderHistogram(msg['Extent'].Values,
                    msg['PointData'][name].Values,
                    msg['PointData'][name].NumberOfComponents,
                    msg['FieldData'],
                    0,
                    ttk.getSocketObject());
            }
        } else if (msg.structureType.Values[0] === 1) {
            console.log("This is a ttkUnstructuredGrid");
        } else {}
        console.log(msg);
    }
}

$("#s2").change(function () {
    $("#s2 option:selected").each(function () {
        if (DEV) {
            renderHistogram(Window.object['Extent'].Values,
                Window.object['PointData'][Window.APPIAttrName].Values,
                Window.object['PointData'][Window.APPIAttrName].NumberOfComponents,
                Window.object['FieldData'],
                parseInt($(this).text()),
                null);
        } else {
            renderHistogram(Window.object['Extent'].Values,
                Window.object['PointData'][Window.APPIAttrName].Values,
                Window.object['PointData'][Window.APPIAttrName].NumberOfComponents,
                Window.object['FieldData'],
                parseInt($(this).text()),
                ttk.getSocketObject());
        }

    });
});

let ttk;

function Connect() {
    DEV = false;
    var btn = $("#connect");
    btn.html('<span class="spinner-border spinner-border-sm" role="status" aria-hidden="true"></span>Loading...')
    btn.attr("disabled", true);
    let PORT = parseInt(document.getElementById("msg").value);
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
            // btn.attr("disabled", false) ;
        },
        function () {
            console.log("on_close");
        },
        objectCallback,
        ip = $("#msg-host").val());
}

function Request() {
    if (ttk)
        ttk.send("requestData");
}

function LoadTest() {
    DEV = true;
    var testdataset = loadTestDataFromString();
    objectCallback(testdataset);
    $("#load_test").attr("disabled", "disabled");
    $("#connect").attr("disabled", true);
}

Window.visibility = {
    "box_line": false,
    "box_box": false,
    "box_polygon": true,
    "box_optimal": false
}

$("#l1").unbind().click(function () {
    if (Window.visibility['box_line'] == true) {
        $("[name='box_line']").attr("visibility", "hidden");
        Window.visibility['box_line'] = false;
    } else {
        $("[name='box_line']").attr("visibility", "show");
        Window.visibility['box_line'] = true;
    }
});

$("#l2").unbind().click(function () {
    if (Window.visibility['box_box'] == true) {
        $("[name='box_box']").attr("visibility", "hidden");
        Window.visibility['box_box'] = false;
    } else {
        $("[name='box_box']").attr("visibility", "show");
        Window.visibility['box_box'] = true;
    }

});

$("#l3").unbind().click(function () {
    if (Window.visibility['box_polygon'] == true) {
        $("[name^='box_polygon']").attr("visibility", "hidden");
        Window.visibility['box_polygon'] = false;
    } else {
        $("[name^='box_polygon']").attr("visibility", "show");
        Window.visibility['box_polygon'] = true;
    }
});

$("#box_zoom_back").unbind().click(function () {
    $("#hidden-zoom-back").click();
});

$(document).keydown(function(e) {
    if (e.ctrlKey) {
        if (typeof Window.hist_h != 'undefined' && typeof Window.hist_w != 'undefined') {  // FLAG, using one global variable
            var selected = false ;
            var g_idx = 0 ;
            if ($("[bin-selected=on]").length > 0) {
                g_idx =  parseInt($("[bin-selected=on]").attr("id").replace("hist-bin-", "")) ;
                var j = Math.floor(g_idx / Window.hist_w) ; 
                var i = g_idx % Window.hist_w ;
                selected = true ;
                console.log(i, j, e.key) ;
            }

            switch(e.key) {
                case "b":
                    $("#hidden-brush-mode").click();
                    break ;
                
                case "ArrowRight":
                    if (selected) {
                        if (i + 1 <= Window.hist_w - 1) {
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
                        if (j + 1 <= Window.hist_h - 1) {
                            d3.select("#hist-bin-" + (g_idx + Window.hist_w)).dispatch("click") ;
                        }
                    }    
                    break ;
                
                case "ArrowDown":
                    if (selected) {
                        if (j - 1 >= 0) {
                            d3.select("#hist-bin-" + (g_idx - Window.hist_w)).dispatch("click") ;
                        }
                    }
                    break ;
                
                default:
                    console.log("do nothing!") ;
            }
        }
    }
});

$("#brush_mode").unbind().click(function() {
    console.log("brush mode") ;
    $("#hidden-brush-mode").click(); 
})

$('#threshold').on('keypress', function (e) {
    if (e.which === 13) {
        if ($(this).val().replace(" ", "") == "") {
            $("#s2").val("0").change();
            $(".histogram-selector").prop("disabled", false);
            d3.select(".optimal-threshold").remove();
        } else if (isNaN($(this).val())) {
            alert("this is not valid number");
        } else {
            // interaction with histogram View
            if (Window.persistence_num && Window.magnitude_num) {
                let v = Math.ceil(parseFloat($(this).val()) / (Window.persistence_num / Window.magnitude_num));
                $(".histogram-selector").each(function () {
                    if (parseInt($(this).val()) < v) {
                        $(this).prop("disabled", true);
                    } else {
                        $(this).prop("disabled", false);
                    }
                });
                $("#s2").val("" + v).change();
                $("#hidden-optimal-threshold").click();
            }
        }
    }
});

$('#rel-window').on('keypress', function (e) {
    if (e.which === 13) {
        if (isNaN($(this).val())) {
            alert("this is not valid number");
        } else {
            if (typeof Window.magnitude_num != 'undefined') {
                if (parseInt($(this).val()) > Window.magnitude_num) {
                    $(this).val(Window.magnitude_num) ; 
                 }

                 if (parseInt($(this).val()) < 2) {
                    alert("must be integer and larger than 1") ;
                    $(this).val("2") ;
                } else if (DEV) {
                    renderHistogram(Window.object['Extent'].Values,
                        Window.object['PointData'][Window.APPIAttrName].Values,
                        Window.object['PointData'][Window.APPIAttrName].NumberOfComponents,
                        Window.object['FieldData'],
                        parseInt($("#s2").val()),
                        null);
                } else {
                    renderHistogram(Window.object['Extent'].Values,
                        Window.object['PointData'][Window.APPIAttrName].Values,
                        Window.object['PointData'][Window.APPIAttrName].NumberOfComponents,
                        Window.object['FieldData'],
                        parseInt($("#s2").val()),
                        ttk.getSocketObject());
                }
            }
        }
        $(this).blur();
    }
});