#!/bin/bash
sleep 0.9
echo -en "HTTP/1.1 200 OK\r\n"
echo -en "Cache-Control: s-maxage=20\r\n"
echo -en "Connection: close\r\n"
echo -en "\r\n"
echo "<h1>hei</h1>"
echo "<p>hallo</p>"
