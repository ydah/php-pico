--TEST--
integer base conversion vector 002
--FILE--
<?php
echo dechex(50), ':', hexdec('32'), ':', decbin(50), ':', bindec('110010'), ':', decoct(50), ':', octdec('62');
--EXPECT--
32:50:110010:50:62:50
