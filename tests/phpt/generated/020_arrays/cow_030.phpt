--TEST--
array copy-on-write vector 030
--FILE--
<?php
$original = [31, 63, 95];
$copy = $original;
$copy[1] = 221; $copy[] = 222;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
31,63,95:31,221,95,222
