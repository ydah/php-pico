--TEST--
integer base conversion vector 008
--FILE--
<?php
echo dechex(152), ':', hexdec('98'), ':', decbin(152), ':', bindec('10011000'), ':', decoct(152), ':', octdec('230');
--EXPECT--
98:152:10011000:152:230:152
