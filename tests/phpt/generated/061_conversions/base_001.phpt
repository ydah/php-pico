--TEST--
integer base conversion vector 001
--FILE--
<?php
echo dechex(33), ':', hexdec('21'), ':', decbin(33), ':', bindec('100001'), ':', decoct(33), ':', octdec('41');
--EXPECT--
21:33:100001:33:41:33
