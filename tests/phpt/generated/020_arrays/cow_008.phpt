--TEST--
array copy-on-write vector 008
--FILE--
<?php
$original = [9, 19, 29];
$copy = $original;
$copy[1] = 67; $copy[] = 68;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
9,19,29:9,67,29,68
