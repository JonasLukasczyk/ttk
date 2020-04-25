// define a global variable to control everything
Window.PPI = {
    "DEV": false,
    // the id of the bin selected in the histogram view
	"selected-bin-id": "",
	"selected-bin-id-sdm": "",
    // it MUST be only one value
    "APPIAttrName": "",
    // color range
    // "color-green": ["#edf8e9", "#bae4b3", "#74c476", "#31a354", "#006d2c"], 
    // "color-gray": ["#f7f7f7", "#d9d9d9", "#bdbdbd", "#969696", "#636363"],
	// "color-red": ["#fee5d9", "#fcae91", "#fb6a4a", "#de2d26", "#a50f15"],
	// "color-blue": ["#eff3ff", "#bdd7e7", "#6baed6", "#3182bd", "#08519c"],
	"color-green": ["#eff3ff", "#bdd7e7", "#6baed6", "#3182bd", "#08519c"], 
    "color-gray": ["#f7f7f7", "#d9d9d9", "#bdbdbd", "#969696", "#636363"],
	"color-red": ["#fee5d9", "#fcae91", "#fb6a4a", "#de2d26", "#a50f15"],
	"color-blue": ["#edf8e9", "#bae4b3", "#74c476", "#31a354", "#006d2c"],
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
		// "threshold index" => [minPPI, maxPPI],
		"medium_top": -1,
		"medium_right": -1,
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
		"time-picker-mode": "multi", // or "single"
	},
	"threshold-picker-left": -1,
	"threshold-picker-right": -1,
	"redraw-MDM-key": "",
	"redraw-SDM-key": "",
	"main-container-height": $("body").outerHeight() - $("#container-fluid-div").outerHeight() - $("#APPI-header-container").outerHeight() - 1.5 * $("#container-row").outerHeight(),
} ;

let ttk, ttk_render;
let RENDERER = new vtkRenderer('RendererContainer', $("#box_dataviz").outerWidth() - 10, 0.6 * (Window.PPI['main-container-height'] - $("#left-continer-middle").outerHeight()) - $("#container-row").outerHeight() );  // width, height

// reset the content of boxplot
function resetBoxplot() {
	d3.select("#box_dataviz *").remove() ;
    // $("#customSwitches--x").prop('checked', false);
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
    // $("#hidden-notification_xxyy").click();  // reset the range of x-axies and y-axies
    $("[name='box_line']").attr("visibility", "hidden");
}

