#!/bin/bash
arg=$1
length=$2
export GATEWAY_INTERFACE="CGI/1.1"
export SERVER_PROTOCOL="HTTP/1.1"
export SCRIPT_FILENAME="/home/abyss4me/Myhttpd/submit.php"
export SCRIPT_NAME="submit.php"
export REQUEST_METHOD="POST"
export REDIRECT_STATUS=200
export REMOTE_HOST="127.0.0.1"
export CONTENT_LENGTH=$length
export HTTP_ACCEPT="text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
export CONTENT_TYPE="application/x-www-form-urlencoded"
PATH=$PATH:/usr/bin
export PATH
echo $arg | /usr/bin/php-cgi
