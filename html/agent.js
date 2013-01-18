var agent = {
	/*
	 * Keep 3 stats objects.
	 */
	stats:new Array(),
	/*
	 * Update them X times a second, useful to increase this during
	 * debugging to avoid flooding yourself. Can't be reliably set at
	 * run time since the timers will remain unchanged.
	 */
	statsInterval: 1,
	params:null,
	/*
	 * Used to have a central way to transmit "output".
	 * Use out_clear() when starting out, add stuff to agent.out, then
	 * on completion use out_up() to show the message(s).
	 */
	out:"Initializing frontend",
	vcl:"",
	vclId:"",
	vclList:null,
	activeVcl:"",
	debug: true,
	/*
	 * Global AJAX timeout (in milliseconds)
	 */
	globaltimeout: 5000,
	version: ""
};

function clog(text)
{
	if(agent.debug) {
		console.log(text);
	}
}

function assert(val)
{
	if (!(val)) {
		throw "Assertion failure";
	}
}

function assertText(val)
{
	assert(val != null);
	assert(val.length > 0);
}

function out_clear()
{
	document.getElementById("out").innerHTML = "";
	agent.out = "";
}

function out_up()
{
	document.getElementById("out").innerHTML = agent.out;
}

function topActive(head)
{
	var navs = new Array('nav-home', 'nav-vcl', 'nav-param');
	assertText(head);

	for (x in navs) {
		document.getElementById(navs[x]).className = "";
	}
	document.getElementById("nav-" + head).className = "active";
}

function showVCL()
{
	document.getElementById("params").style.display = "NONE";
	document.getElementById("home").style.display = "NONE";
	document.getElementById("vcl").style.display = "block";
	topActive("vcl");
}

function showHome()
{
	document.getElementById("params").style.display = "NONE";
	document.getElementById("home").style.display = "block";
	document.getElementById("vcl").style.display = "NONE";
	topActive("home");
}

function showParam()
{
	document.getElementById("vcl").style.display = "NONE";
	document.getElementById("home").style.display = "NONE";
	document.getElementById("params").style.display = "block";
	list_params();
	topActive("param");
}

function reset_status()
{
	var but = document.getElementById("status-btn");
	assert(but != null);
	var stat;
	$.ajax({
		type: "GET",
		url: "/status",
		dataType: "text",
		timeout: agent.globaltimeout,
		success: function (data, textStatus, jqXHR) {
			stat = data;
			assertText(stat);
			but.textContent = stat;
		},
		error: function( jqXHR, textStatus, errorThrown) {
			stat = "Error communicating with agent. " + errorThrown;
			clog("error: "+stat);
			clog(textStatus);
			clog(jqXHR);
		},
		complete: function( jqXHR, textStatus) {
   			but.textContent = stat;
			if (stat == "Child in state running") {
				but.className = "btn btn-primary btn-block disabled";
			} else {
				but.className = "btn btn-danger btn-block disabled";
			}
		}
	});
}	

function show_status(state,message)
{
	assertText(state);
	assertText(message);
	but = document.getElementById("status-btn");
	if (state == "ok")
		but.className = "btn btn-success btn-block disabled";
	if (state == "warn")
		but.className = "btn btn-warning btn-block disabled";
	if (state == "error")
		but.className = "btn btn-danger btn-block disabled";
	but.textContent = message;
	setTimeout(function() { reset_status(); }, 3000);
}

function uploadVCL()
{
	var id = document.getElementById("vclID").value;
	var vcl = document.getElementById("vcl-text").value;
	assertText(vcl);
	assertText(id);
   	out_clear();
	$.ajax({
		type: "PUT",
		url: "/vcl/" + id,
		timeout: agent.globaltimeout,
		contentType: "application/xml",
		data: vcl,
		success: function (data, textStatus, jqXHR) {
			agent.out = jqXHR.responseText;
		},
		error: function( jqXHR, textStatus, errorThrown) {
				agent.out = "Com. errors with agent: \n" + jqXHR.responseText;
		},
		complete: function( jqXHR, textStatus) {
			out_up();
			if( jqXHR.status == 201) {
				show_status("ok","VCL stored");
			} else {
				show_status("warn", "VCL save failed");
			}
		}
	});
}

