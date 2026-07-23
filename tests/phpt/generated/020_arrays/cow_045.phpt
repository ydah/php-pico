--TEST--
array copy-on-write vector 045
--FILE--
<?php
$original = [46, 93, 140];
$copy = $original;
$copy[1] = 326; $copy[] = 327;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
46,93,140:46,326,140,327
