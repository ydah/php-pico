--TEST--
array copy-on-write vector 022
--FILE--
<?php
$original = [23, 47, 71];
$copy = $original;
$copy[1] = 165; $copy[] = 166;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
23,47,71:23,165,71,166
