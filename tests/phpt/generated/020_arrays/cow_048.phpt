--TEST--
array copy-on-write vector 048
--FILE--
<?php
$original = [49, 99, 149];
$copy = $original;
$copy[1] = 347; $copy[] = 348;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
49,99,149:49,347,149,348
