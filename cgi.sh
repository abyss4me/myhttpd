#!/bin/bash
### shell script is mandatory to fulfill POST request
### it calls from my http web server by calling execl(cgi.sh, ...)
### another way (redirect parameters from parent to child's STDIN and to make execl(/usr/bin/php-cgi,...) to read it from pipe) is not working, BASH script it's the only way to pass POST parameters to STDIN of php-cgi

arg=$1              ###### POST - param string
length=$2           ###### CONTENT_LENGTH
method=$3           ###### REQUEST_METHOD identificator
script_path=$4      ###### SCRIPT_FILENAME exmp: "/home/abyss4me/Myhttpd/submit.php"
php_cgi_path=$5     ###### php-cgi path to
  
export GATEWAY_INTERFACE="CGI/1.1"
export SERVER_PROTOCOL="HTTP/1.1"
export SCRIPT_FILENAME=$script_path
export REDIRECT_STATUS=200
export HTTP_ACCEPT="text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
export CONTENT_TYPE="application/x-www-form-urlencoded"
PATH=$PATH:/usr/bin
export PATH
if [ $3 = "POST" ]; then
    export REQUEST_METHOD="POST"
    export CONTENT_LENGTH=$length 
else
    exit  
fi
echo $arg | $5
