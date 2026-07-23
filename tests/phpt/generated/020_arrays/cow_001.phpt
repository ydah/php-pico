--TEST--
array copy-on-write vector 001
--FILE--
<?php
$original = [2, 5, 8];
$copy = $original;
$copy[1] = 18; $copy[] = 19;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
2,5,8:2,18,8,19