function resetHist() {
	// fill in the multi-view
    $('#hist-threshold').html("").attr("title", 'for ' + Window.PPI['APPIAttrName']);
    for (let i = 0; i < Window.PPI['numberOfThreshold-histogram']; i++) {
    	if ( i === 0 ) {
        	$('#hist-threshold').append('<option class="histogram-selector" selected value=' + i + '>' + getThresholdByIndex(i * Window.PPI['thresholdRatio']) + '</option>');
    	} else {
        	$('#hist-threshold').append('<option class="histogram-selector" value=' + i + '>' + getThresholdByIndex(i * Window.PPI['thresholdRatio']) + '</option>');
    	}
    }

    $('#threshold-window').html("").attr("title", 'for ' + Window.PPI['APPIAttrName']);
    for (let i = 0; i < Window.PPI['numberOfThreshold-histogram']; i++) {
    	if ( i === Window.PPI['numberOfThreshold-histogram'] - 1 ) {
        	$('#threshold-window').append('<option class="histogram-selector" selected value=' + i + '>' + getThresholdByIndex(i * Window.PPI['thresholdRatio']) + '</option>');
    	} else {
	        $('#threshold-window').append('<option class="histogram-selector" value=' + i + '>' + getThresholdByIndex(i * Window.PPI['thresholdRatio']) + '</option>');
    	}
    }

	// fill in the single-view
	$("#hist-time").html("");
    for (let i = 0; i < Window.PPI['histogram-width']; i++) {
        $('#hist-time').append('<option class="histogram-selector" value=' + i + '>' + getTimeByIndex(i) + '</option>');
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

	$( "#slider-range-single" ).unbind().slider({
      range: "min",
      value: 0,
      min: 0,
      step: 1,
      max: Window.PPI['histogram-width'] - 1,
      slide: function( event, ui ) {
			$("#picker_left").text(getTimeByIndex(ui.value)) ;
			$("#picker_right").text(getTimeByIndex(ui.value)) ;
			Window.PPI['boxplot-timestamp-range-mode']['enable'] = true ;
			Window.PPI['boxplot-timestamp-range-mode']['start'] = ui.value ;
			Window.PPI['boxplot-timestamp-range-mode']['end'] = ui.value ;
			resetBoxplot() ;
    		drawBoxPlot() ;
    		// Add the box window over histogram's bins
    		$("#hidden-mdm-add-window").click() ;
      }
    });

	$("#slider-range").css("display", "") ;
    // https://jqueryui.com/slider/#range
	$( "#slider-range" ).unbind().slider({
		range: true,
		min: 0,
		orientation: "horizontal",
		max: Window.PPI['histogram-width'] - 1,
		values: [0, Window.PPI['histogram-width'] - 1],
		slide: function( event, ui ) {
			$("#picker_left").text(getTimeByIndex(ui.values[0])) ;
			$("#picker_right").text(getTimeByIndex(ui.values[1])) ;
			Window.PPI['boxplot-timestamp-range-mode']['enable'] = true ;
			Window.PPI['boxplot-timestamp-range-mode']['start'] = ui.values[0] ;
			Window.PPI['boxplot-timestamp-range-mode']['end'] = ui.values[1] ;
			
			drawBoxplotCurve(Window.PPI['image-object'].FieldData.PersistenceCurves.NumberOfComponents, 
    				 		 Window.PPI['image-object'].FieldData.PersistenceCurves.Values);
			
			// Add the box window over histogram's bins
    		$("#hidden-mdm-add-window").click() ;
		},
		create: function() {
			$("#picker_left").text(getTimeByIndex(0)) ;
			$("#picker_right").text(getTimeByIndex(Window.PPI['histogram-width'] - 1)) ;
			$("#histogram-time-picker-default").html("") ;
		}
	});

	$("#threshold-slider-range").css("display", "") ;
	// threshold picker
	$( "#threshold-slider-range" ).unbind().slider({
		range: true,
		min: 0,
		orientation: "horizontal",
		max: Window.PPI['numberOfThreshold-histogram'] - 1,
		values: [0, Window.PPI['numberOfThreshold-histogram'] - 1],
		slide: function( event, ui ) {
			$("#threshold-picker_left").text(getThresholdByIndex(ui.values[0] * Window.PPI['thresholdRatio'])) ;
			$("#threshold-picker_right").text(getThresholdByIndex(ui.values[1] * Window.PPI['thresholdRatio'])) ;
		},
		stop: function(event, ui) {
			if (ui.values[0] !== Window.PPI['threshold-picker-left']) {
				Window.PPI['threshold-picker-left'] = ui.values[0]; 
				$("#hist-threshold").val(ui.values[0]).change() ;
			}

			if (ui.values[1] !== Window.PPI['threshold-picker-right']) {
				Window.PPI['threshold-picker-right'] = ui.values[1]; 
				$("#threshold-window").val(ui.values[1]).change() ;
			}
		},
		create: function() {
			$("#threshold-picker_left").text(getThresholdByIndex(0 * Window.PPI['thresholdRatio'])) ;
			$("#threshold-picker_right").text(getThresholdByIndex((Window.PPI['numberOfThreshold-histogram'] - 1) * Window.PPI['thresholdRatio']) ) ;
			$("#histogram-threshold-picker-default").html("") ;
			Window.PPI['threshold-picker-left'] = 0 ;
			Window.PPI['threshold-picker-right'] = Window.PPI['numberOfThreshold-histogram'] - 1 ;
		}
	});

	$("#legend-slider-top").unbind().slider({
		min: 1,
		orientation: "horizontal",
		max: Window.PPI['histogram-width'] * Window.PPI['histogram-height'],
		value: 0,
		slide: function(event, ui) {
			$("#legend-custom-handle-top").html("<span style='font-size:10px;'>"+ui.value+"</span>").attr("value", ui.value) ;
			$("#hidden-legend-redraw-top").click() ;
		}
	}) ;

	// reverse the min and max
	$("#legend-slider-right").unbind().slider({
		min: 1,
		orientation: "vertical",
		max: Window.PPI['histogram-width'] * Window.PPI['histogram-height'],
		value: 0,
		slide: function(event, ui) {
			var v = Window.PPI['histogram-width'] * Window.PPI['histogram-height'] + 1 - ui.value ;
			$("#legend-custom-handle-right").html("<span style='font-size:10px;'>"+v+"</span>").attr("value", v) ;
			$("#hidden-legend-redraw-right").click() ;
		},
	}) ;

    resetHist() ;

    var mCount = 1 ;
    if (Window.PPI['histogram-width'] <= mCount) {
    	Window.PPI['histogram-mode'] = "multi" ;  // show the single-view at first
    } else {
    	Window.PPI['histogram-mode'] = "single" ; // show the multi-view at first
    }
    resetBoxplot() ;
    drawBoxPlot() ;
	triggerCtrlV() ;
	$('body').loading('stop');
}

function Connect() {
	$('body').loading({theme: 'light'});
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
			btn.html('Connect');
			$("#load_test").attr("disabled", true);
		},
		function () {
			console.log("on_close");
		},
		objectCallback,
		ip = $("#msg-host").val(), false);

	ttk_render = new ttkWebSocketIO(PORT_RENDER, function () {
		console.log("on_open for render");
	}, () => {}, () => {}, () => {}, obj => RENDERER.setScene(obj), $("#msg-host").val(), true);
}

