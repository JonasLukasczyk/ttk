// https://htmlcolorcodes.com/
// http://colorbrewer2.org/#type=diverging&scheme=RdYlBu&n=9
let DEV = false;

function customColor(reliability, ppi, max_persistence_pairs) {
    // from light to dark
    let green = ["#edf8e9", "bae4b3", "#74c476", "#31a354", "#006d2c"]; 
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
    var sliceItems = items.slice(iComponent, iComponent + items.length * parseInt($("#rel-window").val()) / 100);
    for (var i = 0; i < sliceItems.length; i++) {
        sum += sliceItems[i];
    }
    if (Math.max(...sliceItems) == 0) {
        return 1;
    }
    return sum / (sliceItems.length * Math.max(...sliceItems));
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
    let myGroups = getArray(w);
    let myVars = getArray(h);

    let width = 901;
    let height = width * h / w - 64;
    if (height > 1024) {
        height = 1024;
        width = (height + 64) * w / h;
    }
    let svg = d3.select("#my_dataviz")
        .append("svg")
        .attr("width", width)
        .attr("height", height)
        .attr("transform",
            "translate( 0, 0 )");
    
    let x = d3.scaleBand()
        .range([0, width])
        .domain(myGroups)
        .padding(0.01);

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

    svg.append("g")
        .attr("width", width)
        .call(d3.axisBottom(x));

    let y = d3.scaleBand()
        .range([height, 0])
        .domain(myVars)
        .padding(0.01);

    svg.append("g")
        .call(d3.axisLeft(y));

    svg.selectAll()
        .data(vData, function (d) {
            return "";
        })
        .enter()
        .append("rect")
        .attr("class", "bin")
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
            d3.selectAll(".bin").style("stroke", "black").style("stroke-width", 0.2);
            d3.select(this).style("stroke", "black").style("stroke-width", "2");
            var mm = 'updateUnstructuredGridWithoutUpdate:{"FieldData": ' +
                '{"idx_timestamp": [' + d[0] + '], "actual_timestamp": [' + fieldData['Time'].Values[parseInt(d[0])] + '], ' +
                '"idx_scale": [' + d[1] + '], "actual_scale": [' + ((fieldData['ScalarBounds'].Values[1] - fieldData['ScalarBounds'].Values[0]) * parseInt(d[1]) / (h - 1) + fieldData['ScalarBounds'].Values[0]) + '],' +
                '"PPI": [' + d[2] + '] }}';
            if (!DEV) {
                // send the empty unStructuredGrid with field data
                Window.socket = socket;
                //Window.socket.send('updateUnstructuredGrid:{}');
                socket.send(mm) ;
            } else {
                console.log(mm) ;
            }
        });
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
    var idx_sec = iComponent * Window.persistence_num / Window.magnitude_num + (x.domain()[1] - x.domain()[0]) * parseFloat($("#rel-window").val()) / 100;
    if (idx_sec > x.domain()[1]) {
        idx_sec = x.domain()[1];
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
        .text("reliability: " + reliability.toFixed(2) + ", max_PP: " + max_persistence_pairs + ", PP: " + pp);

    svg.append("g")
        .attr("transform", "translate(28, 136)")
        .call(d3.axisBottom(x));

    // Add the Y Axis
    svg.append("g")
        .attr("transform", "translate(28" + ", 4" + ")")
        .call(d3.axisLeft(y));
}

$("#save-xy-axis").click(function () {
    var x = $("#x-y-range-change").val().replace(" ", "");
    if (x.split(",").length !== 2 && x.split(",")[0].split("-").length != 2 && x.split(",")[1].split("-").length != 2) {
        alert("Format must be yMin-yMax, xMin-yMax");
    } else {
        $("#hidden-notification_xxyy-zoom").click();
    }
});

function objectCallback(msg) {
    //reset
    if (msg.hasOwnProperty("FieldData") && msg.FieldData.hasOwnProperty("PersistenceCurves")) {
        $("#box_dataviz").html("");
        $("#box_dataviz_total").html("");

        $("#customSwitches--x").prop('checked', false);
        $("#customSwitches--y").prop('checked', true);
        $("#l1").prop("checked", false);
        $("#b1").html("<option>lines</option>");
        $("#l3").prop("checked", true);
        $("#threshold").val("");
        $("#notification_xxyy").text("0-0, 0-0");
        $("#notification_mode").text("view mode");
        drawBoxplotCurve(msg.FieldData.PersistenceCurves.NumberOfComponents, msg.FieldData.PersistenceCurves.Values);
        Window.persistence_num = msg.FieldData.PersistenceCurves.NumberOfComponents;

        $("[name='box_line']").attr("visibility", "hidden");
        $("[name='box_box']").attr("visibility", "hidden");
    }

    if (msg.hasOwnProperty("structureType")) {
        Window.msg = msg;
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
                        }
                    }
                }
            }
            Window.nameX = name;
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
            renderHistogram(Window.msg['Extent'].Values,
                Window.msg['PointData'][Window.nameX].Values,
                Window.msg['PointData'][Window.nameX].NumberOfComponents,
                Window.msg['FieldData'],
                parseInt($(this).text()),
                null);
        } else {
            renderHistogram(Window.msg['Extent'].Values,
                Window.msg['PointData'][Window.nameX].Values,
                Window.msg['PointData'][Window.nameX].NumberOfComponents,
                Window.msg['FieldData'],
                parseInt($(this).text()),
                ttk.getSocketObject());
        }

    });
});

let ttk, ttk3;

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

// function Close() {
//     if (ttk)
//         ttk.close() ;
// }

// function Send() {
//     if (ttk)
//         ttk.send(document.getElementById("txt").value) ;
// }

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

$("#l1").click(function () {
    if (Window.visibility['box_line'] == true) {
        $("[name='box_line']").attr("visibility", "hidden");
        Window.visibility['box_line'] = false;
    } else {
        $("[name='box_line']").attr("visibility", "show");
        Window.visibility['box_line'] = true;
    }
});

$("#l2").click(function () {
    if (Window.visibility['box_box'] == true) {
        $("[name='box_box']").attr("visibility", "hidden");
        Window.visibility['box_box'] = false;
    } else {
        $("[name='box_box']").attr("visibility", "show");
        Window.visibility['box_box'] = true;
    }

});

$("#l3").click(function () {
    if (Window.visibility['box_polygon'] == true) {
        $("[name^='box_polygon']").attr("visibility", "hidden");
        Window.visibility['box_polygon'] = false;
    } else {
        $("[name^='box_polygon']").attr("visibility", "show");
        Window.visibility['box_polygon'] = true;
    }
});

$("#box_zoom_back").click(function () {
    $("#hidden-zoom-back").click();
});

d3.select("body").on('keydown', function () {
    if (d3.event.ctrlKey) {
        if (d3.event.key == "b") {
            $("#hidden-brush-mode").click();
        }
    }
});

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
            if (DEV) {
                renderHistogram(Window.msg['Extent'].Values,
                    Window.msg['PointData'][Window.nameX].Values,
                    Window.msg['PointData'][Window.nameX].NumberOfComponents,
                    Window.msg['FieldData'],
                    parseInt($("#s2").val()),
                    null);
            } else {
                renderHistogram(Window.msg['Extent'].Values,
                    Window.msg['PointData'][Window.nameX].Values,
                    Window.msg['PointData'][Window.nameX].NumberOfComponents,
                    Window.msg['FieldData'],
                    parseInt($("#s2").val()),
                    ttk.getSocketObject());
            }
        }
        $(this).blur();
    }
});