--TEST--
integer base conversion vector 010
--FILE--
<?php
echo dechex(186), ':', hexdec('ba'), ':', decbin(186), ':', bindec('10111010'), ':', decoct(186), ':', octdec('272');
--EXPECT--
ba:186:10111010:186:272:186
