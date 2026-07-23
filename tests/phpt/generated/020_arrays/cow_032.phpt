--TEST--
array copy-on-write vector 032
--FILE--
<?php
$original = [33, 67, 101];
$copy = $original;
$copy[1] = 235; $copy[] = 236;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
33,67,101:33,235,101,236
