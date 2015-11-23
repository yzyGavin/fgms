<?php
/*Dummy client virtualizing NOWAIT protocal*/

$msgArray=Array(
	"NOWAIT",
	"CONNECT AF2222 test A340-600HGW 2015-11-22 04:37:00",
	"POSITION AF2222 test 45.724361 5.082576 798.709855 2015-11-22 04:37:01",
	"POSITION AF2222 test 45.724361 -5.082576 798.709855 2015-11-22 04:37:11",
	"POSITION AF2222 test 45.724361 5.082576 798.709855 2015-11-22 04:37:21",
	"POSITION AF2222 test 45.724361 5.082576 -798.709855 2015-11-22 04:37:31",
	"POSITION AF2222 test 45.724361 5.082576 798.709855 2015-11-22 04:37:41",
	"DISCONNECT AF2222 test A340-600HGW 2015-11-22 04:37:50");

$socket=socket_create ( AF_INET , SOCK_STREAM , SOL_TCP );
if ($socket===false)
	die("could not create socket\n");
socket_connect ( $socket , "127.0.0.1",5000);

foreach ($msgArray AS $msg)
{
	$msg.="\0";
	print "SEND: $msg\n";
	socket_send($socket, $msg, strLen($msg), 0);
	print "RECV: ".socket_read ( $socket , 2048 )."\n";
}
socket_close ( $socket );

?>