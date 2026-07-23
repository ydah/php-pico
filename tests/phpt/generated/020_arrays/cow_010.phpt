--TEST--
array copy-on-write vector 010
--FILE--
<?php
$original = [11, 23, 35];
$copy = $original;
$copy[1] = 81; $copy[] = 82;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
11,23,35:11,81,35,82
