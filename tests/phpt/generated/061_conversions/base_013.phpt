--TEST--
integer base conversion vector 013
--FILE--
<?php
echo dechex(237), ':', hexdec('ed'), ':', decbin(237), ':', bindec('11101101'), ':', decoct(237), ':', octdec('355');
--EXPECT--
ed:237:11101101:237:355:237
