--TEST--
array copy-on-write vector 044
--FILE--
<?php
$original = [45, 91, 137];
$copy = $original;
$copy[1] = 319; $copy[] = 320;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
45,91,137:45,319,137,320
