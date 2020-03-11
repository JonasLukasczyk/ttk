// define a global variable to control everything
Window.PPI = {
    "DEV": false,
    // the id of the bin selected in the histogram view
    "selected-bin-id": "",
    // it MUST be only one value
    "APPIAttrName": "",
    // color range
    "color-green": ["#edf8e9", "#bae4b3", "#74c476", "#31a354", "#006d2c"],
    "color-gray": ["#f7f7f7", "#d9d9d9", "#bdbdbd", "#969696", "#636363"],
	"color-red": ["#fee5d9", "#fcae91", "#fb6a4a", "#de2d26", "#a50f15"],
	"color-blue": ["#eff3ff", "#bdd7e7", "#6baed6", "#3182bd", "#08519c"],
	// the number of elements for the width in the histogram
	"histogram-width": 0,
	// the number of elements for the height in the histogram
    "histogram-height": 0,
	// the number of elements for the height in the SDM histogram
	"sdm-histogram-width": 0,
	// the received ImageData object
    "image-object": null,
    "numberOfThreshold-histogram": 0,
    "numberOfThreshold-boxplot": 0,
    "boxplot-element-visibility": {
        "box_line": false,
       // "box_box": false,
        "box_polygon": true,
        "box_optimal": false
	},
    "persistence_pairs_range": {
        "is_custom": false,
        "custom_upper": -1, 
		"custom_lower": -1, 
		// "threshold index" => [minPPI, maxPPI] 
    },  
	"histogram-mode": "multi", // multi or single
	"thresholdRatio": 1,
	"selected-time-id": 0,
	// {min_persistence_pairs}_{left_threshold}_{right_threshold}_{max_persistence_pairs}
	"histogram_frame_data": { },
	// the mode: hide/show the hover in the histogram
	"X-mode": 1,
	"boxplot-timestamp-range-mode": {
		"enable": false,
		"start": 2,
		"end": 12, // inclusive
	},
} ;

let ttk, ttk_render;
let RENDERER = new vtkRenderer('RendererContainer', 581, 321);

// reset the content of boxplot
function resetBoxplot() {
	d3.select("#box_dataviz *").remove() ;
    $("#customSwitches--x").prop('checked', false);
    $("#customSwitches--y").prop('checked', true);
    $("#boxplot-checkbox-lines").prop("checked", false);
    $("#boxplot-select-lines").html("<option>lines</option>");
    $("#boxplot-checkbox-polygon").prop("checked", true);
    $("#download_raw_data").attr("href", "") ;
    $("#notification_xxyy").text("0-0, 0-0");
    $("#brush_mode").text("view mode").attr("mode", "view");
}

function drawBoxPlot() {
	// redraw the curve
    drawBoxplotCurve(Window.PPI['image-object'].FieldData.PersistenceCurves.NumberOfComponents, 
    				 Window.PPI['image-object'].FieldData.PersistenceCurves.Values);
    $("#hidden-notification_xxyy").click();  // reset the range of x-axies and y-axies
    $("[name='box_line']").attr("visibility", "hidden");
    //$("[name='box_box']").attr("visibility", "hidden");    
}

function resetHist() {
	// fill in the multi-view
    $('#hist-threshold').html("").attr("title", 'for ' + Window.PPI['APPIAttrName']);
    for (let i = 0; i < Window.PPI['numberOfThreshold-histogram']; i++) {
        $('#hist-threshold').append('<option class="histogram-selector" value=' + i + '>' + i * Window.PPI['thresholdRatio'] + '</option>');
    }

    $('#threshold-window').html("").attr("title", 'for ' + Window.PPI['APPIAttrName']);
    for (let i = 0; i < Window.PPI['numberOfThreshold-histogram']; i++) {
    	if ( i == Math.floor(Window.PPI['numberOfThreshold-histogram'] * 0.1) ) {
        	$('#threshold-window').append('<option class="histogram-selector" selected value=' + i + '>' + i * Window.PPI['thresholdRatio'] + '</option>');
    	} else {
	        $('#threshold-window').append('<option class="histogram-selector" value=' + i + '>' + i * Window.PPI['thresholdRatio'] + '</option>');
    	}
    }

	// fill in the single-view
	$("#hist-time").html("");
    for (let i = 0; i < Window.PPI['histogram-width']; i++) {
        $('#hist-time').append('<option class="histogram-selector" value=' + i + '>' + i + '</option>');
    }
}

