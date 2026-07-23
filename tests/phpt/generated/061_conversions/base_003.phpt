--TEST--
integer base conversion vector 003
--FILE--
<?php
echo dechex(67), ':', hexdec('43'), ':', decbin(67), ':', bindec('1000011'), ':', decoct(67), ':', octdec('103');
--EXPECT--
43:67:1000011:67:103:67
