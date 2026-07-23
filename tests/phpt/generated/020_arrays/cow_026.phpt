--TEST--
array copy-on-write vector 026
--FILE--
<?php
$original = [27, 55, 83];
$copy = $original;
$copy[1] = 193; $copy[] = 194;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
27,55,83:27,193,83,194
