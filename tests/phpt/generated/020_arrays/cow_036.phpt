--TEST--
array copy-on-write vector 036
--FILE--
<?php
$original = [37, 75, 113];
$copy = $original;
$copy[1] = 263; $copy[] = 264;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
37,75,113:37,263,113,264
