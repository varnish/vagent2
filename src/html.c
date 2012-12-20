#include "main.h"
#include "plugins.h"
#include "ipc.h"
#include "httpd.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

struct html_priv_t {
	int logger;
};

unsigned int html_reply(struct httpd_request *request, void *data)
{
	struct agent_core_t *core = data;
	struct httpd_response response;
	struct html_priv_t *html;
	struct agent_plugin_t *plug;
	char *htmltxt = "<html><body>"


"VCL: <br><textarea id=\"vcl\" cols=80 rows=25></textarea>"
"<br>"
"VCL ID: <br><input type=text id=\"vclID\"></input>"
"<button onclick=\"clearID()\">Clear</button><br>"
"<button onclick=\"uploadVCL()\">Save VCL</button>"
"<button onclick=\"loadVCL()\">Load VCL</button>"
"<button onclick=\"deployVCL()\">Deploy VCL</button>"
"<button onclick=\"listVCL()\">List VCL</button>"
"<button onclick=\"discardVCL()\">Discard VCL</button>"
"<button onclick=\"stop()\">Stop Varnish</button>"
"<button onclick=\"start()\">Start Varnish</button>"
"<button onclick=\"status()\">Get Status</button>"
"<br>output:<br> <pre id=\"out\"></pre>"
"<script type=\"text/javascript\">"
"function uploadVCL() {"
"document.getElementById(\"out\").innerHTML = \"\";"
"var client = new XMLHttpRequest();"
"client.open(\"PUT\", \"/vcl/\" + document.getElementById(\"vclID\").value, false);"
"client.send(document.getElementById(\"vcl\").value);"
"doc = client.responseText;"
"document.getElementById(\"out\").innerHTML = doc;"
"}"
"function loadVCL() {"
"var client = new XMLHttpRequest();"
"document.getElementById(\"out\").innerHTML = \"\";"
"client.open(\"GET\", \"/vcl/\" + document.getElementById(\"vclID\").value, false);"
"client.send(document.getElementById(\"vcl\").value);"
"doc = client.responseText;"
"document.getElementById(\"vcl\").innerHTML = doc;"
"}"
"function listVCL() {"
"var client = new XMLHttpRequest();"
"document.getElementById(\"out\").innerHTML = \"\";"
"client.open(\"GET\", \"/vcl/\", false);"
"client.send(document.getElementById(\"vcl\").value);"
"doc = client.responseText;"
"document.getElementById(\"out\").innerHTML = doc;"
"}"
"function deployVCL() {"
"document.getElementById(\"out\").innerHTML = \"\";"
"var client = new XMLHttpRequest();"
"client.open(\"PUT\", \"/vcldeploy/\" + document.getElementById(\"vclID\").value, false);"
"client.send(document.getElementById(\"vcl\").value);"
"doc = client.responseText;"
"document.getElementById(\"out\").innerHTML = doc;"
"}"
"function clearID() {"
"document.getElementById(\"vclID\").value = \"\";"
"}"
"function discardVCL() {"
"document.getElementById(\"out\").innerHTML = \"\";"
"var client = new XMLHttpRequest();"
"client.open(\"PUT\", \"/vcldiscard/\" + document.getElementById(\"vclID\").value, false);"
"client.send(document.getElementById(\"vcl\").value);"
"doc = client.responseText;"
"document.getElementById(\"out\").innerHTML = doc;"
"}"
"function stop() {"
"document.getElementById(\"out\").innerHTML = \"\";"
"var client = new XMLHttpRequest();"
"client.open(\"PUT\", \"/stop\", false);"
"client.send();"
"doc = client.responseText;"
"document.getElementById(\"out\").innerHTML = doc;"
"}"
"function start() {"
"document.getElementById(\"out\").innerHTML = \"\";"
"var client = new XMLHttpRequest();"
"client.open(\"PUT\", \"/start\", false);"
"client.send();"
"doc = client.responseText;"
"document.getElementById(\"out\").innerHTML = doc;"
"}"
"function status() {"
"document.getElementById(\"out\").innerHTML = \"\";"
"var client = new XMLHttpRequest();"
"client.open(\"GET\", \"/status\", false);"
"client.send();"
"doc = client.responseText;"
"document.getElementById(\"out\").innerHTML = doc;"
"}"
"</script></body></html>";
	plug = plugin_find(core,"html");
	html = plug->data;

	response.status = 200;
	response.body = htmltxt;
	response.nbody = strlen(htmltxt);
	send_response(request->connection, &response);
	return 0;
}

void
html_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct html_priv_t *priv = malloc(sizeof(struct html_priv_t));
	plug = plugin_find(core,"html");
	
	priv->logger = ipc_register(core,"logd");
	plug->data = (void *)priv;
	plug->start = NULL;
        httpd_register_url(core, "/html/", M_GET, html_reply, core);
}


