--TEST--
array copy-on-write vector 006
--FILE--
<?php
$original = [7, 15, 23];
$copy = $original;
$copy[1] = 53; $copy[] = 54;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
7,15,23:7,53,23,54