function loadVCL()
{
	out_clear();
	var id = document.getElementById("loader").value;
	assertText(id);
	$.ajax({
		type: "GET",
		url: "/vcl/" + id,
		timeout: agent.globaltimeout,
		success: function (data, textStatus, jqXHR) { },
		error: function( jqXHR, textStatus, errorThrown) {
			agent.out = "Com. errors with agent: \n" + errorThrown;
			clog("error");
			clog(errorThrown);
			clog(textStatus);
			clog(jqXHR);
		},
		complete: function( jqXHR, textStatus) {
			if( jqXHR.status == 200) {
				agent.vcl = jqXHR.responseText;
				agent.vclId = id;
				show_status("ok", "VCL loaded");
				document.getElementById("vcl-text").value = agent.vcl;
				document.getElementById("vclID").value = agent.vclId;
			} else {
				agent.out = jqXHR.responseText;
				show_status("warn","VCL load failed");
			}
			out_up();
		}
	});
}


function listVCL()
{
	$.ajax({
		type: "GET",
		url: "/vcljson/",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			var vclList = JSON.parse(data);
			agent.vclList = vclList;
			var txt = "";
			for (x in agent.vclList["vcls"]) {
				v = agent.vclList["vcls"][x];
				if (v.status == "discarded") {
					continue;
				}
				if (v.status == "active") {
					agent.activeVcl = v.name;
					document.getElementById("vcl-btn").innerHTML = "Active VCL: " + v.name;
				}
				txt = txt + "\n" + "<option>" + v.name + "</option>";
			}
			var d = document.getElementById("loader");
			d.innerHTML = txt;
			if (document.getElementById("vcl-text").value == "") {
				d.value = agent.activeVcl;
				document.getElementById("vclID").value = agent.activeVcl;
				loadVCL();
			}
		},
		error: function( jqXHR, textStatus, errorThrown) {
			agent.out = "Listing VCL failed.";
			clog(jqXHR);
			clog(textStatus);
			clog(errorThrown);
		}
	});
}

function list_params()
{
	$.ajax({
		type: "GET",
		url: "/paramjson/",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			if( jqXHR.status == 200) {
				var paramlist = JSON.parse(jqXHR.responseText);
				agent.params = paramlist;
				var list = document.getElementById("param-sel");
				var v = list.value;
				var arry = new Array();
				for (x in agent.params) {
					arry.push(x);
				}
				arry.sort();
				list.options.length = 0;
				for (x in arry) {
					list.add(new Option(arry[x]));
				}
				if (v)
					list.value = v;
				paramChange();
			}
		},
		error: function( jqXHR, textStatus, errorThrown) {
			clog("error");
			clog(jqXHR);
			clog(textStatus);
			clog(errorThrown);
			out_clear();
			agent.out = "Error listing parameters.\n";
			agent.out += "Communication error with agent: \n";
			agent.out += errorThrown;
		},
		complete: function( jqXHR, textStatus) {
			if (jqXHR.status != 200) {
				agent.out += "Varnish-Agent returned " +
					jqXHR.status + ":" + jqXHR.responseText;
			}
			out_up();
		}
	});
	
}

function paramChange()
{
	var pname = document.getElementById("param-sel").value;
	assertText(pname);
	document.getElementById("param-val").value = agent.params[pname].value;
	out_clear();
	agent.out =  "Default: " + agent.params[pname].default + "\n";
	agent.out += "Value:   " + agent.params[pname].value + "\n";
	agent.out += "Unit:    " + agent.params[pname].unit + "\n";
	agent.out += agent.params[pname].description;
	out_up();
}

function paramListDiff()
{
	out_clear();
	agent.out = "Non-default parameters:\n\n";
	assert(agent.params != null);
	for (x in agent.params) {
		if (agent.params[x].unit == "seconds" || agent.params[x].unit == "s" || agent.params[x].unit == "bitmap") {
			var one = Number(agent.params[x].value);
			var two = Number(agent.params[x].default);
			if (one != two) {
				agent.out +=  x + "(";
				agent.out += one + " vs ";
				agent.out += two + ")\n";
			}
		} else if (agent.params[x].unit == "bytes") {
			var one = agent.params[x].value == "-1" ? "unlimited" : agent.params[x].value;
			var two = agent.params[x].default == "-1" ? "unlimited" : agent.params[x].default;
			if (one != two) {
				agent.out += x + "(";
				agent.out +=  one + " vs ";
				agent.out +=  two + ")\n";
			}

		} else if (agent.params[x].value != agent.params[x].default) {
			agent.out += x + "(";
			agent.out += agent.params[x].value + " vs ";
			agent.out += agent.params[x].default + ")\n";
		}
	}
	out_up();
}

