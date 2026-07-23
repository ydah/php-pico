--TEST--
integer base conversion vector 012
--FILE--
<?php
echo dechex(220), ':', hexdec('dc'), ':', decbin(220), ':', bindec('11011100'), ':', decoct(220), ':', octdec('334');
--EXPECT--
dc:220:11011100:220:334:220
