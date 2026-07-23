--TEST--
array copy-on-write vector 012
--FILE--
<?php
$original = [13, 27, 41];
$copy = $original;
$copy[1] = 95; $copy[] = 96;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
13,27,41:13,95,41,96
