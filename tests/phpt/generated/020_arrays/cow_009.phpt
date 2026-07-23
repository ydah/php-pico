--TEST--
array copy-on-write vector 009
--FILE--
<?php
$original = [10, 21, 32];
$copy = $original;
$copy[1] = 74; $copy[] = 75;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
10,21,32:10,74,32,75
