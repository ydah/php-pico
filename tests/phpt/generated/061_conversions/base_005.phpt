--TEST--
integer base conversion vector 005
--FILE--
<?php
echo dechex(101), ':', hexdec('65'), ':', decbin(101), ':', bindec('1100101'), ':', decoct(101), ':', octdec('145');
--EXPECT--
65:101:1100101:101:145:101
