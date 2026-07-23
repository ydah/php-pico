--TEST--
integer base conversion vector 007
--FILE--
<?php
echo dechex(135), ':', hexdec('87'), ':', decbin(135), ':', bindec('10000111'), ':', decoct(135), ':', octdec('207');
--EXPECT--
87:135:10000111:135:207:135