function saveParam()
{
	var pname = document.getElementById("param-sel").value;
	var pval = document.getElementById("param-val").value;
	assertText(pname);
	assertText(pval);
	out_clear();

	$.ajax({
		type: "PUT",
		url: "/param/"+pname,
		timeout: agent.globaltimeout,
		contentType: "application/xml",
		data: pval,
		complete: function( jqXHR, textStatus) {
			agent.out = jqXHR.responseText;
			out_up();
			if (jqXHR.status == 200) {
				show_status("ok","Parameter saved");
				list_params();
			} else {
				show_status("warn","Couldn't save parameter");
			}
		}
	});
}

function setParamDef()
{
	var pname = document.getElementById("param-sel").value;
	var pval = document.getElementById("param-val");
	assertText(pname);
	assertText(agent.params[pname].default);
	pval.value = agent.params[pname].default;
}

function deployVCL()
{
	out_clear();
	var id = document.getElementById("loader").value;
	assertText(id);
	$.ajax({
		type: "PUT",
		url: "/vcldeploy/" + id,
		timeout: agent.globaltimeout,
		success: function (data, textStatus, jqXHR) {
			agent.out = jqXHR.responseText;
		},
		error: function( jqXHR, textStatus, errorThrown) {
			agent.out = "Com. errors with agent: \n" + jqXHR.responseText;
			clog("error");
			clog(jqXHR);
			clog(textStatus);
			clog(errorThrown);	
		},
		complete: function( jqXHR, textStatus) {
			if (jqXHR.status == "200") {
				agent.activeVcl = id;
				show_status("ok","VCL deployed");
			} else {
				show_status("warn","vcl deploy failed");
			}
			out_up();	
		}
	});
}

function clearID()
{
	document.getElementById("vclID").value = "";
	agent.vclId = "";
}

function discardVCL()
{
	out_clear();
	var id = document.getElementById("loader").value;
	assertText(id);

	$.ajax({
		type: "DELETE",
		url: "/vcl/" + id,
		timeout: agent.globaltimeout,
		success: function (data, textStatus, jqXHR) {
			agent.out = jqXHR.responseText;
		},
		error: function( jqXHR, textStatus, errorThrown) {
			agent.out = "Com. errors with agent: \n" + jqXHR.responseText;
			clog("error");
			clog(jqXHR);
			clog(textStatus);
			clog(errorThrown);
		},
		complete: function( jqXHR, textStatus) {
			if (jqXHR.status = "200" && jqXHR.responseText == "") {
				show_status("ok","VCL discarded");
			} else {
				show_status("warn","VCL discard failed. "+jqXHR.responseText);
			}
			out_up();
		}
	});

}

function stop()
{
	$.ajax({
		type: "PUT",
		url: "/stop/",
		timeout: agent.globaltimeout,
		complete: function( jqXHR, textStatus) {
			var doc = jqXHR.responseText;
			document.getElementById("out").innerHTML = doc;
			status();
		}
	});
}

function start()
{
	$.ajax({
		type: "PUT",
		url: "/start",
		timeout: agent.globaltimeout,
		complete: function( jqXHR, textStatus) {
			var doc = jqXHR.responseText;
			document.getElementById("out").innerHTML = doc;
			status();
		}
	});

}

function status()
{
	listVCL();
	reset_status();
}

