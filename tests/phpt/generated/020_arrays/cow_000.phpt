--TEST--
array copy-on-write vector 000
--FILE--
<?php
$original = [1, 3, 5];
$copy = $original;
$copy[1] = 11; $copy[] = 12;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
1,3,5:1,11,5,12
