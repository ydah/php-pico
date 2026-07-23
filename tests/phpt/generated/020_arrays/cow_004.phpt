--TEST--
array copy-on-write vector 004
--FILE--
<?php
$original = [5, 11, 17];
$copy = $original;
$copy[1] = 39; $copy[] = 40;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
5,11,17:5,39,17,40