function update_stats()
{
	var d = document.getElementById("stats-btn");
	assert(d != null);
	$.ajax({
		type: "GET",
		url: "/stats",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			for (i = 0; i < 3; i++) {
				agent.stats[i] = agent.stats[i+1];
				}
			agent.stats[3] = JSON.parse(data);
			var n_req = 0;
			var n_n_req = 0;
			for (i = 3; agent.stats[i-1] != null; i--) {
				if (agent.stats[i] == null)
					break;
				if (agent.stats[i].client_req == null)
					break;
				if (agent.stats[i-1] != null && agent.stats[i-1].client_req != null)
					n_req += agent.stats[i].client_req.value - agent.stats[i-1].client_req.value;
				else
					n_req += agent.stats[i].client_req.value;
				n_n_req++;
			}
			if (n_n_req>0)
				d.innerHTML = Number((n_req/n_n_req/agent.statsInterval)).toFixed(0) + "req/s\n";
		},
		error: function( jqXHR, textStatus, errorThrown) {
			d.innerHTML = "Couldn't get stats: " + err;
			clog(jqXHR);
			clog(textStatus);
			clog(errorThrown);
		}
	});
}

function updateTop()
{
	var tag = document.getElementById("varnishtop-sel").value;
	assertText(tag);
	$.ajax({
		type: "GET",
		url: "/log/1/" + tag,
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			agent.rxtop = JSON.parse(data);
			var tmp = "";
			var list = new Object();
			for (var i = 0; i < agent.rxtop.log.length; i++) {
				if (list[agent.rxtop.log[i].value] == null)
					list[agent.rxtop.log[i].value] = 1;
				else
					list[agent.rxtop.log[i].value]++;
			}
			var arr = new Array();
			for (i in list) {
				arr.push({"url":i,"num":list[i]});
			}
			arr.sort(function(a,b) {return b.num - a.num;});
			for (var i=0; i<arr.length && i < 5; i++) {
				tmp += arr[i].url + "  (" + arr[i].num + " times)\n";
			}
			var d = document.getElementById("varnishtop");
			d.innerHTML = tmp;
		},
		error: function( jqXHR, textStatus, errorThrown) {
			d.innerHTML = "Couldn't get stats: " + err;
			clog(jqXHR);
			clog(textStatus);
			clog(errorThrown);
		}
	});
}

function banSmart()
{
	var banUrl = document.getElementById("smartBan").value;
	assertText(banUrl);
	$.ajax({
		type: "POST",
		url: "/ban" + banUrl,
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			agent.out = "OK!"+  data;
			out_up();

		},
        error: function( jqXHR, textStatus, errorThrown) {
			agent.out = "Couldn't ban: " + errorThrown;
			out_up();
        }
	});
}

function banList()
{
	$.ajax({
		type: "GET",
		url: "/ban",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			agent.out = data;
			out_up();
		},
		error: function (jqXHR, textStatus, errorThrown) {
			agent.out = "Failed to list!\n" + errorThrown;
			out_up();
		}
	});
}

function panicShow()
{
	$.ajax({
		type: "GET",
		url: "/panic",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			agent.out = data;
			out_up();
		},
		error: function (jqXHR, textStatus, errorThrown) {
			agent.out = "Failed to run panic.show\n" + errorThrown + "\n" + textStatus + "\n" + jqXHR;
			agent.out += jqXHR.responseText;
			if (jqXHR.responseText == "Child has not panicked or panic has been cleared")
				agent.out = jqXHR.responseText;

			clog(jqXHR);
			clog(errorThrown);
			out_up();
		}
	});
}

function panicClear()
{
	$.ajax({
		type: "DELETE",
		url: "/panic",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			agent.out = data;
			out_up();
		},
		error: function (jqXHR, textStatus, errorThrown) {
			agent.out = "Failed to run panic.clear\n" + errorThrown;
			if (jqXHR.responseText == "No panic to clear")
				agent.out = jqXHR.responseText;
			out_up();
		}
	});
}

function getVersion()
{
	$.ajax({
		type: "GET",
		url: "/version",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			agent.version = data;
			document.getElementById("agentVersion").innerHTML = agent.version;
		},
		error: function (jqXHR, textStatus, errorThrown) {
			agent.out = "Failed to fetch agent version\n" + errorThrown + "\n" + textStatus + "\n" + jqXHR;
			agent.out += jqXHR.responseText;
			clog(jqXHR);
			clog(errorThrown);
			out_up();
		}
	});
}
$('.btn').button();
setInterval(function(){status()},10000);
setInterval(function(){update_stats()},agent.statsInterval * 1000);
setInterval(function(){updateTop()},5000);
updateTop();
listVCL();
list_params();
getVersion();