// callback when getting an ImageData object from Paraview
function objectCallback(msg) {
    if (msg.VtkDataObjectType.Values[0] !== 2) {
        alert("MUST be ImageData") ;
    }
    // initialize the global variable 
    Window.PPI['image-object'] = msg ;
	Window.PPI['histogram-width'] = msg.Extent.Values[1] + 1 ;
    Window.PPI['histogram-height'] = msg.Extent.Values[3] + 1 ;
    // MUST contain one element
    Window.PPI['APPIAttrName'] = Object.keys(msg.PointData)[0] ;
    Window.PPI['numberOfThreshold-histogram'] = msg.PointData[Window.PPI['APPIAttrName']].NumberOfComponents ;
    Window.PPI['numberOfThreshold-boxplot'] = msg.FieldData.PersistenceCurves.NumberOfComponents ;
	Window.PPI['sdm-histogram-width'] = Window.PPI['numberOfThreshold-histogram'] ;
	// it should be a integer otherwise error may occur
	Window.PPI['thresholdRatio'] = Window.PPI['numberOfThreshold-boxplot'] / Window.PPI['numberOfThreshold-histogram'] ;
    resetBoxplot() ;
    drawBoxPlot() ;
    resetHist() ;

    var mCount = 20 ;
    if (Window.PPI['histogram-width'] <= mCount) {
    	Window.PPI['histogram-mode'] = "multi" ;  // show the single-view at first
    } else {
    	Window.PPI['histogram-mode'] = "single" ; // show the multi-view at first
    }
	
	triggerCtrlV() ;

    $('body').plainOverlay('hide');

    // https://jqueryui.com/slider/#range
	$( "#slider-range" ).unbind().slider({
		range: true,
		min: 0,
		orientation: "horizontal",
		max: Window.PPI['histogram-width'] - 1,
		values: [0, Window.PPI['histogram-width'] - 1],
		slide: function( event, ui ) {
			$("#histogram_zoom_back").click() ;
			$("#picker_left").text(ui.values[0]) ;
			$("#picker_right").text(ui.values[1]) ;
			Window.PPI['boxplot-timestamp-range-mode']['enable'] = true ;
			Window.PPI['boxplot-timestamp-range-mode']['start'] = ui.values[0] ;
			Window.PPI['boxplot-timestamp-range-mode']['end'] = ui.values[1] ;
			resetBoxplot() ;
    		drawBoxPlot() ;
    		// Add the box window over histogram's bins
    		$("#hidden-mdm-add-window").click() ;
		},
		create: function() {
			$("#picker_left").text(0) ;
			$("#picker_right").text(Window.PPI['histogram-width'] - 1) ;
			$("#histogram-time-picker-default").html("") ;
		}
	});
	
}

function Connect() {
	$('body').plainOverlay("show");
	if (ttk && ttk.getSocketObject().readyState !== 3) {
		alert("please try it again after closing current connection");
		return;
	}
	Window.PPI['DEV'] = false;
	var btn = $("#connect");
	btn.html('<span class="spinner-border spinner-border-sm" role="status" aria-hidden="true"></span>Loading...');
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
			console.log("on_clbinose");
		},
		objectCallback,
		ip = $("#msg-host").val(), false);

	ttk_render = new ttkWebSocketIO(PORT_RENDER, function () {
		console.log("on_open for render");
	}, () => {}, () => {}, () => {}, obj => RENDERER.setScene(obj), $("#msg-host").val(), false);
}

function LoadTest() {
	Window.PPI['DEV'] = true;
	var testdataset = loadTestDataFromString();
	objectCallback(testdataset);
	$("#load_test").attr("disabled", "disabled");
	$("#connect").attr("disabled", true);
}

function triggerCtrlV() {
	if (Window.PPI['histogram-mode'] == "multi") {
		Window.PPI['histogram-mode'] = "single" ;
		$("[element-show='mdm']").css("display", "none") ;
		$("[element-show='sdm']").css("display", "") ;
		drawSDMHistogram(Window.PPI['selected-time-id'], Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)) ;
	} else {
		Window.PPI['histogram-mode'] = "multi" ;
		$("[element-show='mdm']").css("display", "") ;
		$("[element-show='sdm']").css("display", "none") ;
		drawHistogram(parseInt($("#hist-threshold").val()), Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)) ;
	}
}

