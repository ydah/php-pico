--TEST--
array copy-on-write vector 034
--FILE--
<?php
$original = [35, 71, 107];
$copy = $original;
$copy[1] = 249; $copy[] = 250;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
35,71,107:35,249,107,250
