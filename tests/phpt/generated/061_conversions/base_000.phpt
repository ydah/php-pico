--TEST--
integer base conversion vector 000
--FILE--
<?php
echo dechex(16), ':', hexdec('10'), ':', decbin(16), ':', bindec('10000'), ':', decoct(16), ':', octdec('20');
--EXPECT--
10:16:10000:16:20:16
