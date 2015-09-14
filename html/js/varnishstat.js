/*
 * Copyright (C) 2015  Norwegian Broadcasting Corporation (NRK)
 *
 * Author: Kristian Lyngstøl <kristian@bohemians.org>
 *
 * FIXME: This thing is entirely separate from the rest of agent.js, for better
 * or worse. This is arguably a feature, as it's quite reasonable to want to
 * use this stand-alone.
 *
 */


var vdata = new Array();
var vdatap = new Array();
var ignore_null = true;
var hitrate = { 
	10: { v: 0, d: 0 },
	100: { v: 0, d: 0 },
	1000: { v: 0, d: 0 }
}

/*
 * Store the last 1000 seconds of varnishstat data, but also the very first.
 */
function compressData()
{
	if (vdata.length > 1000)
		vdata.splice(10,vdata.length-1001);
}

/*
 * Updates stats
 */
function updateArray()
{
	$.ajax({
		type: "GET",
		url: "/stats",
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			var tmp = JSON.parse(data);
			var tmpp = {};
			if (vdatap.length != 0) {
				if (vdatap[0]['timestamp'] == tmp['timestamp']) {
					return;
				}
			}
			for (v in tmp) {
				if (tmp[v]['value'] != undefined)  {
					tmp[v]['value'] = parseInt(tmp[v]['value']);
					tmpp[v.replace("MAIN.","")] = parseInt(tmp[v]['value']);
				}
			}
			vdata.unshift(tmp);
			vdatap.unshift(tmpp);
			compressData();
			updateHitrate();
			updateTable();
		}
	})	
}

/*
 * Construct the varnishstat table if needed.
 *
 * Also empties it first.
 *
 * Used both on init, if variables get added (upgraded agent/varnish?), or if
 * the toggle-button for null-values is pressed.
 */
function constructTable()
{
	var el = document.getElementById("varnishstat");
	var table = document.getElementById("varnishstat-inner");
	el.removeChild(table);
	table = document.createElement("table");
	table.className = "table table-condensed";
	table.id = "varnishstat-inner";
	header = table.createTHead();
	tr = header.insertRow(0);
	td = tr.insertCell(0);
	td.innerHTML = "Name";
	td = tr.insertCell(1);
	td.innerHTML = "Current";
	td = tr.insertCell(2);
	td.innerHTML = "Change";
	td = tr.insertCell(3);
	td.innerHTML = "Average";
	td = tr.insertCell(4);
	td.innerHTML = "Average 10";
	td = tr.insertCell(5);
	td.innerHTML = "Average 100";
	td = tr.insertCell(6);
	td.innerHTML = "Average 1000";
	td = tr.insertCell(7);
	td.innerHTML = "Description";
	
	for (v in vdata[0]) {
		if (v == 'timestamp')
			continue;
		if (vdata[0][v]['value'] == 0 && ignore_null)
			continue;
		tr = table.insertRow(-1);
		td = tr.insertCell(0);
		td.id = v + "-name";
		td.innerHTML = v;
		td = tr.insertCell(1);
		td.id = v + "-cur";
		td.innerHTML = vdata[0][v]['value'];
		td = tr.insertCell(2);
		td.id = v + "-diff";
		td = tr.insertCell(3);
		td.id = v + "-avg";
		td = tr.insertCell(4);
		td.id = v + "-avg10";
		td = tr.insertCell(5);
		td.id = v + "-avg100";
		td = tr.insertCell(6);
		td.id = v + "-avg1000";
		td = tr.insertCell(7);
		td.id = v + "-sdec";
		td.innerHTML = vdata[0][v]['description'];
	}
	el.appendChild(table);
}

/*
 * You don't want to read this. Pretend it's not here.
 * (converts 124124123 to "day+hour:minute:second")
 */
function secToDiff(x)
{
	if (x < 60) {
		return x;
	}
	s = x % 60;
	x -= s;
	m = x % 3600;
	x -= m;
	h = x % (3600*24);
	x -= h;
	d = x / (3600*24);

	d = d/(3600*24);
	h = h/3600;
	m = m/60;
	if (s<10) 
		s = "0" + s;
	if (m < 10)
		m = "0" + m;
	if (h < 10)
		h = "0" + h;

	return  d + "+" +  h + ":" + m + ":" + s ;
}

function toggleIgnoreNull()
{
	ignore_null = !ignore_null;
	constructTable();
}

/*
 * Primary update-function, after the data has changed.
 *
 * Could/should probably be a bit prettier.
 */
function updateTable()
{
	for (v in vdata[0]) {
		if (v == 'timestamp')
			continue;
		if (vdata[0][v]['value'] == 0 && ignore_null)
			continue;
		el = document.getElementById(v + "-cur");
		if (el == undefined) {
			constructTable();
			return;
		}
		el.innerHTML = vdata[0][v]['value'];
		if (vdata.length > 1) {
			el = document.getElementById(v + "-diff");
			el.innerHTML = vdata[0][v]['value'] - vdata[1][v]['value'];
			if (vdata[0][v]['flag'] == 'a') {
				el = document.getElementById(v + "-avg");
				el.innerHTML = (vdata[0][v]['value'] / vdata[0]['MAIN.uptime']['value']).toFixed(5);
				scope = [10, 100, 1000];
				for (a in scope) {
					a = scope[a];
					el = document.getElementById(v + "-avg" + a);
					now =  vdata[0][v]['value'];
					nthen = vdata.length > a ? a : vdata.length - 1;
					then = vdata[nthen][v]['value'];
					timed = vdata[0]['MAIN.uptime']['value'] - vdata[nthen]['MAIN.uptime']['value'];
					el.innerHTML = ((now - then) / timed).toFixed(2);
				}
			}
		}
		
	}
}

/*
 * Updates the hitrate and uptime-boxes
 */
function updateHitrate()
{
	var tmp = [10, 100, 1000];	
	for (x in tmp ) {
		a = tmp[x];
		var origa = a;
		if (vdatap.length-1 < a) {
			a = vdatap.length-1;
		}
		hitrate[origa].d = a;
		hitrate[origa].v = ((vdatap[0]['cache_hit'] - vdatap[a]['cache_hit']) / (vdatap[0]['client_req'] - vdatap[a]['client_req'])).toFixed(5);
		document.getElementById("hitrate" + origa + "d").innerHTML = hitrate[origa].d;
		document.getElementById("hitrate" + origa + "v").innerHTML = (hitrate[origa].v*100).toFixed(3) + "%";
	}
	document.getElementById("MAIN.uptime-1").innerHTML = secToDiff(vdatap[0]['uptime']);
	document.getElementById("MGT.uptime-1").innerHTML = secToDiff(vdatap[0]['MGT.uptime']);
}

/*
 * FIXME: Should not really be here. Hidden away from the rest of the agent.js code...
 */
setInterval(updateArray,1000);

