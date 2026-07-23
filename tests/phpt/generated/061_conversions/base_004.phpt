--TEST--
integer base conversion vector 004
--FILE--
<?php
echo dechex(84), ':', hexdec('54'), ':', decbin(84), ':', bindec('1010100'), ':', decoct(84), ':', octdec('124');
--EXPECT--
54:84:1010100:84:124:84
