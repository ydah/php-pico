--TEST--
array copy-on-write vector 021
--FILE--
<?php
$original = [22, 45, 68];
$copy = $original;
$copy[1] = 158; $copy[] = 159;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
22,45,68:22,158,68,159
