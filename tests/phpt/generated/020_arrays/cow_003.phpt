--TEST--
array copy-on-write vector 003
--FILE--
<?php
$original = [4, 9, 14];
$copy = $original;
$copy[1] = 32; $copy[] = 33;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
4,9,14:4,32,14,33
