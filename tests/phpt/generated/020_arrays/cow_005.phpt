--TEST--
array copy-on-write vector 005
--FILE--
<?php
$original = [6, 13, 20];
$copy = $original;
$copy[1] = 46; $copy[] = 47;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
6,13,20:6,46,20,47