function LoadTest() {
	Window.PPI['DEV'] = true;
	var testdataset = loadTestDataFromString();
	objectCallback(testdataset);
	$("#load_test").attr("disabled", "disabled");
	$("#connect").attr("disabled", true);
}

function triggerCtrlV() {
	$("#histogram-notification-placeholder-0").html("-") ;
	if (Window.PPI['histogram-mode'] == "multi") {
		Window.PPI['histogram-mode'] = "single" ;
		$("[element-show='mdm']").css("display", "none") ;
		$("[element-show='sdm']").css("display", "") ;
		drawSDMHistogram(Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)) ;
		if (Window.PPI['selected-bin-id-sdm']) {
			d3.select("#" + Window.PPI['selected-bin-id-sdm']).dispatch("click") ;
		}
	} else {
		Window.PPI['histogram-mode'] = "multi" ;
		$("[element-show='mdm']").css("display", "") ;
		$("[element-show='sdm']").css("display", "none") ;
		drawHistogram(parseInt($("#hist-threshold").val()), Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)) ;
		if (Window.PPI['selected-bin-id']) {
			d3.select("#" + Window.PPI['selected-bin-id']).dispatch("click") ;
		}
	}
}

$(document).keydown(function (e) {
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

	// shift + event 
	if ( e.shiftKey) {
		switch(e.key) {
			case "ArrowRight":
				// single view
				if (Window.PPI['histogram-mode'] == "multi") {
					
				} else {
					$("#hidden-sdm-move-next").click() ;
				}
				break;
			case "ArrowLeft":
				// single view
				if (Window.PPI['histogram-mode'] == "multi") {
					
				} else {
					$("#hidden-sdm-move-prev").click() ;
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

	// for sdm
	var selected_sdm = false;
	var g_idx_sdm = 0;
	if ($("[bin-selected-sdm=on]").length > 0) {
		g_idx_sdm = parseInt($("[bin-selected-sdm=on]").attr("id").replace("sdm-hist-bin-", ""));
		var j_sdm = Math.floor(g_idx_sdm / Window.PPI['sdm-histogram-width']);
		var i_sdm = g_idx_sdm % Window.PPI['sdm-histogram-width'];
		selected_sdm = true;
	}

	switch (e.key) {
		case "v":  // toggle between MDM and SDM view
			triggerCtrlV() ;
			break ;

		case "x":  // toggle hover in the MDM view
			Window.PPI["X-mode"] = 1 - Window.PPI["X-mode"] ;
			break ;

		case "m": // toogle the time picker mode
			if ( Window.PPI['boxplot-timestamp-range-mode']['start'] === Window.PPI['boxplot-timestamp-range-mode']['end'] ) {
				if ( Window.PPI['boxplot-timestamp-range-mode']["time-picker-mode"] === "multi" ) {
					Window.PPI['boxplot-timestamp-range-mode']["time-picker-mode"] = "single" ;
					$("#slider-range").css("display", "none") ;
					$("#slider-range-single").css("display", "") ;
					$("#slider-range-single").slider("value", parseInt(Window.PPI['boxplot-timestamp-range-mode']['start']))
				} else {
					Window.PPI['boxplot-timestamp-range-mode']["time-picker-mode"] = "multi" ;
					$("#slider-range").css("display", "") ;
					$("#slider-range-single").css("display", "none") ;
					$( "#slider-range" ).slider("option", "values", [ parseInt(Window.PPI['boxplot-timestamp-range-mode']['start']),  parseInt(Window.PPI['boxplot-timestamp-range-mode']['end'])])
				}
			}
			break ;

		case "b": // toggle selection in the boxplot
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
				if (selected_sdm) {
					if (i_sdm + 1 <= Window.PPI['sdm-histogram-width'] - 1) {
						d3.select("#sdm-hist-bin-" + (g_idx_sdm + 1)).dispatch("click");
					}
				}
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
				if (selected_sdm) {
					if (i_sdm - 1 >= 0) {
						d3.select("#sdm-hist-bin-" + (g_idx_sdm - 1)).dispatch("click");
					}
				}
			}
			break;

		case "ArrowUp":
			if (Window.PPI['histogram-mode'] == "multi") {
				if (selected) {
					if (j + 1 <= Window.PPI['histogram-height'] - 1) {
						d3.select("#hist-bin-" + (g_idx + Window.PPI['histogram-width'])).dispatch("click");
					}
				}
			} else {
				if (selected_sdm) {
					if (j_sdm + 1 <= Window.PPI['histogram-height'] - 1) {
						d3.select("#sdm-hist-bin-" + (g_idx_sdm + Window.PPI['sdm-histogram-width'])).dispatch("click");
					}
				}
			}
			
			break;

		case "ArrowDown":
			if (Window.PPI['histogram-mode'] == "multi") {
				if (selected) {
					if (j - 1 >= 0) {
						d3.select("#hist-bin-" + (g_idx - Window.PPI['histogram-width'])).dispatch("click");
					}
				}
			} else {
				if (selected_sdm) {
					if (j_sdm - 1 >= 0) {
						d3.select("#sdm-hist-bin-" + (g_idx_sdm - Window.PPI['sdm-histogram-width'])).dispatch("click");
					}
				}
			}
			
			break;
	}
});

$("#hist-threshold").change(function () {
    v0 = parseInt($($("#hist-threshold option:selected")[0]).val()) ;
    v1 = parseInt($($("#threshold-window option:selected")[0]).val()) ;

    if ( v0 > v1 ) {
    	var tmp = v0 + v1 - parseInt($(this).attr("data-value")) ;
    	tmp = Math.min(Window.PPI['numberOfThreshold-histogram'] - 1, tmp) ;
    	$("#threshold-window").attr("data-value", tmp) ;
	    $("#threshold-window").val(tmp) ;
    }

    $(this).attr("data-value", v0)
    
    $("#hist-threshold option:selected").each(function () {
    	if (Window.PPI['histogram-mode'] == "multi") {
	        drawHistogram(parseInt($(this).val()), Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    	} else {
	        drawSDMHistogram(Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    	}
	});
	if ( Window.PPI['histogram-mode'] == "multi" ) {
		if (Window.PPI['selected-bin-id']) {
			d3.select("#" + Window.PPI['selected-bin-id']).dispatch("click") ;
		}
	} else {
		if (Window.PPI['selected-bin-id-sdm']) {
			d3.select("#" + Window.PPI['selected-bin-id-sdm']).dispatch("click") ;
		}
	}
    
    $("#hidden-optimal-threshold").click();
	$(this).focus() ;
	
	$("#histogram_viz").loading("stop");
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

    $("#hist-threshold option:selected").each(function () {
        if (Window.PPI['histogram-mode'] == "multi") {
	        drawHistogram(parseInt($(this).val()), Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    	} else {
	        drawSDMHistogram(Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    	}
    });
    if ( Window.PPI['histogram-mode'] == "multi" ) {
		if (Window.PPI['selected-bin-id']) {
			d3.select("#" + Window.PPI['selected-bin-id']).dispatch("click") ;
		}
	} else {
		if (Window.PPI['selected-bin-id-sdm']) {
			d3.select("#" + Window.PPI['selected-bin-id-sdm']).dispatch("click") ;
		}
	}
    $("#hidden-optimal-threshold").click();
	$(this).focus() ;
}) ;

$("#hist-rescale").unbind().click(function() {
    tmp = getRangeOfPPIByThreshold() ;
    Window.PPI['persistence_pairs_range']['is_custom'] = true ;
    // setup domain of # of bins
    Window.PPI['persistence_pairs_range']['custom_upper'] = tmp[1] ;
    Window.PPI['persistence_pairs_range']['custom_lower'] = tmp[0] ;

    if (Window.PPI['histogram-mode'] == "multi") {
	    drawHistogram(parseInt($("#hist-threshold").val()), Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null), tag=1); 
    } else {
	    drawSDMHistogram(Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
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
	    drawHistogram(parseInt($("#hist-threshold").val()), Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null), tag=1); 
    } else {
	    drawSDMHistogram(Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    }
    // re-draw the histogram
    $("#histRescaleCustom-close").click() ;
}) ;

