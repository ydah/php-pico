--TEST--
integer base conversion vector 009
--FILE--
<?php
echo dechex(169), ':', hexdec('a9'), ':', decbin(169), ':', bindec('10101001'), ':', decoct(169), ':', octdec('251');
--EXPECT--
a9:169:10101001:169:251:169