$(document).keydown(function (e) {
	// alt + event
	if (e.ctrlKey && e.shiftKey) {
		switch(e.key) {
			case "ArrowRight":
				// single view
				if (Window.PPI['histogram-mode'] == "multi") {
					
				} else {
					$("#hidden-sdm-move-right-next").click() ;
				}
				break;
			case "ArrowLeft":
				// single view
				if (Window.PPI['histogram-mode'] == "multi") {
					
				} else {
					$("#hidden-sdm-move-right-prev").click() ;
				}
				break;
		}
		return ;
	}

	// control + event
	if (!e.ctrlKey) {
		return ;
	}
	if (!(typeof Window.PPI['histogram-height'] != 'undefined' && typeof Window.PPI['histogram-width'] != 'undefined')) {
        return ;
    }
	var selected = false;
	var g_idx = 0;
	if ($("[bin-selected=on]").length > 0) {
		g_idx = parseInt($("[bin-selected=on]").attr("id").replace("hist-bin-", ""));
		var j = Math.floor(g_idx / Window.PPI['histogram-width']);
		var i = g_idx % Window.PPI['histogram-width'];
		selected = true;
	}

	switch (e.key) {
		case "v":
			triggerCtrlV() ;
			break ;

		case "x":
			Window.PPI["X-mode"] = 1 - Window.PPI["X-mode"] ;
			break ;

		case "b":
			$("#hidden-brush-mode").click();
			break;

		case "ArrowRight":
			// single view
			if (Window.PPI['histogram-mode'] == "multi") {
				// multi view
				if (selected) {
					if (i + 1 <= Window.PPI['histogram-width'] - 1) {
						d3.select("#hist-bin-" + (g_idx + 1)).dispatch("click");
					}
				}
			} else {
				$("#hidden-sdm-move-next").click() ;
			}
			break;

		case "ArrowLeft":
			// single view
			if (Window.PPI['histogram-mode'] == "multi") {
				// multi view
				if (selected) {
					if (i - 1 >= 0) {
						d3.select("#hist-bin-" + (g_idx - 1)).dispatch("click");
					}
				}
			} else {
				$("#hidden-sdm-move-prev").click() ;
			}
			break;

		case "ArrowUp":
			if (selected) {
				if (j + 1 <= Window.PPI['histogram-height'] - 1) {
					d3.select("#hist-bin-" + (g_idx + Window.PPI['histogram-width'])).dispatch("click");
				}
			}
			break;

		case "ArrowDown":
			if (selected) {
				if (j - 1 >= 0) {
					d3.select("#hist-bin-" + (g_idx - Window.PPI['histogram-width'])).dispatch("click");
				}
			}
			break;
	}
});

$("#hist-threshold").change(function () {
    v0 = parseInt($($("#hist-threshold option:selected")[0]).val()) ;
    v1 = parseInt($($("#threshold-window option:selected")[0]).val()) ;
    $("#threshold-window").attr("data-value", v0 + v1 - parseInt($(this).attr("data-value"))) ;
    $("#threshold-window").val(v0 + v1 - parseInt($(this).attr("data-value"))) ;
    $(this).attr("data-value", v0) ;
    $('#histogram-view-container').plainOverlay("show");
    $("#hist-threshold option:selected").each(function () {
    	if (Window.PPI['histogram-mode'] == "multi") {
	        drawHistogram(parseInt($(this).val()), Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    	} else {
	        drawSDMHistogram(0, Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    	}
    });
    $('#histogram-view-container').plainOverlay('hidden');
    if (Window.PPI['selected-bin-id']) {
        d3.select("#" + Window.PPI['selected-bin-id']).dispatch("click") ;
    }
    $(this).focus() ;
});

$("#threshold-window").change(function() {
    v0 = parseInt($($("#hist-threshold option:selected")[0]).val()) ;
    v1 = parseInt($($("#threshold-window option:selected")[0]).val()) ;
    if ( v0 > v1 ) {
        alert('threshold-window must be larger than hist-threshold') ;
        $(this).val($(this).attr("data-value")) ;
        return false;
    }
    $(this).attr("data-value", v1) ;
    $('#histogram-view-container').plainOverlay("show");

    $("#hist-threshold option:selected").each(function () {
        if (Window.PPI['histogram-mode'] == "multi") {
	        drawHistogram(parseInt($(this).val()), Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    	} else {
	        drawSDMHistogram(0, Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    	}
    });
    $('#histogram-view-container').plainOverlay('hidden');
    if (Window.PPI['selected-bin-id']) {
        d3.select("#" + Window.PPI['selected-bin-id']).dispatch("click") ;
    }
    $(this).focus() ;
}) ;

$("#hist-rescale").unbind().click(function() {
    tmp = getRangeOfPPIByThreshold() ;
    Window.PPI['persistence_pairs_range']['is_custom'] = true ;
    Window.PPI['persistence_pairs_range']['custom_upper'] = tmp[1] ;
    Window.PPI['persistence_pairs_range']['custom_lower'] = tmp[0] ;
    if (Window.PPI['histogram-mode'] == "multi") {
	    drawHistogram(parseInt($("#hist-threshold").val()), Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    } else {
	    drawSDMHistogram(0, Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    }

}) ;

$("#histRescaleCustom").unbind().click(function() {
    if (isNaN($("#histRescaleCustomLower").val()) || isNaN($("#histRescaleCustomUpper").val())) {
        alert("lower and upper should be number") ;
        return ;
    }
    Window.PPI['persistence_pairs_range']['is_custom'] = true ;
    Window.PPI['persistence_pairs_range']['custom_lower'] = parseFloat($("#histRescaleCustomLower").val()) ;
    Window.PPI['persistence_pairs_range']['custom_upper'] = parseFloat($("#histRescaleCustomUpper").val());
    if (Window.PPI['histogram-mode'] == "multi") {
	    drawHistogram(parseInt($("#hist-threshold").val()), Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    } else {
	    drawSDMHistogram(0, Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    }
    // re-draw the histogram
    $("#histRescaleCustom-close").click() ;
}) ;

