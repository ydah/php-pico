--TEST--
array copy-on-write vector 002
--FILE--
<?php
$original = [3, 7, 11];
$copy = $original;
$copy[1] = 25; $copy[] = 26;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
3,7,11:3,25,11,26
