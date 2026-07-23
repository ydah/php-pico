--TEST--
integer base conversion vector 011
--FILE--
<?php
echo dechex(203), ':', hexdec('cb'), ':', decbin(203), ':', bindec('11001011'), ':', decoct(203), ':', octdec('313');
--EXPECT--
cb:203:11001011:203:313:203
