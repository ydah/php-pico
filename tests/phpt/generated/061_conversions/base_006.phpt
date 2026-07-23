--TEST--
integer base conversion vector 006
--FILE--
<?php
echo dechex(118), ':', hexdec('76'), ':', decbin(118), ':', bindec('1110110'), ':', decoct(118), ':', octdec('166');
--EXPECT--
76:118:1110110:118:166:118
